/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * dl.c — in-process dynamic loader (dlopen/dlsym/dlclose/dlerror/dladdr)
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
 * (1) its own defined symbols, then (2) the symbols the HOST executable exports.
 * An undefined weak symbol that neither defines resolves to 0.
 *
 * What the host exports, in order of preference:
 *
 *   - its dynamic symbol table, when it has one. A static Phoenix program gets
 *     one by linking with
 *         -Wl,--no-dynamic-linker -Wl,--dynamic-list=<file>   (the listed symbols)
 *     or  -Wl,--no-dynamic-linker -Wl,--export-dynamic        (every global symbol)
 *     Both flags are needed: without --no-dynamic-linker, ld creates no dynamic
 *     sections for a program that links no shared library. The table is part of
 *     the loaded image (found through _DYNAMIC, looked up through its DT_HASH), so
 *     it survives strip, and ld keeps every exported symbol under --gc-sections.
 *     It is the whole export list: a symbol it does not contain is not exported,
 *     even when the program file still has a .symtab.
 *   - otherwise, the .symtab of the program file (read from argv[0]), which
 *     needs an unstripped program and exports every symbol it has.
 *
 * Not supported: thread-local storage in a loaded object (no dynamic TLS),
 * DT_GNU_HASH-only objects (link them with -Wl,--hash-style=sysv or =both),
 * symbol preemption by the host of an object's own definitions, DT_NEEDED.
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
#define PT_TLS       7
#define PF_X         0x1
#define PF_W         0x2
#define SHT_SYMTAB   2
#define SHN_UNDEF    0
#define STB_LOCAL    0
#define STB_WEAK     2
#define STT_OBJECT   1
#define STT_FUNC     2
#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xfU)

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

#define R_AARCH64_NONE         0
#define R_AARCH64_ABS64        257
#define R_AARCH64_GLOB_DAT     1025
#define R_AARCH64_JUMP_SLOT    1026
#define R_AARCH64_RELATIVE     1027
#define R_AARCH64_TLS_DTPMOD64 1028
#define R_AARCH64_TLS_DTPREL64 1029
#define R_AARCH64_TLS_TPREL64  1030
#define R_AARCH64_TLSDESC      1031

#define PAGE_SZ 0x1000UL
#define PAGE_DOWN(x) ((x) & ~(PAGE_SZ - 1UL))
#define PAGE_UP(x)   PAGE_DOWN((x) + PAGE_SZ - 1UL)

/* a symbol table and its SysV hash table (DT_HASH), or no hash table: linear scan */
typedef struct {
	const Elf64_Sym *sym;
	const char *str;
	const uint32_t *hash;
	uint32_t nsym;
} dl_symtab_t;

typedef struct dl_obj {
	uintptr_t bias;
	uintptr_t map_base;
	size_t map_span;
	int fd;
	dl_symtab_t dyn;     /* mapped .dynsym, .dynstr and DT_HASH */
	char *path;          /* the name dlopen() was given, for dladdr() */
	struct dl_obj *next; /* loaded-object list */
} dl_obj_t;

extern const char *argv_progname; /* argv[0], set by crt0-common.c */

/* the program's own dynamic section, defined by ld when the program was linked with
 * dynamic sections (see the header comment), NULL otherwise */
extern const Elf64_Dyn _DYNAMIC[] __attribute__((weak));

static dl_obj_t *dl_loaded;
static char dl_errbuf[384];
static int dl_haveErr;

/* Pseudo-handle returned by dlopen(NULL): a handle to the main program itself.
 * dlsym() on it resolves against the host's exported symbols (the same table used
 * to satisfy a loaded object's undefined symbols). This is the POSIX "global symbol
 * object" behaviour that e.g. Python's ctypes (PyDLL(None)) needs. */
static dl_obj_t dl_mainProg;

/* the symbols the host exports, chosen once on first use */
static struct {
	int tried;
	int fromDynsym; /* 1: the in-memory dynamic symbol table, 0: the file's .symtab */
	dl_symtab_t tab;
} dl_host;

/* the program file mapped read-only: the .symtab fallback, and dladdr() */
static struct {
	int tried;
	const unsigned char *base;
	size_t size;
	dl_symtab_t symtab; /* .symtab, unhashed; sym == NULL for a stripped program */
} dl_prog;


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


