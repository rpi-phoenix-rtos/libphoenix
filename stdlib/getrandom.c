/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stdlib/getrandom
 *
 * Copyright 2026 Phoenix Systems
 * Author: Witold Bołt
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/random.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


/* Both functions draw from /dev/urandom (hardware-RNG-backed on platforms with a
 * /dev/hwrng, PRNG fallback otherwise). /dev/urandom never blocks, so GRND_* flags
 * are accepted and ignored. The fd is opened per call to stay reentrant. */
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags)
{
	int fd;
	ssize_t total = 0;

	(void)flags;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0) {
		return -1;
	}

	while ((size_t)total < buflen) {
		ssize_t n = read(fd, (char *)buf + total, buflen - (size_t)total);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (n == 0) {
			break;
		}
		total += n;
	}

	close(fd);

	if ((total == 0) && (buflen > 0)) {
		return -1;
	}
	return total;
}


int getentropy(void *buf, size_t buflen)
{
	ssize_t n;

	if (buflen > 256) {
		errno = EIO;
		return -1;
	}

	n = getrandom(buf, buflen, 0);
	if ((n < 0) || ((size_t)n != buflen)) {
		errno = EIO;
		return -1;
	}
	return 0;
}
