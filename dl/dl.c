/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * dl.c — in-process dynamic loader (dlopen/dlsym/dlclose/dlerror)
 *
 * Loads a -fPIC ET_DYN aarch64 shared object entirely from userspace using the
 * primitives Phoenix already provides (open/read/mmap/mprotect):
 *
 *   - text/RO segments : mapped FILE-BACKED at their final protection (R-X / R),
 *                        so .text is never written and the W^X mprotect policy
 *                        (which rejects escalating a mapping's protection) is
 *                        never triggered — no kernel change required.
 *   - data/RW segments : mapped ANONYMOUS R-W then filled from the file; .bss is
 *                        naturally zero. Relocations only ever write here.
 *
 * Handles the relocation types a PIC .so emits on aarch64 (RELATIVE, GLOB_DAT,
 * JUMP_SLOT, ABS64). A loaded object's undefined symbols are resolved against
 * (1) its own defined symbols, then (2) the HOST executable's symbol table,
 * read once from the program image on disk (via argv_progname). The host must
 * therefore be linked unstripped (its .symtab holds every libc/host symbol at
 * its final, no-ASLR address).
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS RPi4 port
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <dlfcn.h>

/* --- minimal ELF64 (aarch64) --- */
typedef struct {
	unsigned char e_ident[16];
	uint16_t e_type, e_machine;
	uint32_t e_version;
	uint64_t e_entry, e_phoff, e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	uint32_t p_type, p_flags;
	uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

typedef struct {
	uint32_t sh_name, sh_type;
	uint64_t sh_flags, sh_addr, sh_offset, sh_size;
	uint32_t sh_link, sh_info;
	uint64_t sh_addralign, sh_entsize;
} Elf64_Shdr;

typedef struct {
	int64_t d_tag;
	uint64_t d_val;
} Elf64_Dyn;

typedef struct {
	uint32_t st_name;
	unsigned char st_info, st_other;
	uint16_t st_shndx;
	uint64_t st_value, st_size;
} Elf64_Sym;

typedef struct {
	uint64_t r_offset, r_info, r_addend;
} Elf64_Rela;

#define ET_DYN       3
#define EM_AARCH64   183
#define PT_LOAD      1
#define PT_DYNAMIC   2
#define PF_X         0x1
#define PF_W         0x2
#define SHT_SYMTAB   2
#define SHN_UNDEF    0

#define DT_NULL         0
#define DT_HASH         4
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_INIT_ARRAY   25
#define DT_INIT_ARRAYSZ 27
#define DT_PLTRELSZ     2
#define DT_JMPREL       23

#define ELF64_R_SYM(i)  ((uint32_t)((i) >> 32))
#define ELF64_R_TYPE(i) ((uint32_t)((i) & 0xffffffffU))

#define R_AARCH64_ABS64     257
#define R_AARCH64_GLOB_DAT  1025
#define R_AARCH64_JUMP_SLOT 1026
#define R_AARCH64_RELATIVE  1027

#define PAGE_SZ 0x1000UL
#define PAGE_DOWN(x) ((x) & ~(PAGE_SZ - 1UL))
#define PAGE_UP(x)   PAGE_DOWN((x) + PAGE_SZ - 1UL)

typedef struct dl_obj {
	uintptr_t bias;
	uintptr_t map_base;
	size_t map_span;
	int fd;
	Elf64_Sym *symtab;   /* mapped .dynsym */
	const char *strtab;  /* mapped .dynstr */
	uint32_t symcount;   /* DT_HASH nchain */
	struct dl_obj *next; /* loaded-object list */
} dl_obj_t;

extern const char *argv_progname; /* argv[0], set by crt0-common.c */

static dl_obj_t *dl_loaded;
static char dl_errbuf[160];
static int dl_haveErr;

/* the host executable's symbol table, mapped + cached on first use */
static struct {
	int tried;
	const unsigned char *base; /* mmap of the host ELF file (read-only) */
	size_t size;
	const Elf64_Sym *sym;
	uint32_t nsym;
	const char *str;
} dl_host;

static void dl_seterr(const char *msg, const char *arg)
{
	if (arg != NULL) {
		(void)snprintf(dl_errbuf, sizeof(dl_errbuf), "%s: %s", msg, arg);
	}
	else {
		(void)snprintf(dl_errbuf, sizeof(dl_errbuf), "%s", msg);
	}
	dl_haveErr = 1;
}

/* map + parse the host executable's .symtab once (needs an unstripped host) */
static void dl_hostInit(void)
{
	int fd;
	off_t sz;
	const Elf64_Ehdr *eh;
	const Elf64_Shdr *sh;
	int i;

	dl_host.tried = 1;
	if (argv_progname == NULL) {
		return;
	}
	fd = open(argv_progname, O_RDONLY);
	if (fd < 0) {
		return;
	}
	sz = lseek(fd, 0, SEEK_END);
	(void)lseek(fd, 0, SEEK_SET);
	if (sz <= 0) {
		close(fd);
		return;
	}
	/* map the host image file read-only (file-backed, no MAP_ANONYMOUS) */
	dl_host.base = mmap(NULL, (size_t)sz, PROT_READ, 0, fd, 0);
	dl_host.size = (size_t)sz;
	close(fd);
	if (dl_host.base == (const unsigned char *)MAP_FAILED) {
		dl_host.base = NULL;
		return;
	}
	eh = (const Elf64_Ehdr *)dl_host.base;
	if (memcmp(eh->e_ident, "\177ELF", 4) != 0) {
		dl_host.base = NULL;
		return;
	}
	sh = (const Elf64_Shdr *)(dl_host.base + eh->e_shoff);
	for (i = 0; i < eh->e_shnum; i++) {
		if (sh[i].sh_type == SHT_SYMTAB) {
			dl_host.sym = (const Elf64_Sym *)(dl_host.base + sh[i].sh_offset);
			dl_host.nsym = (uint32_t)(sh[i].sh_size / sizeof(Elf64_Sym));
			dl_host.str = (const char *)(dl_host.base + sh[sh[i].sh_link].sh_offset);
			break;
		}
	}
}

static void *dl_hostLookup(const char *name)
{
	uint32_t i;

	if (!dl_host.tried) {
		dl_hostInit();
	}
	if (dl_host.sym == NULL) {
		return NULL;
	}
	for (i = 0; i < dl_host.nsym; i++) {
		if (dl_host.sym[i].st_shndx != SHN_UNDEF && dl_host.sym[i].st_value != 0 &&
			strcmp(dl_host.str + dl_host.sym[i].st_name, name) == 0) {
			return (void *)(uintptr_t)dl_host.sym[i].st_value;
		}
	}
	return NULL;
}

/* resolve an object symbol index to a runtime address */
static void *dl_resolve(dl_obj_t *o, uint32_t symidx)
{
	Elf64_Sym *s = &o->symtab[symidx];
	const char *name = o->strtab + s->st_name;

	if (s->st_shndx != SHN_UNDEF) {
		return (void *)(o->bias + s->st_value);
	}
	return dl_hostLookup(name);
}

void *dlopen(const char *filename, int flags)
{
	int fd, i;
	off_t fsize;
	unsigned char *fbuf = NULL;
	Elf64_Ehdr *eh;
	Elf64_Phdr *ph;
	dl_obj_t *o = NULL;
	uint64_t vmin = ~0ULL, vmax = 0, dyn_vaddr = 0;
	const Elf64_Dyn *dyn;
	uint64_t rela = 0, relasz = 0, jmprel = 0, pltrelsz = 0, hashv = 0;
	uintptr_t bias;

	(void)flags; /* relocation is always eager */

	if (filename == NULL) {
		dl_seterr("dlopen: NULL filename", NULL);
		return NULL;
	}
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		dl_seterr("dlopen: cannot open", filename);
		return NULL;
	}
	fsize = lseek(fd, 0, SEEK_END);
	(void)lseek(fd, 0, SEEK_SET);
	if (fsize <= 0) {
		dl_seterr("dlopen: empty file", filename);
		close(fd);
		return NULL;
	}
	fbuf = malloc((size_t)fsize);
	if (fbuf == NULL || read(fd, fbuf, (size_t)fsize) != (ssize_t)fsize) {
		dl_seterr("dlopen: read failed", filename);
		goto fail;
	}
	eh = (Elf64_Ehdr *)fbuf;
	if (memcmp(eh->e_ident, "\177ELF", 4) != 0 || eh->e_ident[4] != 2) {
		dl_seterr("dlopen: not ELF64", filename);
		goto fail;
	}
	if (eh->e_type != ET_DYN || eh->e_machine != EM_AARCH64) {
		dl_seterr("dlopen: not aarch64 ET_DYN", filename);
		goto fail;
	}

	ph = (Elf64_Phdr *)(fbuf + eh->e_phoff);
	for (i = 0; i < eh->e_phnum; i++) {
		if (ph[i].p_type == PT_LOAD) {
			if (ph[i].p_vaddr < vmin) {
				vmin = ph[i].p_vaddr;
			}
			if (ph[i].p_vaddr + ph[i].p_memsz > vmax) {
				vmax = ph[i].p_vaddr + ph[i].p_memsz;
			}
		}
		else if (ph[i].p_type == PT_DYNAMIC) {
			dyn_vaddr = ph[i].p_vaddr;
		}
	}
	if (vmin == ~0ULL || dyn_vaddr == 0) {
		dl_seterr("dlopen: no PT_LOAD/PT_DYNAMIC", filename);
		goto fail;
	}

	o = calloc(1, sizeof(*o));
	if (o == NULL) {
		dl_seterr("dlopen: out of memory", NULL);
		goto fail;
	}
	o->fd = fd;
	o->map_span = PAGE_UP(vmax) - PAGE_DOWN(vmin);
	o->map_base = (uintptr_t)mmap(NULL, o->map_span, PROT_READ, MAP_ANONYMOUS, -1, 0);
	if (o->map_base == (uintptr_t)MAP_FAILED) {
		dl_seterr("dlopen: reserve failed", filename);
		goto fail;
	}
	bias = o->map_base - (uintptr_t)PAGE_DOWN(vmin);
	o->bias = bias;

	for (i = 0; i < eh->e_phnum; i++) {
		uint64_t segstart, segoff, mapend;
		int prot;
		void *want, *got;

		if (ph[i].p_type != PT_LOAD) {
			continue;
		}
		segstart = PAGE_DOWN(ph[i].p_vaddr);
		segoff = PAGE_DOWN(ph[i].p_offset);
		want = (void *)(bias + segstart);

		if ((ph[i].p_flags & PF_W) != 0) {
			mapend = PAGE_UP(ph[i].p_vaddr + ph[i].p_memsz);
			got = mmap(want, (size_t)(mapend - segstart), PROT_READ | PROT_WRITE,
				MAP_FIXED | MAP_ANONYMOUS, -1, 0);
			if (got != want) {
				dl_seterr("dlopen: data mmap failed", filename);
				goto fail;
			}
			memcpy((void *)(bias + ph[i].p_vaddr), fbuf + ph[i].p_offset,
				(size_t)ph[i].p_filesz);
		}
		else {
			mapend = PAGE_UP(ph[i].p_vaddr + ph[i].p_filesz);
			prot = PROT_READ | (((ph[i].p_flags & PF_X) != 0) ? PROT_EXEC : 0);
			got = mmap(want, (size_t)(mapend - segstart), prot, MAP_FIXED, fd,
				(off_t)segoff);
			if (got != want) {
				dl_seterr("dlopen: text mmap failed", filename);
				goto fail;
			}
		}
	}

	dyn = (const Elf64_Dyn *)(bias + dyn_vaddr);
	for (; dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
			case DT_SYMTAB:   o->symtab = (Elf64_Sym *)(bias + dyn->d_val); break;
			case DT_STRTAB:   o->strtab = (const char *)(bias + dyn->d_val); break;
			case DT_HASH:     hashv = bias + dyn->d_val; break;
			case DT_RELA:     rela = bias + dyn->d_val; break;
			case DT_RELASZ:   relasz = dyn->d_val; break;
			case DT_JMPREL:   jmprel = bias + dyn->d_val; break;
			case DT_PLTRELSZ: pltrelsz = dyn->d_val; break;
			default: break;
		}
	}
	if (o->symtab == NULL || o->strtab == NULL) {
		dl_seterr("dlopen: no dynsym/dynstr", filename);
		goto fail;
	}
	o->symcount = (hashv != 0) ? ((uint32_t *)hashv)[1] : 0;

	for (int pass = 0; pass < 2; pass++) {
		uint64_t rbase = (pass == 0) ? rela : jmprel;
		uint64_t rsz = (pass == 0) ? relasz : pltrelsz;
		uint64_t off;

		for (off = 0; off + sizeof(Elf64_Rela) <= rsz; off += sizeof(Elf64_Rela)) {
			Elf64_Rela *r = (Elf64_Rela *)(rbase + off);
			uint32_t type = ELF64_R_TYPE(r->r_info);
			uint32_t sym = ELF64_R_SYM(r->r_info);
			uint64_t *where = (uint64_t *)(bias + r->r_offset);
			void *val;

			switch (type) {
				case R_AARCH64_RELATIVE:
					*where = (uint64_t)bias + r->r_addend;
					break;
				case R_AARCH64_GLOB_DAT:
				case R_AARCH64_JUMP_SLOT:
				case R_AARCH64_ABS64:
					val = dl_resolve(o, sym);
					if (val == NULL) {
						dl_seterr("dlopen: unresolved symbol",
							o->strtab + o->symtab[sym].st_name);
						goto fail;
					}
					*where = (uint64_t)val + r->r_addend;
					break;
				default:
					dl_seterr("dlopen: unsupported reloc type", filename);
					goto fail;
			}
		}
	}

	/* DT_INIT_ARRAY */
	dyn = (const Elf64_Dyn *)(bias + dyn_vaddr);
	{
		uint64_t ia = 0, iasz = 0, k;
		for (; dyn->d_tag != DT_NULL; dyn++) {
			if (dyn->d_tag == DT_INIT_ARRAY) {
				ia = bias + dyn->d_val;
			}
			else if (dyn->d_tag == DT_INIT_ARRAYSZ) {
				iasz = dyn->d_val;
			}
		}
		if (ia != 0) {
			void (**fns)(void) = (void (**)(void))ia;
			for (k = 0; k < iasz / sizeof(void *); k++) {
				if (fns[k] != NULL) {
					fns[k]();
				}
			}
		}
	}

	free(fbuf);
	o->next = dl_loaded;
	dl_loaded = o;
	dl_haveErr = 0;
	return o;

fail:
	free(fbuf);
	if (o != NULL) {
		if (o->map_base != 0 && o->map_base != (uintptr_t)MAP_FAILED) {
			(void)munmap((void *)o->map_base, o->map_span);
		}
		free(o);
	}
	close(fd);
	return NULL;
}

