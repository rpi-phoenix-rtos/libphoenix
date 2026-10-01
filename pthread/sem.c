/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Unnamed POSIX semaphores (see <semaphore.h>)
 *
 * __value is the count when it is >= 0, and minus the number of threads
 * waiting when it is < 0. A waiter decrements it; if there was no unit it
 * waits for a token -- one byte -- in the pipe. A poster increments it, and if
 * that releases a waiter, writes one token. Tokens are not addressed to a
 * particular waiter, so a waiter that gives up (signal, timeout) first tries
 * to take itself off the count; if a post has already counted it, a token for
 * it is in the pipe or on its way, and it takes that instead.
 *
 * sem_post() takes no lock and makes at most one write() call, so it is
 * async-signal-safe and never deadlocks with the thread it interrupts.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <semaphore.h>
#include <time.h>
#include <unistd.h>


/* sem_take() timeouts */
#define SEM_FOREVER (-1)


int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	if (pshared != 0) {
		errno = ENOSYS;
		return -1;
	}

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}

	if (pipe2(sem->__fd, O_CLOEXEC) < 0) {
		errno = ENOSPC;
		return -1;
	}

	__atomic_store_n(&sem->__value, (int)value, __ATOMIC_SEQ_CST);

	return 0;
}


int sem_destroy(sem_t *sem)
{
	(void)close(sem->__fd[0]);
	(void)close(sem->__fd[1]);
	sem->__fd[0] = -1;
	sem->__fd[1] = -1;

	return 0;
}


int sem_post(sem_t *sem)
{
	int old = __atomic_load_n(&sem->__value, __ATOMIC_RELAXED), err = errno;
	ssize_t n;

	do {
		if (old == SEM_VALUE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
	} while (__atomic_compare_exchange_n(&sem->__value, &old, old + 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED) == 0);

	if (old < 0) {
		/* A waiter was counted: release one */
		do {
			n = write(sem->__fd[1], "", 1);
		} while ((n < 0) && (errno == EINTR));

		if (n != 1) {
			/* Only a destroyed semaphore gets here */
			errno = EINVAL;
			return -1;
		}
	}

	errno = err; /* async-signal-safe: callable from a handler */
	return 0;
}


/* Waits for a token, for at most timeout ms (SEM_FOREVER: no limit) */
static int sem_token(sem_t *sem, int timeout)
{
	struct pollfd pfd = { .fd = sem->__fd[0], .events = POLLIN };
	char c;
	int n;

	if (timeout != SEM_FOREVER) {
		n = poll(&pfd, 1, timeout);
		if (n == 0) {
			return ETIMEDOUT;
		}
		if (n < 0) {
			return errno;
		}
	}

	n = (int)read(sem->__fd[0], &c, 1);
	if (n == 1) {
		return 0;
	}

	return (n < 0) ? errno : EINVAL;
}


/* Gives up waiting: off the count, or take the token a post sent already */
static int sem_cancel(sem_t *sem, int err)
{
	int old = __atomic_load_n(&sem->__value, __ATOMIC_SEQ_CST);

	while (old < 0) {
		if (__atomic_compare_exchange_n(&sem->__value, &old, old + 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) != 0) {
			return err;
		}
	}

	do {
		err = sem_token(sem, SEM_FOREVER);
	} while (err == EINTR);

	return err;
}


/* Milliseconds from now to abstime on clock, rounded up; 0 if past */
static int sem_remaining(clockid_t clock, const struct timespec *abstime)
{
	struct timespec now;
	long long ms;

	if (clock_gettime(clock, &now) != 0) {
		return 0;
	}

	ms = ((long long)abstime->tv_sec - now.tv_sec) * 1000LL + (abstime->tv_nsec - now.tv_nsec + 999999L) / 1000000L;
	if (ms <= 0) {
		return 0;
	}

	return (ms > 0x7fffffffLL) ? 0x7fffffff : (int)ms;
}


static int sem_take(sem_t *sem, clockid_t clock, const struct timespec *abstime)
{
	int err;

	if (__atomic_fetch_sub(&sem->__value, 1, __ATOMIC_SEQ_CST) > 0) {
		return 0;
	}

	if ((abstime != NULL) && ((abstime->tv_nsec < 0) || (abstime->tv_nsec >= 1000000000L))) {
		err = sem_cancel(sem, EINVAL);
	}
	else {
		for (;;) {
			err = sem_token(sem, (abstime == NULL) ? SEM_FOREVER : sem_remaining(clock, abstime));
			if ((err != ETIMEDOUT) || (abstime == NULL) || (sem_remaining(clock, abstime) == 0)) {
				break;
			}
			/* poll() woke early: wait out the rest */
		}

		if (err != 0) {
			err = sem_cancel(sem, err);
		}
	}

	if (err != 0) {
		errno = err;
		return -1;
	}

	return 0;
}


int sem_wait(sem_t *sem)
{
	return sem_take(sem, CLOCK_REALTIME, NULL);
}


int sem_timedwait(sem_t *__restrict sem, const struct timespec *__restrict abstime)
{
	return sem_take(sem, CLOCK_REALTIME, abstime);
}


int sem_clockwait(sem_t *__restrict sem, clockid_t clock, const struct timespec *__restrict abstime)
{
	if ((clock != CLOCK_REALTIME) && (clock != CLOCK_MONOTONIC)) {
		errno = EINVAL;
		return -1;
	}

	return sem_take(sem, clock, abstime);
}


int sem_trywait(sem_t *sem)
{
	int old = __atomic_load_n(&sem->__value, __ATOMIC_RELAXED);

	while (old > 0) {
		if (__atomic_compare_exchange_n(&sem->__value, &old, old - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED) != 0) {
			return 0;
		}
	}

	errno = EAGAIN;
	return -1;
}


int sem_getvalue(sem_t *__restrict sem, int *__restrict sval)
{
	int v = __atomic_load_n(&sem->__value, __ATOMIC_SEQ_CST);

	/* POSIX allows minus the number of waiters; 0 is what glibc reports */
	*sval = (v < 0) ? 0 : v;

	return 0;
}
