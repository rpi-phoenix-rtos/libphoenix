/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * malloc.h - non-standard allocator interfaces
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_MALLOC_H_
#define _LIBPHOENIX_MALLOC_H_


#include <stdlib.h> /* malloc(), free(), malloc_usable_size() */


#ifdef __cplusplus
extern "C" {
#endif


/* Obsolete: allocate `size` bytes aligned to `alignment` (a power of two).
 * Use aligned_alloc() or posix_memalign(). */
extern void *memalign(size_t alignment, size_t size);


#ifdef __cplusplus
}
#endif


#endif
