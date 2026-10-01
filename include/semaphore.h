/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * semaphore.h - unnamed POSIX semaphores
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_SEMAPHORE_H_
#define _LIBPHOENIX_SEMAPHORE_H_

#include <time.h>


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Process-private unnamed semaphores. The count is kept in user space, so
 * sem_trywait() and an uncontended sem_wait()/sem_post() make no system
 * call; a waiter blocks reading a pipe into which sem_post() writes one byte
 * per waiter it releases. sem_post() is async-signal-safe, as POSIX requires,
 * and sem_wait() can be interrupted by a signal (EINTR). Each semaphore holds
 * two descriptors until sem_destroy(), and must not be used by both sides of a
 * fork(). sem_init() with pshared != 0 fails with ENOSYS; named semaphores
 * (sem_open()) are not provided.
 */
typedef struct {
	int __value; /* the count, or minus the number of waiters */
	int __fd[2];
} sem_t;


#define SEM_VALUE_MAX 0x7fffffff
#define SEM_FAILED    ((sem_t *)0)


extern int sem_init(sem_t *sem, int pshared, unsigned int value);


extern int sem_destroy(sem_t *sem);


extern int sem_post(sem_t *sem);


extern int sem_wait(sem_t *sem);


extern int sem_trywait(sem_t *sem);


extern int sem_timedwait(sem_t *__restrict sem, const struct timespec *__restrict abstime);


extern int sem_clockwait(sem_t *__restrict sem, clockid_t clock, const struct timespec *__restrict abstime);


extern int sem_getvalue(sem_t *__restrict sem, int *__restrict sval);


#ifdef __cplusplus
}
#endif


#endif
