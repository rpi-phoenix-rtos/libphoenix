/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * POSIX implementation - semaphores
 *
 * Copyright 2026 Phoenix Systems
 * Author: Michal Lach, Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBPHOENIX_SEMAPHORE_H_
#define _LIBPHOENIX_SEMAPHORE_H_

#include <time.h>
#include <limits.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEM_FAILED ((sem_t *)0)


/*
 * Named semaphores (sem_open()) live in posixsrv and are reached through a
 * descriptor on their /dev node.
 *
 * Unnamed semaphores (sem_init()) are process-private and keep their count in
 * user space, so sem_trywait() and an uncontended sem_wait()/sem_post() make
 * no system call; a waiter blocks reading a pipe into which sem_post() writes
 * one byte per waiter it releases. sem_post() is async-signal-safe, as POSIX
 * requires, and sem_wait() can be interrupted by a signal (EINTR). Each one
 * holds two descriptors until sem_destroy(), and must not be used by both sides
 * of a fork(). sem_init() with pshared != 0 fails with ENOSYS.
 */
typedef struct {
	/* clang-format off */
	enum { smNamed, smUnnamed } type;
	/* clang-format on */

	union {
		struct {
			int value; /* the count, or minus the number of waiters */
			int fd[2];
		} unnamed;
		int fd;
	};
} sem_t;


int sem_wait(sem_t *sem);


int sem_trywait(sem_t *sem);


int sem_timedwait(sem_t *__restrict sem, const struct timespec *__restrict abs_timeout);


/* POSIX.1-2024: abs_timeout on CLOCK_REALTIME or CLOCK_MONOTONIC */
int sem_clockwait(sem_t *__restrict sem, clockid_t clock, const struct timespec *__restrict abs_timeout);


int sem_getvalue(sem_t *__restrict sem, int *__restrict value);


int sem_post(sem_t *sem);


int sem_close(sem_t *sem);


sem_t *sem_open(const char *name, int oflag, ... /* mode_t mode, unsigned int value */);


int sem_unlink(const char *name);


int sem_destroy(sem_t *sem);


int sem_init(sem_t *sem, int pshared, unsigned int value);


#ifdef __cplusplus
}
#endif


#endif /* _LIBPHOENIX_SEMAPHORE_H_ */
