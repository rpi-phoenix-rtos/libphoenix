/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * POSIX barriers (pthread_barrier_*)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <pthread.h>


/* A mutex, a condition variable and a generation counter. The thread that
 * completes a round bumps the generation and wakes the rest; a waiter sleeps
 * until the generation it arrived in is over, so a thread that races ahead
 * into the NEXT round cannot be confused with the one being released.
 * `inside` counts threads anywhere in pthread_barrier_wait(), released or not,
 * so pthread_barrier_destroy() can wait until the last one has left the mutex
 * and condition variable before destroying them. */


int pthread_barrierattr_init(pthread_barrierattr_t *attr)
{
	if (attr == NULL) {
		return EINVAL;
	}
	attr->pshared = PTHREAD_PROCESS_PRIVATE;
	return 0;
}


int pthread_barrierattr_destroy(pthread_barrierattr_t *attr)
{
	return (attr == NULL) ? EINVAL : 0;
}


int pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared)
{
	if ((attr == NULL) || (pshared == NULL)) {
		return EINVAL;
	}
	*pshared = attr->pshared;
	return 0;
}


int pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared)
{
	if ((attr == NULL) || ((pshared != PTHREAD_PROCESS_PRIVATE) && (pshared != PTHREAD_PROCESS_SHARED))) {
		return EINVAL;
	}
	/* OS-LIMITATION: the mutex and condvar underneath are process-local
	 * handles, as for pthread_mutexattr_setpshared() */
	if (pshared == PTHREAD_PROCESS_SHARED) {
		return ENOTSUP;
	}
	attr->pshared = pshared;
	return 0;
}


int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count)
{
	int err;

	if ((barrier == NULL) || (count == 0u)) {
		return EINVAL;
	}
	if ((attr != NULL) && (attr->pshared != PTHREAD_PROCESS_PRIVATE)) {
		return ENOTSUP;
	}

	err = pthread_mutex_init(&barrier->mutex, NULL);
	if (err != 0) {
		return err;
	}
	err = pthread_cond_init(&barrier->cond, NULL);
	if (err != 0) {
		(void)pthread_mutex_destroy(&barrier->mutex);
		return err;
	}

	barrier->count = count;
	barrier->waiting = 0;
	barrier->inside = 0;
	barrier->generation = 0;

	return 0;
}


int pthread_barrier_destroy(pthread_barrier_t *barrier)
{
	if (barrier == NULL) {
		return EINVAL;
	}

	(void)pthread_mutex_lock(&barrier->mutex);
	if (barrier->waiting != 0u) {
		/* threads are blocked in the current round */
		(void)pthread_mutex_unlock(&barrier->mutex);
		return EBUSY;
	}
	/* released threads may not have returned yet */
	while (barrier->inside != 0u) {
		(void)pthread_cond_wait(&barrier->cond, &barrier->mutex);
	}
	(void)pthread_mutex_unlock(&barrier->mutex);

	(void)pthread_cond_destroy(&barrier->cond);
	(void)pthread_mutex_destroy(&barrier->mutex);

	return 0;
}


int pthread_barrier_wait(pthread_barrier_t *barrier)
{
	unsigned int generation;
	int ret = 0;

	if (barrier == NULL) {
		return EINVAL;
	}

	(void)pthread_mutex_lock(&barrier->mutex);
	barrier->inside++;
	generation = barrier->generation;

	if (++barrier->waiting == barrier->count) {
		barrier->generation++;
		barrier->waiting = 0;
		ret = PTHREAD_BARRIER_SERIAL_THREAD;
		(void)pthread_cond_broadcast(&barrier->cond);
	}
	else {
		while (generation == barrier->generation) {
			(void)pthread_cond_wait(&barrier->cond, &barrier->mutex);
		}
	}

	if (--barrier->inside == 0u) {
		/* a pthread_barrier_destroy() may be waiting for us to leave */
		(void)pthread_cond_broadcast(&barrier->cond);
	}
	(void)pthread_mutex_unlock(&barrier->mutex);

	return ret;
}
