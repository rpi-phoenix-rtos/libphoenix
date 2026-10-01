/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal user-space lock
 *
 * A mutex that is one word of memory. Taking and releasing it when nobody else
 * wants it is a single atomic operation, with no system call; the kernel is
 * entered only to sleep while the lock is held by another thread (futexWait)
 * and, on release, to wake such a sleeper (futexWake). The algorithm is mutex 3
 * from U. Drepper, "Futexes Are Tricky" (2011):
 *
 *   ULOCK_FREE       unlocked
 *   ULOCK_LOCKED     locked, nobody is asleep on it
 *   ULOCK_CONTENDED  locked, a thread may be asleep on it: release must wake one
 *
 * A word of zeroes is an unlocked lock, so static storage needs no initializer
 * and no destruction. Acquire ordering on lock, release ordering on unlock.
 * Not recursive and not owner-checked: see pthread/pthread.c for those.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_ULOCK_H_
#define _LIBPHOENIX_INTERNAL_ULOCK_H_

#include <errno.h>
#include <sys/threads.h>


#define ULOCK_FREE      0U
#define ULOCK_LOCKED    1U
#define ULOCK_CONTENDED 2U


/* The contended half of _ulock_lock(): spins briefly, then sleeps. `timeout` and
 * `clock` as for futexWait() (0 = no limit; a relative timeout restarts after a
 * wake-up that loses the race). Returns EOK or -ETIME. */
int _ulock_lockSlow(volatile unsigned int *word, time_t timeout, int clock);


/* Busy-waiting hint for spin loops */
void _ulock_cpuRelax(void);


static inline int _ulock_tryLock(volatile unsigned int *word)
{
	unsigned int expected = ULOCK_FREE;

	if (__atomic_compare_exchange_n(word, &expected, ULOCK_LOCKED, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
		return EOK;
	}

	return -EBUSY;
}


static inline void _ulock_lock(volatile unsigned int *word)
{
	if (_ulock_tryLock(word) != EOK) {
		(void)_ulock_lockSlow(word, 0, PH_CLOCK_RELATIVE);
	}
}


/* Returns -EPERM if the lock was not held (it is left unlocked) */
static inline int _ulock_unlock(volatile unsigned int *word)
{
	unsigned int old = __atomic_exchange_n(word, ULOCK_FREE, __ATOMIC_RELEASE);

	if (old == ULOCK_CONTENDED) {
		(void)futexWake(word, 1);
	}

	return (old == ULOCK_FREE) ? -EPERM : EOK;
}


#endif
