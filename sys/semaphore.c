/*
 * Phoenix-RTOS
 *
 * Semaphores
 *
 * Copyright 2012, 2017, 2018, 2026 Phoenix Systems
 * Copyright 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/time.h>
#include <errno.h>
#include <sys/threads.h>
#include <time.h>


int semaphoreCreate(semaphore_t *s, unsigned int v)
{
	static const struct condAttr cAttr = { .clock = PH_CLOCK_MONOTONIC, .type = PH_COND_NORMAL };
	int err;

	err = mutexCreate(&s->mutex);
	if (err < 0) {
		return err;
	}

	err = condCreateWithAttr(&s->cond, &cAttr);
	if (err < 0) {
		resourceDestroy(s->mutex);
		return err;
	}

	s->v = v;

	return 0;
}


int semaphoreDown(semaphore_t *s, time_t timeout)
{
	time_t deadline = 0;

	if (timeout != 0) {
		time_t now;
		gettime(&now, NULL);
		deadline = now + timeout;
	}

	mutexLock(s->mutex);
	int err;
	do {
		if (s->v > 0) {
			--s->v;
			err = 0;
			break;
		}

		err = condWait(s->cond, s->mutex, deadline);
	} while (err != -ETIME);
	mutexUnlock(s->mutex);

	return err;
}


int semaphoreUp(semaphore_t *s)
{
	mutexLock(s->mutex);
	++s->v;
	mutexUnlock(s->mutex);

	/* Signal outside the mutex - condSignal reschedules, so doing it under the
	 * lock forces the woken waiter to immediately re-block on the mutex we still
	 * hold. Signalling after unlock is race-free here: a waiter only parks while
	 * holding this mutex (condWait releases it atomically), so no up can slip its
	 * increment in before the waiter has parked.
	 *
	 * This MUST fire on every up, not only on the 0->1 transition: a counting
	 * semaphore can have several units become available while multiple threads
	 * are parked. Signalling only when v was zero wakes exactly one waiter and
	 * loses the wakeup for every other parked waiter even though v > 0, which
	 * deadlocks multi-consumer pools (e.g. vkQuake's task workers).
	 */
	condSignal(s->cond);

	return 0;
}


int semaphoreDone(semaphore_t *s)
{
	resourceDestroy(s->mutex);
	resourceDestroy(s->cond);

	return 0;
}
