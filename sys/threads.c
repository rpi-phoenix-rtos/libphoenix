/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/threads
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/threads.h>
#include <errno.h>

#include "threads-internal.h"


int __libc_multithreaded = 0;


/* The raw syscall stub; arch/<arch>/syscalls.S gives it this name */
int sys_beginthreadex(void (*start)(void *), int priority, void *stack, unsigned int stacksz, void *arg, handle_t *id);


__EXPORT_INLINE int beginthread(void (*start)(void *), int priority, void *stack, unsigned int stacksz, void *arg);
__EXPORT_INLINE int threadsinfo(int n, unsigned int flags, threadinfo_t *info);
__EXPORT_INLINE int threadinfo(int tid, unsigned int flags, threadinfo_t *info);
__EXPORT_INLINE int threadcount(void);
__EXPORT_INLINE int mutexCreateWithAttr(handle_t *h, const struct lockAttr *attr);
__EXPORT_INLINE int condCreateWithAttr(handle_t *h, const struct condAttr *attr);


/* Every thread of a process starts here: pthread_create(), beginthread() (an
 * inline in <sys/threads.h>) and every server that calls beginthread*() directly.
 *
 * The flag is stored BEFORE the syscall, so it is 1 before the new thread can
 * run. Ordering: the creating thread sees its own store (program order; the
 * call below is also a compiler barrier). The new thread is made runnable by
 * the kernel under its scheduler lock and starts after that lock's release, so
 * everything this thread wrote before the svc -- the flag and the heap it built
 * unlocked -- happens-before the new thread's first instruction, the same edge
 * that makes the start argument visible to it. The release store only states
 * that intent. It stays set if the syscall fails: a stale 1 costs speed, never
 * correctness. */
int beginthreadex(void (*start)(void *), int priority, void *stack, unsigned int stacksz, void *arg, handle_t *id)
{
	__atomic_store_n(&__libc_multithreaded, 1, __ATOMIC_RELEASE);

	return sys_beginthreadex(start, priority, stack, stacksz, arg, id);
}


int mutexCreate(handle_t *h)
{
	static const struct lockAttr defaultAttr = { .type = PH_LOCK_NORMAL, .protocol = PH_LOCK_PROTO_INHERIT, .robust = PH_LOCK_STALLED };

	return phMutexCreate(h, &defaultAttr);
}


int mutexLock(handle_t m)
{
	int err;

	do {
		err = phMutexLock(m, 0, PH_CLOCK_MONOTONIC);
	} while (err == -EINTR);

	return err;
}


int mutexLockClockWait(handle_t m, time_t timeout, int clock)
{
	int err;

	do {
		/* FIXME: for PH_CLOCK_RELATIVE the timeout should be recalculated on EINTR */
		err = phMutexLock(m, timeout, clock);
	} while (err == -EINTR);

	return err;
}


int mutexLockWait(handle_t m, time_t timeout)
{
	return mutexLockClockWait(m, timeout, PH_CLOCK_MONOTONIC);
}


int condCreate(handle_t *h)
{
	static const struct condAttr defaultAttr = { .clock = PH_CLOCK_RELATIVE, .type = PH_COND_NORMAL };

	return phCondCreate(h, &defaultAttr);
}


int condClockWait(handle_t h, handle_t m, time_t timeout, int clock)
{
	int err, mut_err;

	err = phCondWait(h, m, timeout, clock);

	while (err == -EINTR) {
		mut_err = mutexLock(m);
		if (mut_err != EOK) {
			return mut_err;
		}
		err = phCondWait(h, m, timeout, clock);
	}

	return err;
}


int condWait(handle_t h, handle_t m, time_t timeout)
{
	return condClockWait(h, m, timeout, -1);
}


/* WARN: does not support robust locks (TODO?) */
int mutexLock2(handle_t m1, handle_t m2)
{
	int err;
	int tmp;

	if ((err = mutexLock(m1)) < 0)
		return err;

	while (mutexTry(m2) < 0) {
		mutexUnlock(m1);
		if ((err = mutexLock(m2)) < 0)
			return err;

		tmp = m1;
		m1 = m2;
		m2 = tmp;
	}

	return EOK;
}


int priority(int priority)
{
	return sys_priority(priority, NULL);
}


int setPriority(int priority)
{
	return sys_priority(priority, NULL);
}


int getPriority(void)
{
	int prio = PH_PRIO_DEFAULT;
	(void)sys_priority(PH_GET_PRIO, &prio);
	return prio;
}
