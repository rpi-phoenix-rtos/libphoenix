/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * flock() - BSD whole-file advisory locks, emulated with POSIX record locks
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>


/* Phoenix has no native flock(), but the kernel implements real fcntl()
 * record locks (F_GETLK/F_SETLK/F_SETLKW), so flock() is mapped onto a lock
 * of the whole file: LOCK_SH -> F_RDLCK, LOCK_EX -> F_WRLCK, LOCK_UN ->
 * F_UNLCK, and LOCK_NB selects the non-blocking F_SETLK. This is the same
 * emulation gnulib and several BSD-derived libcs use on systems without a
 * native flock.
 *
 * It inherits the two ways record locks differ from BSD flock() locks,
 * which a caller relying on the latter must know about:
 *   - the lock is owned by the PROCESS, not by the open file description, so
 *     two descriptors of the same file in one process never conflict, and a
 *     lock taken through one of them can be changed through the other;
 *   - the lock is released when the process closes ANY descriptor of the
 *     file, not only the last one, and is not inherited across fork().
 * Cross-process mutual exclusion -- what lock files are used for -- behaves
 * as expected.
 *
 * The kernel only locks seekable (regular) files; for a pipe, socket or tty
 * this reports EOPNOTSUPP, as BSD does for "an object other than a file".
 * The no-op stub this replaces reported success for everything, so two
 * processes could both "hold" an exclusive lock. */
int flock(int fd, int operation)
{
	struct flock fl;
	int cmd;

	switch (operation & ~LOCK_NB) {
		case LOCK_SH:
			fl.l_type = F_RDLCK;
			break;
		case LOCK_EX:
			fl.l_type = F_WRLCK;
			break;
		case LOCK_UN:
			fl.l_type = F_UNLCK;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 0; /* to end of file, however far it grows */
	fl.l_pid = 0;

	cmd = ((operation & LOCK_NB) != 0) ? F_SETLK : F_SETLKW;

	if (fcntl(fd, cmd, &fl) < 0) {
		/* The request is well formed, so EINVAL can only mean the kernel
		 * refused the object type (F_SEEKABLE). EAGAIN is EWOULDBLOCK. */
		if (errno == EINVAL) {
			errno = EOPNOTSUPP;
		}
		return -1;
	}

	return 0;
}
