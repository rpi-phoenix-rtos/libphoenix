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
