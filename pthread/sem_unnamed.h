/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Unnamed POSIX semaphores - internal interface for posix/sem.c
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBPHOENIX_PTHREAD_SEM_UNNAMED_H_
#define _LIBPHOENIX_PTHREAD_SEM_UNNAMED_H_

#include <semaphore.h>
#include <time.h>


/*
 * All of these follow the POSIX convention: 0 on success, -1 with errno set
 * on failure. _sem_unnamedPost() preserves errno on success, so it can be
 * called from a signal handler.
 */

int _sem_unnamedInit(sem_t *sem, unsigned int value);


int _sem_unnamedDestroy(sem_t *sem);


int _sem_unnamedPost(sem_t *sem);


/* abstime NULL: wait without a limit; clock is CLOCK_REALTIME or CLOCK_MONOTONIC */
int _sem_unnamedTake(sem_t *sem, clockid_t clock, const struct timespec *abstime);


int _sem_unnamedTryWait(sem_t *sem);


int _sem_unnamedGetValue(sem_t *sem, int *sval);


#endif
