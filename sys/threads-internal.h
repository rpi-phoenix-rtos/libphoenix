/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal thread interface
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_THREADS_H_
#define _LIBPHOENIX_INTERNAL_THREADS_H_


/* Nonzero once the process has started (or tried to start) a second thread.
 *
 * Set by beginthreadex() -- the one call every thread creation goes through,
 * pthread_create() and beginthread() included -- BEFORE the syscall, and never
 * cleared, so it only goes 0 -> 1. A thread that reads 0 is therefore the only
 * thread in the process: nothing that existed before the store can see a stale
 * 0 except the thread that made the store, and every thread created after it
 * sees 1 (see beginthreadex()). That makes a relaxed load enough to decide
 * whether process-wide state such as the heap needs a lock.
 *
 * A fork() child inherits the value (a stale 1 only costs speed); exec() starts
 * over at 0. Kept free of other includes so host harnesses that compile single
 * libphoenix files can use it. */
extern int __libc_multithreaded;


static inline int _libc_isMultithreaded(void)
{
	return __atomic_load_n(&__libc_multithreaded, __ATOMIC_RELAXED);
}


#endif
