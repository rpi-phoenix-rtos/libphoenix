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


/* Memory advice.
 *
 * The kernel has no way to drop the pages of a range: anonymous memory stays
 * backed from mmap() to munmap(). Advice that only describes the expected
 * access pattern is therefore accepted and ignored, and advice whose effect a
 * caller may depend on is refused rather than faked:
 *
 * - posix_madvise(): all of POSIX_MADV_* succeed. They are hints by
 *   definition (POSIX_MADV_DONTNEED included: the contents are kept).
 * - madvise(): MADV_NORMAL, MADV_RANDOM, MADV_SEQUENTIAL and MADV_WILLNEED
 *   succeed, as does MADV_FREE, which lets the system discard the contents
 *   and so also allows it to keep them. MADV_DONTNEED fails with EINVAL:
 *   on Linux it discards a private range, which then reads back as zeros,
 *   and allocators rely on that to skip zeroing memory they reuse. A caller
 *   that gets an error falls back to clearing the memory itself.
 *
 * The range must start on a page boundary (EINVAL). Whether it is mapped is
 * not checked. */
#define POSIX_MADV_NORMAL     0
#define POSIX_MADV_RANDOM     1
#define POSIX_MADV_SEQUENTIAL 2
#define POSIX_MADV_WILLNEED   3
#define POSIX_MADV_DONTNEED   4

#define MADV_NORMAL     POSIX_MADV_NORMAL
#define MADV_RANDOM     POSIX_MADV_RANDOM
#define MADV_SEQUENTIAL POSIX_MADV_SEQUENTIAL
#define MADV_WILLNEED   POSIX_MADV_WILLNEED
#define MADV_DONTNEED   POSIX_MADV_DONTNEED
#define MADV_FREE       8


extern int posix_madvise(void *addr, size_t len, int advice);


extern int madvise(void *addr, size_t len, int advice);


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
