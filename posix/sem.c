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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <phoenix/semaphore.h>

#include "../common/util.h"
#include "../pthread/sem_unnamed.h"


#define SEM_PATH_SIZE (sizeof(SEMAPHORE_PATH) + NAME_MAX)


static int sem_path(const char *name, char *path, size_t size)
{
	int len;

	if (*name == '/') {
		name++;
	}

	if (*name == '\0' || strchr(name, '/') != NULL) {
		return -EINVAL;
	}

	len = snprintf(path, size, "%s%s", SEMAPHORE_PATH, name);
	if (len < 0 || (size_t)len >= size) {
		return -ENAMETOOLONG;
	}

	return EOK;
}


static int sem_create(const char *name, unsigned int value, mode_t mode)
{
	int ret;
	oid_t semctl;
	msg_t msg = { 0 };

	ret = lookup(SEMCTL_PATH, NULL, &semctl);
	if (ret != EOK) {
		return ret;
	}

	msg.type = mtCreate;
	msg.oid = semctl;
	msg.i.create.type = (int)value;
	msg.i.create.mode = mode & ~__getumask();
	msg.i.data = name;
	msg.i.size = strlen(name) + 1;

	ret = msgSend(semctl.port, &msg);
	if (ret == EOK) {
		ret = msg.o.err;
	}

	return ret;
}


sem_t *sem_open(const char *name, int oflag, ...)
{
	va_list list;
	char path[SEM_PATH_SIZE];
	int err, fd;
	mode_t mode = 0;
	unsigned int value = 0;
	sem_t *sem;

	if (name == NULL) {
		errno = EINVAL;
		return SEM_FAILED;
	}

	err = sem_path(name, path, sizeof(path));
	if (err != EOK) {
		errno = -err;
		return SEM_FAILED;
	}

	if ((oflag & O_CREAT) != 0) {
		va_start(list, oflag);
		mode = va_arg(list, mode_t);
		value = va_arg(list, unsigned int);
		va_end(list);
	}

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return SEM_FAILED;
	}

	if ((oflag & O_CREAT) != 0 && (oflag & O_EXCL) != 0) {
		err = sem_create(name, value, mode);
		if (err != EOK) {
			errno = -err;
			return SEM_FAILED;
		}
	}

	/*
	 * O_CLOEXEC gives the POSIX requirement that named semaphores are closed
	 * on a successful exec().
	 */
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0 && (oflag & O_CREAT) != 0 && (oflag & O_EXCL) == 0) {
		err = sem_create(name, value, mode);
		/*
		 * Another process may have created it between the open() and here.
		 * Without O_EXCL that is not an error so retry open() below.
		 */
		if (err != EOK && err != -EEXIST) {
			errno = -err;
			return SEM_FAILED;
		}

		fd = open(path, O_RDWR | O_CLOEXEC);
	}

	if (fd < 0) {
		/* errno set by open() */
		return SEM_FAILED;
	}

	sem = malloc(sizeof(*sem));
	if (sem == NULL) {
		(void)close(fd);
		errno = ENOMEM;
		return SEM_FAILED;
	}

	sem->type = smNamed;
	sem->fd = fd;

	return sem;
}


/*
 * POSIX-DEVIATION: sem_wait(), sem_timedwait() and sem_clockwait() on named semaphores:
 *
 *  - never fail with EINTR, whatever signal is delivered,
 *  - are not cancellation points (XSH 2.9.5),
 *  - cannot be killed while blocked, so a thread waiting on a semaphore that is
 *    never posted stays blocked until the process is torn down.
 *
 */
int sem_wait(sem_t *sem)
{
	int ret;

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (sem->type == smNamed) {
		/* errno set by ioctl() */
		return ioctl(sem->fd, SEM_DOWN, NULL);
	}
	else if (sem->type == smUnnamed) {
		return _sem_unnamedTake(sem, CLOCK_REALTIME, NULL);
	}
	else {
		ret = -EINVAL;
	}

	return SET_ERRNO(ret);
}


int sem_trywait(sem_t *sem)
{
	int ret;

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (sem->type == smNamed) {
		/* errno set by ioctl() */
		return ioctl(sem->fd, SEM_DOWN_TRY, NULL);
	}
	else if (sem->type == smUnnamed) {
		return _sem_unnamedTryWait(sem);
	}
	else {
		ret = -EINVAL;
	}

	return SET_ERRNO(ret);
}


