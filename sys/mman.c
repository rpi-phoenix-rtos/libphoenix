/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * mmap/munmap
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <errno.h>


WRAP_ERRNO_DEF(int, munmap, (void *vadddr, size_t size), (vadddr, size))


WRAP_ERRNO_DEF(int, mprotect, (void *vaddr, size_t len, int prot), (vaddr, len, prot))


extern int sys_mmap(void **vaddr, size_t size, int prot, int flags, int fildes, off_t offs);


void *mmap(void *vaddr, size_t size, int prot, int flags, int fildes, off_t offs)
{
	int err = sys_mmap(&vaddr, size, prot, flags, fildes, offs);
	if (err < 0) {
		vaddr = MAP_FAILED;
		SET_ERRNO(err);
	}

	return vaddr;
}


/* Phoenix has no swap-to-disk, so anonymous memory is never paged out to a
 * backing store; locking it against paging is a no-op that trivially succeeds. */
int mlock(const void *addr, size_t len)
{
	(void)addr;
	(void)len;
	return 0;
}


int munlock(const void *addr, size_t len)
{
	(void)addr;
	(void)len;
	return 0;
}


int mlockall(int flags)
{
	(void)flags;
	return 0;
}


int munlockall(void)
{
	return 0;
}


/* Shared argument check: 0, or the error number (see <sys/mman.h>) */
static int madvise_check(const void *addr, size_t len)
{
	if (((uintptr_t)addr & (PAGE_SIZE - 1u)) != 0u) {
		return EINVAL;
	}

	if (len > (UINTPTR_MAX - (uintptr_t)addr)) {
		return EINVAL;
	}

	return 0;
}


int posix_madvise(void *addr, size_t len, int advice)
{
	switch (advice) {
		case POSIX_MADV_NORMAL:
		case POSIX_MADV_RANDOM:
		case POSIX_MADV_SEQUENTIAL:
		case POSIX_MADV_WILLNEED:
		case POSIX_MADV_DONTNEED:
			break;

		default:
			return EINVAL;
	}

	/* Every advice is a hint, so none needs doing */
	return madvise_check(addr, len);
}


int madvise(void *addr, size_t len, int advice)
{
	int err;

	switch (advice) {
		case MADV_NORMAL:
		case MADV_RANDOM:
		case MADV_SEQUENTIAL:
		case MADV_WILLNEED:
		case MADV_FREE:
			err = madvise_check(addr, len);
			break;

		case MADV_DONTNEED:
			/* Promises zero-filled pages on the next access, which needs the
			 * kernel to drop them; it cannot */
		default:
			err = EINVAL;
			break;
	}

	return (err == 0) ? 0 : SET_ERRNO(-err);
}