void *dlsym(void *handle, const char *symbol)
{
	dl_obj_t *o = (dl_obj_t *)handle;
	uint32_t i;

	if (o == NULL || symbol == NULL) {
		dl_seterr("dlsym: bad argument", NULL);
		return NULL;
	}
	for (i = 0; i < o->symcount; i++) {
		if (o->symtab[i].st_shndx != SHN_UNDEF &&
			strcmp(o->strtab + o->symtab[i].st_name, symbol) == 0) {
			return (void *)(o->bias + o->symtab[i].st_value);
		}
	}
	dl_seterr("dlsym: symbol not found", symbol);
	return NULL;
}

int dlclose(void *handle)
{
	dl_obj_t *o = (dl_obj_t *)handle;
	dl_obj_t **pp;

	if (o == NULL) {
		return -1;
	}
	for (pp = &dl_loaded; *pp != NULL; pp = &(*pp)->next) {
		if (*pp == o) {
			*pp = o->next;
			break;
		}
	}
	if (o->map_base != 0 && o->map_base != (uintptr_t)MAP_FAILED) {
		(void)munmap((void *)o->map_base, o->map_span);
	}
	close(o->fd);
	free(o);
	return 0;
}

char *dlerror(void)
{
	if (!dl_haveErr) {
		return NULL;
	}
	dl_haveErr = 0;
	return dl_errbuf;
}
