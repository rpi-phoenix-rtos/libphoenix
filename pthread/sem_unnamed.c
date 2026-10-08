/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Unnamed POSIX semaphores, the backend of sem_init() and friends in
 * posix/sem.c (named ones go to posixsrv instead)
 *
 * value is the count when it is >= 0, and minus the number of threads
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

#include "sem_unnamed.h"


/* sem_take() timeouts */
#define SEM_FOREVER (-1)


int _sem_unnamedInit(sem_t *sem, unsigned int value)
{
	if (pipe2(sem->unnamed.fd, O_CLOEXEC) < 0) {
		errno = ENOSPC;
		return -1;
	}

	/* The read end does not block: poll() reporting a token does not reserve
	 * it, and another waiter may take it first (see sem_token()). */
	if (fcntl(sem->unnamed.fd[0], F_SETFL, O_NONBLOCK) < 0) {
		(void)close(sem->unnamed.fd[0]);
		(void)close(sem->unnamed.fd[1]);
		errno = ENOSPC;
		return -1;
	}

	__atomic_store_n(&sem->unnamed.value, (int)value, __ATOMIC_SEQ_CST);

	return 0;
}


int _sem_unnamedDestroy(sem_t *sem)
{
	(void)close(sem->unnamed.fd[0]);
	(void)close(sem->unnamed.fd[1]);
	sem->unnamed.fd[0] = -1;
	sem->unnamed.fd[1] = -1;

	return 0;
}


int _sem_unnamedPost(sem_t *sem)
{
	int old = __atomic_load_n(&sem->unnamed.value, __ATOMIC_RELAXED), err = errno;
	ssize_t n;

	do {
		if (old == SEM_VALUE_MAX) {
			errno = EOVERFLOW;
			return -1;
		}
	} while (__atomic_compare_exchange_n(&sem->unnamed.value, &old, old + 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED) == 0);

	if (old < 0) {
		/* A waiter was counted: release one */
		do {
			n = write(sem->unnamed.fd[1], "", 1);
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


/* Waits for a token, for at most timeout ms (SEM_FOREVER: no limit).
 *
 * poll() reporting the pipe readable does not reserve the token: two waiters
 * can both see it, and the one that loses the read would then block past its
 * deadline. The read end is non-blocking, so the loser polls again; for a timed
 * wait it reports ETIMEDOUT and sem_take() re-waits for what is left of the
 * deadline. */
static int sem_token(sem_t *sem, int timeout)
{
	struct pollfd pfd = { .fd = sem->unnamed.fd[0], .events = POLLIN };
	char c;
	int n;

	for (;;) {
		n = poll(&pfd, 1, timeout);
		if (n == 0) {
			return ETIMEDOUT;
		}
		if (n < 0) {
			return errno;
		}

		n = (int)read(sem->unnamed.fd[0], &c, 1);
		if (n == 1) {
			return 0;
		}
		if ((n < 0) && (errno == EAGAIN)) {
			if (timeout != SEM_FOREVER) {
				return ETIMEDOUT;
			}
			continue;
		}

		return (n < 0) ? errno : EINVAL;
	}
}


/* Gives up waiting: off the count, or take the token a post sent already */
static int sem_cancel(sem_t *sem, int err)
{
	int old = __atomic_load_n(&sem->unnamed.value, __ATOMIC_SEQ_CST);

	while (old < 0) {
		if (__atomic_compare_exchange_n(&sem->unnamed.value, &old, old + 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST) != 0) {
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


int _sem_unnamedTake(sem_t *sem, clockid_t clock, const struct timespec *abstime)
{
	int err;

	if (__atomic_fetch_sub(&sem->unnamed.value, 1, __ATOMIC_SEQ_CST) > 0) {
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


int _sem_unnamedTryWait(sem_t *sem)
{
	int old = __atomic_load_n(&sem->unnamed.value, __ATOMIC_RELAXED);

	while (old > 0) {
		if (__atomic_compare_exchange_n(&sem->unnamed.value, &old, old - 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED) != 0) {
			return 0;
		}
	}

	errno = EAGAIN;
	return -1;
}


int _sem_unnamedGetValue(sem_t *sem, int *sval)
{
	int v = __atomic_load_n(&sem->unnamed.value, __ATOMIC_SEQ_CST);

	/* POSIX allows minus the number of waiters; 0 is what glibc reports */
	*sval = (v < 0) ? 0 : v;

	return 0;
}
