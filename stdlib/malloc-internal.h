/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal allocator interface
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_MALLOC_H_
#define _LIBPHOENIX_INTERNAL_MALLOC_H_


#include <stddef.h>


/* Allocate `size` bytes aligned to `alignment`, freeable with free().
 * `alignment` must be a power of two. Returns NULL with errno = ENOMEM on
 * failure. Backs posix_memalign(), aligned_alloc() and memalign(). */
void *_malloc_aligned(size_t alignment, size_t size);


/* Called by fork() around the system call: prepare in the parent before it,
 * then parent in the parent or child in the child after it. */
void _malloc_forkPrepare(void);


void _malloc_forkParent(void);


void _malloc_forkChild(void);


#endif
