/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal file operations
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_FILE_H_
#define _LIBPHOENIX_INTERNAL_FILE_H_


#include <errno.h>
#include <sys/msg.h>
#include <sys/types.h>


ssize_t __safe_write(int fd, const void *buf, size_t size);


ssize_t __safe_pwrite(int fd, const void *buf, size_t size, off_t offset);


/* non-blocking variant - returns on EAGAIN */
ssize_t __safe_write_nb(int fd, const void *buf, size_t size);


/* non-blocking variant - returns on EAGAIN */
ssize_t __safe_pwrite_nb(int fd, const void *buf, size_t size, off_t offset);


ssize_t __safe_read(int fd, void *buf, size_t size);


ssize_t __safe_pread(int fd, void *buf, size_t size, off_t offset);


/* non-blocking variant - returns on EAGAIN */
ssize_t __safe_read_nb(int fd, void *buf, size_t size);


/* non-blocking variant - returns on EAGAIN */
ssize_t __safe_pread_nb(int fd, void *buf, size_t size, off_t offset);


int __safe_open(const char *path, int oflag, mode_t mode);


int __safe_close(int fd);


/*
 * lookup() and msgSend() restarted on EINTR, for the path queries behind
 * access(), stat() and path resolution. POSIX gives those no EINTR (Linux never
 * returns it), but the kernel aborts a send that a signal interrupts while no
 * server has taken the message yet -- e.g. a SIGCHLD arriving while a busy
 * filesystem server has the request queued -- so a caller that asked for
 * SA_RESTART was told "no such file". An aborted message was never seen by the
 * server (a taken one is waited for uninterruptibly), so resending it is safe.
 */
static inline int __safe_lookup(const char *name, oid_t *file, oid_t *dev)
{
	int err;

	do {
		err = lookup(name, file, dev);
	} while (err == -EINTR);

	return err;
}


static inline int __safe_msgSend(uint32_t port, msg_t *msg)
{
	int err;

	do {
		err = msgSend(port, msg);
	} while (err == -EINTR);

	return err;
}


#endif