/* the SysV ELF hash (DT_HASH) */
static uint32_t dl_elfHash(const char *name)
{
	uint32_t h = 0, g;
	const unsigned char *p = (const unsigned char *)name;

	while (*p != '\0') {
		h = (h << 4) + *p++;
		g = h & 0xf0000000U;
		if (g != 0) {
			h ^= g >> 24;
		}
		h &= ~g;
	}
	return h;
}


/* A hashed table is a dynamic symbol table: a symbol there is a definition other objects
 * may bind to when it is defined and not local. The unhashed .symtab fallback keeps its
 * original rule: any defined symbol with a non-zero value. */
static int dl_symUsable(const dl_symtab_t *t, const Elf64_Sym *s)
{
	if (s->st_shndx == SHN_UNDEF) {
		return 0;
	}
	if (t->hash != NULL) {
		return (ELF64_ST_BIND(s->st_info) != STB_LOCAL) ? 1 : 0;
	}
	return (s->st_value != 0) ? 1 : 0;
}


static const Elf64_Sym *dl_tabLookup(const dl_symtab_t *t, const char *name)
{
	uint32_t i, nbucket;
	const uint32_t *bucket, *chain;

	if (t->sym == NULL) {
		return NULL;
	}
	if (t->hash != NULL) {
		nbucket = t->hash[0];
		if (nbucket == 0) {
			return NULL;
		}
		bucket = &t->hash[2];
		chain = &bucket[nbucket];
		/* chain index 0 (STN_UNDEF) ends a chain; the bound guards a corrupt table */
		for (i = bucket[dl_elfHash(name) % nbucket]; (i != 0) && (i < t->nsym); i = chain[i]) {
			if ((dl_symUsable(t, &t->sym[i]) != 0) && (strcmp(t->str + t->sym[i].st_name, name) == 0)) {
				return &t->sym[i];
			}
		}
		return NULL;
	}
	for (i = 0; i < t->nsym; i++) {
		if ((dl_symUsable(t, &t->sym[i]) != 0) && (strcmp(t->str + t->sym[i].st_name, name) == 0)) {
			return &t->sym[i];
		}
	}
	return NULL;
}


/* open the program file: argv[0], or for a bare name (e.g. "python3" launched via PATH)
 * the PATH entry the shell found it in */
static int dl_progOpen(void)
{
	int fd;
	const char *path, *p, *colon;
	char cand[512];
	size_t dlen;

	if (argv_progname == NULL) {
		return -1;
	}
	fd = open(argv_progname, O_RDONLY);
	if ((fd >= 0) || (strchr(argv_progname, '/') != NULL)) {
		return fd;
	}
	path = getenv("PATH");
	if (path == NULL) {
		return -1;
	}
	p = path;
	while ((*p != '\0') && (fd < 0)) {
		colon = strchr(p, ':');
		dlen = (colon != NULL) ? (size_t)(colon - p) : strlen(p);
		if ((dlen > 0) && ((dlen + 1 + strlen(argv_progname) + 1) <= sizeof(cand))) {
			memcpy(cand, p, dlen);
			cand[dlen] = '/';
			strcpy(cand + dlen + 1, argv_progname);
			fd = open(cand, O_RDONLY);
		}
		p = (colon != NULL) ? (colon + 1) : (p + dlen);
	}
	return fd;
}


