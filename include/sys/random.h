/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/random.h
 *
 * Copyright 2026 Phoenix Systems
 * Author: Witold Bołt
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYS_RANDOM_H
#define _SYS_RANDOM_H

#include <sys/types.h>


/* getrandom() flags. The Phoenix implementation reads /dev/urandom, which never
 * blocks, so the blocking-related flags are accepted and ignored. */
#define GRND_NONBLOCK 0x0001
#define GRND_RANDOM   0x0002
#define GRND_INSECURE 0x0004


/* Fill buf with up to buflen random bytes from /dev/urandom (hardware-RNG-backed
 * where available). Returns the number of bytes written, or -1 with errno set. */
extern ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);


/* Fill buf with exactly buflen (<= 256) random bytes. Returns 0 on success, -1 with
 * errno set on error. */
extern int getentropy(void *buf, size_t buflen);


#endif
