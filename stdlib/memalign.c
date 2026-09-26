/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stdlib/memalign - aligned allocation (posix_memalign, aligned_alloc, memalign)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <malloc.h>
#include <stdlib.h>

#include "malloc-internal.h"


/* Each public entry point is a thin validator over _malloc_aligned(), which
 * lives in the allocator. They are kept out of malloc_dl.o on purpose: that
 * object is linked into every program, so a program (or a port's compat
 * shim) that still defines its own posix_memalign() keeps linking. */


static int memalign_isPow2(size_t x)
{
	return ((x != 0u) && ((x & (x - 1u)) == 0u)) ? 1 : 0;
}


int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	int err = errno;
	void *p;

	/* POSIX: a power of two and a multiple of sizeof(void *) */
	if ((memalign_isPow2(alignment) == 0) || ((alignment % sizeof(void *)) != 0u)) {
		return EINVAL;
	}

	p = _malloc_aligned(alignment, size);
	if (p == NULL) {
		errno = err; /* reports through the return value, leaves errno alone */
		return ENOMEM;
	}

	*memptr = p;
	return 0;
}


void *aligned_alloc(size_t alignment, size_t size)
{
	/* C17 7.22.3.1: any valid alignment, i.e. a power of two. (The C11
	 * requirement that size be a multiple of alignment was dropped by DR 460.) */
	if (memalign_isPow2(alignment) == 0) {
		errno = EINVAL;
		return NULL;
	}

	return _malloc_aligned(alignment, size);
}


void *memalign(size_t alignment, size_t size)
{
	/* Obsolete SVID/glibc interface; same rules as aligned_alloc(). */
	return aligned_alloc(alignment, size);
}