int sem_timedwait(sem_t *__restrict sem, const struct timespec *__restrict abstime)
{
	return sem_clockwait(sem, CLOCK_REALTIME, abstime);
}


int sem_clockwait(sem_t *__restrict sem, clockid_t clock, const struct timespec *__restrict abstime)
{
	sem_timeout_t timeout;

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	switch (clock) {
		case CLOCK_REALTIME:
			timeout.clock = PH_CLOCK_REALTIME;
			break;

		case CLOCK_MONOTONIC:
			/* fallthrough */
		case CLOCK_MONOTONIC_RAW:
			clock = CLOCK_MONOTONIC;
			timeout.clock = PH_CLOCK_MONOTONIC;
			break;

		default:
			return SET_ERRNO(-EINVAL);
	}

	if (sem->type == smUnnamed) {
		/* Validates abstime only when it has to block, as POSIX allows */
		return _sem_unnamedTake(sem, clock, abstime);
	}

	if (sem->type != smNamed) {
		return SET_ERRNO(-EINVAL);
	}

	if (!__timespecValid(abstime)) {
		return SET_ERRNO(-EINVAL);
	}

	/* posixsrv waits for the deadline on the clock it is given */
	timeout.abstime = *abstime;

	/* errno set by ioctl() */
	return ioctl(sem->fd, SEM_DOWN_TIMEOUT, &timeout);
}


int sem_post(sem_t *sem)
{
	int ret;

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (sem->type == smNamed) {
		/* errno set by ioctl() */
		return ioctl(sem->fd, SEM_UP, NULL);
	}
	else if (sem->type == smUnnamed) {
		/* No lock, at most one write(): async-signal-safe */
		return _sem_unnamedPost(sem);
	}
	else {
		ret = -EINVAL;
	}

	return SET_ERRNO(ret);
}


int sem_getvalue(sem_t *__restrict sem, int *__restrict value)
{
	int ret;
	unsigned int v = 0;

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (sem->type == smNamed) {
		ret = ioctl(sem->fd, SEM_GETVALUE, &v);
		if (ret < 0) {
			/* errno set by ioctl() */
			return -1;
		}

		if (value != NULL) {
			*value = (int)v;
		}

		return 0;
	}
	else if (sem->type == smUnnamed) {
		if (value == NULL) {
			return 0;
		}
		return _sem_unnamedGetValue(sem, value);
	}
	else {
		ret = -EINVAL;
	}

	return SET_ERRNO(ret);
}


int sem_close(sem_t *sem)
{
	int ret = EOK;

	if (sem == NULL || sem->type != smNamed) {
		return SET_ERRNO(-EINVAL);
	}

	if (close(sem->fd) < 0) {
		ret = -errno;
	}
	free(sem);

	return SET_ERRNO(ret);
}


int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	if (pshared != 0) {
		return SET_ERRNO(-ENOSYS);
	}

	if (sem == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (value > SEM_VALUE_MAX) {
		return SET_ERRNO(-EINVAL);
	}

	sem->type = smUnnamed;

	/* POSIX: ENOSPC when a resource needed to initialise it is exhausted */
	return _sem_unnamedInit(sem, value);
}


int sem_destroy(sem_t *sem)
{
	if (sem == NULL || sem->type != smUnnamed) {
		return SET_ERRNO(-EINVAL);
	}

	return _sem_unnamedDestroy(sem);
}


int sem_unlink(const char *name)
{
	char path[SEM_PATH_SIZE];
	int ret;
	oid_t oid;
	msg_t msg = { 0 };

	if (name == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	ret = sem_path(name, path, sizeof(path));
	if (ret != EOK) {
		return SET_ERRNO(ret);
	}

	if (lookup(path, NULL, &oid) < 0) {
		return SET_ERRNO(-ENOENT);
	}

	msg.type = mtDestroy;
	msg.oid = oid;
	ret = msgSend(oid.port, &msg);
	if (ret == EOK) {
		ret = msg.o.err;
	}

	return SET_ERRNO(ret);
}
