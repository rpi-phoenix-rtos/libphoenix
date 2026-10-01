/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal user-space lock (see ulock-internal.h)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <unistd.h>
#include <sys/threads.h>

#include "ulock-internal.h"


/* Polls of a held lock before going to sleep. A critical section that ends
 * within a few hundred nanoseconds -- an allocation, a list update -- then costs
 * no system call even when contended; sleeping and waking costs two. */
#define ULOCK_SPIN 100U


void _ulock_cpuRelax(void)
{
#if defined(__aarch64__)
	__asm__ volatile("yield" ::: "memory");
#elif defined(__i386__) || defined(__x86_64__)
	__asm__ volatile("pause" ::: "memory");
#else
	__asm__ volatile("" ::: "memory");
#endif
}


int _ulock_lockSlow(volatile unsigned int *word, time_t timeout, int clock)
{
	unsigned int i;
	int err;

	for (i = 0; i < ULOCK_SPIN; i++) {
		if ((__atomic_load_n(word, __ATOMIC_RELAXED) == ULOCK_FREE) && (_ulock_tryLock(word) == EOK)) {
			return EOK;
		}
		_ulock_cpuRelax();
	}

	/* Mark the lock contended before sleeping, so its holder wakes us. Whoever
	 * takes it here leaves it marked: others may still be asleep, and its unlock
	 * must wake the next one. */
	while (__atomic_exchange_n(word, ULOCK_CONTENDED, __ATOMIC_ACQUIRE) != ULOCK_FREE) {
		err = futexWait(word, ULOCK_CONTENDED, timeout, clock);
		if (err == -ETIME) {
			return -ETIME;
		}

		if ((err != EOK) && (err != -EAGAIN) && (err != -EINTR)) {
			/* Cannot sleep (a kernel without futexes, no memory): poll instead */
			(void)usleep(0);
		}
	}

	return EOK;
}
