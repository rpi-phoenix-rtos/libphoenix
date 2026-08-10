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

#ifdef __cplusplus
}
#endif

#endif
