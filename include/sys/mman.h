/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/mman
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYS_MMAN_H_
#define _SYS_MMAN_H_

#include <sys/types.h>
#include <phoenix/sysinfo.h>
#include <phoenix/mman.h>


#define MAP_ANON MAP_ANONYMOUS


#ifdef __cplusplus
extern "C" {
#endif


extern void meminfo(meminfo_t *info);


extern int syspageprog(syspageprog_t *prog, int index);


extern void *mmap(void *vaddr, size_t size, int prot, int flags, int fildes, off_t offs);


extern int munmap(void *vaddr, size_t size);


extern int mprotect(void *vaddr, size_t len, int prot);


/* Memory locking. Phoenix has no swap-to-disk, so anonymous pages are never
 * paged out to a backing store: locking is a no-op that trivially succeeds.
 * Provided for portable software that locks sensitive buffers (e.g. OpenSSL's
 * secure heap). */
#define MCL_CURRENT 1
#define MCL_FUTURE  2

extern int mlock(const void *addr, size_t len);


extern int munlock(const void *addr, size_t len);


extern int mlockall(int flags);


extern int munlockall(void);


extern addr_t va2pa(void *va);


#ifdef __cplusplus
}
#endif


#endif