/* map the program file and find its .symtab, once */
static void dl_progInit(void)
{
	int fd, i;
	off_t sz;
	const Elf64_Ehdr *eh;
	const Elf64_Shdr *sh;
	const void *base;

	if (dl_prog.tried != 0) {
		return;
	}
	dl_prog.tried = 1;
	fd = dl_progOpen();
	if (fd < 0) {
		return;
	}
	sz = lseek(fd, 0, SEEK_END);
	(void)lseek(fd, 0, SEEK_SET);
	if (sz < (off_t)sizeof(Elf64_Ehdr)) {
		close(fd);
		return;
	}
	base = mmap(NULL, (size_t)sz, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (base == MAP_FAILED) {
		return;
	}
	eh = (const Elf64_Ehdr *)base;
	if (memcmp(eh->e_ident, "\177ELF", 4) != 0) {
		(void)munmap((void *)base, (size_t)sz);
		return;
	}
	dl_prog.base = (const unsigned char *)base;
	dl_prog.size = (size_t)sz;
	if ((eh->e_shoff == 0) || ((eh->e_shoff + (uint64_t)eh->e_shnum * sizeof(Elf64_Shdr)) > dl_prog.size)) {
		return;
	}
	sh = (const Elf64_Shdr *)(dl_prog.base + eh->e_shoff);
	for (i = 0; i < eh->e_shnum; i++) {
		if ((sh[i].sh_type == SHT_SYMTAB) && (sh[i].sh_link < eh->e_shnum)) {
			dl_prog.symtab.sym = (const Elf64_Sym *)(dl_prog.base + sh[i].sh_offset);
			dl_prog.symtab.nsym = (uint32_t)(sh[i].sh_size / sizeof(Elf64_Sym));
			dl_prog.symtab.str = (const char *)(dl_prog.base + sh[sh[i].sh_link].sh_offset);
			break;
		}
	}
}


/* Choose the host's export table, once: the in-memory dynamic symbol table when the
 * program has a hashed one, else the program file's .symtab (unstripped program). */
static void dl_hostInit(void)
{
	const Elf64_Dyn *d;
	dl_symtab_t t = { 0 };

	if (dl_host.tried != 0) {
		return;
	}
	dl_host.tried = 1;

	if (_DYNAMIC != NULL) {
		/* the program is ET_EXEC: d_ptr values are its runtime addresses */
		for (d = _DYNAMIC; d->d_tag != DT_NULL; d++) {
			switch (d->d_tag) {
				case DT_SYMTAB: t.sym = (const Elf64_Sym *)(uintptr_t)d->d_val; break;
				case DT_STRTAB: t.str = (const char *)(uintptr_t)d->d_val; break;
				case DT_HASH: t.hash = (const uint32_t *)(uintptr_t)d->d_val; break;
				default: break;
			}
		}
		if ((t.sym != NULL) && (t.str != NULL) && (t.hash != NULL)) {
			t.nsym = t.hash[1]; /* nchain == the number of symbols */
			dl_host.tab = t;
			dl_host.fromDynsym = 1;
		}
	}

	if (dl_host.fromDynsym == 0) {
		dl_progInit();
		dl_host.tab = dl_prog.symtab;
	}
}


static const Elf64_Sym *dl_hostLookup(const char *name)
{
	dl_hostInit();
	return dl_tabLookup(&dl_host.tab, name);
}


/* Resolve symbol symidx of object o to its runtime address: the object's own definition,
 * else the host's export. An undefined weak symbol nobody defines is 0. */
static int dl_resolve(dl_obj_t *o, uint32_t symidx, uint64_t *val)
{
	const Elf64_Sym *s, *hs;

	if (symidx == 0) {
		*val = 0; /* no symbol: the relocation is the addend alone */
		return 0;
	}
	s = &o->dyn.sym[symidx];
	if (s->st_shndx != SHN_UNDEF) {
		*val = (uint64_t)o->bias + s->st_value;
		return 0;
	}
	hs = dl_hostLookup(o->dyn.str + s->st_name);
	if (hs != NULL) {
		*val = hs->st_value;
		return 0;
	}
	if (ELF64_ST_BIND(s->st_info) == STB_WEAK) {
		*val = 0;
		return 0;
	}
	return -1;
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
	uint64_t ia = 0, iasz = 0, k;
	uintptr_t bias;

	(void)flags; /* relocation is always eager */

	if (filename == NULL) {
		/* handle to the main program: dlsym resolves against the host's exports */
		dl_hostInit();
		dl_haveErr = 0;
		return &dl_mainProg;
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
		else if (ph[i].p_type == PT_TLS) {
			dl_seterr("dlopen: thread-local storage in a loaded object is not supported", filename);
			goto fail;
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
	o->path = strdup(filename);
	if (o->path == NULL) {
		dl_seterr("dlopen: out of memory", NULL);
		goto fail;
	}
	o->map_span = PAGE_UP(vmax) - PAGE_DOWN(vmin);
	o->map_base = (uintptr_t)mmap(NULL, o->map_span, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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
				MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
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
			got = mmap(want, (size_t)(mapend - segstart), prot, MAP_PRIVATE | MAP_FIXED, fd,
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
			case DT_SYMTAB:   o->dyn.sym = (const Elf64_Sym *)(bias + dyn->d_val); break;
			case DT_STRTAB:   o->dyn.str = (const char *)(bias + dyn->d_val); break;
			case DT_HASH:     hashv = bias + dyn->d_val; break;
			case DT_RELA:     rela = bias + dyn->d_val; break;
			case DT_RELASZ:   relasz = dyn->d_val; break;
			case DT_JMPREL:   jmprel = bias + dyn->d_val; break;
			case DT_PLTRELSZ: pltrelsz = dyn->d_val; break;
			case DT_INIT_ARRAY: ia = bias + dyn->d_val; break;
			case DT_INIT_ARRAYSZ: iasz = dyn->d_val; break;
			default: break;
		}
	}
	if (o->dyn.sym == NULL || o->dyn.str == NULL) {
		dl_seterr("dlopen: no dynsym/dynstr", filename);
		goto fail;
	}
	if (hashv == 0) {
		/* without DT_HASH there is no symbol count, so dlsym() could find nothing */
		dl_seterr("dlopen: no DT_HASH (link the object with -Wl,--hash-style=sysv)", filename);
		goto fail;
	}
	o->dyn.hash = (const uint32_t *)hashv;
	o->dyn.nsym = o->dyn.hash[1];

	for (int pass = 0; pass < 2; pass++) {
		uint64_t rbase = (pass == 0) ? rela : jmprel;
		uint64_t rsz = (pass == 0) ? relasz : pltrelsz;
		uint64_t off;

		for (off = 0; off + sizeof(Elf64_Rela) <= rsz; off += sizeof(Elf64_Rela)) {
			Elf64_Rela *r = (Elf64_Rela *)(rbase + off);
			uint32_t type = ELF64_R_TYPE(r->r_info);
			uint32_t sym = ELF64_R_SYM(r->r_info);
			uint64_t *where = (uint64_t *)(bias + r->r_offset);
			uint64_t val;

			switch (type) {
				case R_AARCH64_NONE:
					break;
				case R_AARCH64_RELATIVE:
					*where = (uint64_t)bias + r->r_addend;
					break;
				case R_AARCH64_GLOB_DAT:
				case R_AARCH64_JUMP_SLOT:
				case R_AARCH64_ABS64:
					if ((sym >= o->dyn.nsym) || (dl_resolve(o, sym, &val) < 0)) {
						dl_seterr("dlopen: unresolved symbol",
							(sym < o->dyn.nsym) ? (o->dyn.str + o->dyn.sym[sym].st_name) : "?");
						goto fail;
					}
					*where = val + r->r_addend;
					break;
				case R_AARCH64_TLS_DTPMOD64:
				case R_AARCH64_TLS_DTPREL64:
				case R_AARCH64_TLS_TPREL64:
				case R_AARCH64_TLSDESC:
					dl_seterr("dlopen: thread-local storage in a loaded object is not supported", filename);
					goto fail;
				default:
					dl_seterr("dlopen: unsupported reloc type", filename);
					goto fail;
			}
		}
	}

	/* DT_INIT_ARRAY */
	if (ia != 0) {
		void (**fns)(void) = (void (**)(void))ia;
		for (k = 0; k < iasz / sizeof(void *); k++) {
			if (fns[k] != NULL) {
				fns[k]();
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
		free(o->path);
		free(o);
	}
	close(fd);
	return NULL;
}

void *dlsym(void *handle, const char *symbol)
{
	dl_obj_t *o = (dl_obj_t *)handle;
	const Elf64_Sym *s;

	if (o == NULL || symbol == NULL) {
		dl_seterr("dlsym: bad argument", NULL);
		return NULL;
	}
	if (o == &dl_mainProg) {
		s = dl_hostLookup(symbol);
		if (s == NULL) {
			dl_seterr("dlsym: symbol not found", symbol);
			return NULL;
		}
		return (void *)(uintptr_t)s->st_value;
	}
	s = dl_tabLookup(&o->dyn, symbol);
	if (s == NULL) {
		dl_seterr("dlsym: symbol not found", symbol);
		return NULL;
	}
	return (void *)(o->bias + s->st_value);
}

int dlclose(void *handle)
{
	dl_obj_t *o = (dl_obj_t *)handle;
	dl_obj_t **pp;

	if (o == NULL) {
		return -1;
	}
	if (o == &dl_mainProg) {
		return 0; /* the main-program handle is never unmapped */
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
	free(o->path);
	free(o);
	return 0;
}


/* the symbol whose [value, value + size) holds addr (a sized function or object); base is
   added to every value (the load bias of an object, 0 for the host) */
static const Elf64_Sym *dl_symAt(const Elf64_Sym *sym, uint32_t nsym, uintptr_t base, uintptr_t addr)
{
	uint32_t i;
	unsigned int type;
	uintptr_t v;

	if (sym == NULL) {
		return NULL;
	}
	for (i = 0; i < nsym; i++) {
		type = ELF64_ST_TYPE(sym[i].st_info);
		if ((sym[i].st_shndx == SHN_UNDEF) || (sym[i].st_value == 0) || ((type != STT_FUNC) && (type != STT_OBJECT))) {
			continue;
		}
		v = base + (uintptr_t)sym[i].st_value;
		if ((addr >= v) && ((addr - v) < ((sym[i].st_size != 0) ? sym[i].st_size : 1))) {
			return &sym[i];
		}
	}
	return NULL;
}


/* the host executable's loaded range, from its PT_LOAD headers (no ASLR: file vaddrs) */
static int dl_hostRange(uintptr_t *lo, uintptr_t *hi)
{
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)dl_prog.base;
	const Elf64_Phdr *ph;
	uint64_t vmin = ~0ULL, vmax = 0;
	int i;

	if ((eh == NULL) || (eh->e_phoff == 0) || (eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(Elf64_Phdr) > dl_prog.size)) {
		return -1;
	}
	ph = (const Elf64_Phdr *)(dl_prog.base + eh->e_phoff);
	for (i = 0; i < eh->e_phnum; i++) {
		if (ph[i].p_type == PT_LOAD) {
			if (ph[i].p_vaddr < vmin) {
				vmin = ph[i].p_vaddr;
			}
			if (ph[i].p_vaddr + ph[i].p_memsz > vmax) {
				vmax = ph[i].p_vaddr + ph[i].p_memsz;
			}
		}
	}
	if (vmin >= vmax) {
		return -1;
	}
	*lo = (uintptr_t)vmin;
	*hi = (uintptr_t)vmax;
	return 0;
}


int dladdr(const void *addr, Dl_info *info)
{
	uintptr_t a = (uintptr_t)addr, lo, hi;
	const dl_obj_t *o;
	const Elf64_Sym *s;
	const dl_symtab_t *names;

	if (info == NULL) {
		return 0;
	}
	memset(info, 0, sizeof(*info));

	/* a loaded object: its name and base, and the .dynsym symbol that holds addr */
	for (o = dl_loaded; o != NULL; o = o->next) {
		if ((a >= o->map_base) && ((a - o->map_base) < o->map_span)) {
			info->dli_fname = o->path;
			info->dli_fbase = (void *)o->map_base;
			s = dl_symAt(o->dyn.sym, o->dyn.nsym, o->bias, a);
			if (s != NULL) {
				info->dli_sname = o->dyn.str + s->st_name;
				info->dli_saddr = (void *)(o->bias + (uintptr_t)s->st_value);
			}
			return 1;
		}
	}

	/* the host executable: named from its .symtab when the program is unstripped, else
	   from its export table (only exported symbols then have names) */
	dl_progInit();
	dl_hostInit();
	if ((dl_prog.base == NULL) || (dl_hostRange(&lo, &hi) < 0) || (a < lo) || (a >= hi)) {
		return 0;
	}
	info->dli_fname = argv_progname;
	info->dli_fbase = (void *)lo;
	names = (dl_prog.symtab.sym != NULL) ? &dl_prog.symtab : &dl_host.tab;
	s = dl_symAt(names->sym, names->nsym, 0, a);
	if (s != NULL) {
		info->dli_sname = names->str + s->st_name;
		info->dli_saddr = (void *)(uintptr_t)s->st_value;
	}
	return 1;
}

char *dlerror(void)
{
	if (!dl_haveErr) {
		return NULL;
	}
	dl_haveErr = 0;
	return dl_errbuf;
}
