/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * dlfcn.h — in-process dynamic loading (dlopen/dlsym/dlclose/dlerror)
 *
 * Loads a -fPIC ET_DYN shared object into the running process and resolves its
 * undefined symbols against the host executable's symbol table. Phase A of
 * dynamic-linking support (no kernel change); see the dynamic-linking design
 * doc in the coordination repo.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS RPi4 port
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#ifndef _DLFCN_H_
#define _DLFCN_H_

#ifdef __cplusplus
extern "C" {
#endif

/* mode flags for dlopen(); relocation is always performed eagerly (RTLD_NOW
 * semantics) — RTLD_LAZY is accepted but behaves as RTLD_NOW. */
#define RTLD_LAZY   0x0001
#define RTLD_NOW    0x0002
#define RTLD_LOCAL  0x0000
#define RTLD_GLOBAL 0x0100

extern void *dlopen(const char *filename, int flags);
extern void *dlsym(void *handle, const char *symbol);
extern int dlclose(void *handle);
extern char *dlerror(void);

/* dladdr(): the object and the symbol an address belongs to (a GNU/BSD extension; returns 0
   when addr lies in no loaded object and not in the program). dli_sname/dli_saddr are NULL
   when no sized symbol holds addr, and for a stripped program. */
typedef struct {
	const char *dli_fname; /* path of the object (argv[0] for the program) */
	void *dli_fbase;       /* its load address */
	const char *dli_sname; /* the symbol that holds addr, or NULL */
	void *dli_saddr;       /* that symbol's address, or NULL */
} Dl_info;

extern int dladdr(const void *addr, Dl_info *info);

#ifdef __cplusplus
}
#endif

#endif
