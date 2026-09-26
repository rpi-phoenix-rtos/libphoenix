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


/* Memory export (Phoenix-specific).
 *
 * memExport() publishes the page-aligned range [vaddr, vaddr + size) of the caller's
 * MAP_ANONYMOUS | MAP_CONTIGUOUS mapping under *oid, whose port must be one the caller owns.
 * Any process holding a descriptor that carries that oid (opened through the owner's
 * namespace, or received over SCM_RIGHTS) can then mmap() the same physical pages.
 *
 * The memory type of the caller's mapping (cached, or MAP_UNCACHED) is fixed for the export:
 * mmap() with any other type, or past the end, fails with EINVAL. The pages stay allocated
 * until the export is withdrawn -- memUnexport(), or release of the port -- and the last
 * mapping is gone. Mappings of exported memory are shared, not copied, across fork().
 *
 * Both return 0 or a negative error code (EPERM: port not owned by the caller, EINVAL: range
 * not exportable, EEXIST: oid in use, ENOENT: nothing exported under oid). */
extern int memExport(const oid_t *oid, void *vaddr, size_t size);


extern int memUnexport(const oid_t *oid);


#ifdef __cplusplus
}
#endif


#endif
