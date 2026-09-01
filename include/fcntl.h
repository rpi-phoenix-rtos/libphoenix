/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * fcntl.h
 *
 * Copyright 2017, 2018, 2024 Phoenix Systems
 * Author: Aleksander Kaminski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_FCNTL_H_
#define _LIBPHOENIX_FCNTL_H_


#include <sys/types.h>

#include <phoenix/posix-fcntl.h>


#ifdef __cplusplus
extern "C" {
#endif


int fcntl(int fildes, int cmd, ...);


int open(const char *path, int oflag, ...);


int creat(const char *pathname, mode_t mode);


/* Directory-fd base + flags for the *at() family (Linux/glibc-compatible values). */
#define AT_FDCWD            (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#define AT_REMOVEDIR        0x200
#define AT_EACCESS          0x200
#define AT_SYMLINK_FOLLOW   0x400
#define AT_EMPTY_PATH       0x1000


int openat(int dirfd, const char *path, int oflag, ...);


enum {
	LOCK_SH = 1,
	LOCK_EX = 2,
	LOCK_NB = 4,
	LOCK_UN = 8,
};


/* F_RDLCK/F_UNLCK/F_WRLCK and struct flock come from <phoenix/posix-fcntl.h>
 * (single ABI-shared definition with the kernel). */


#ifdef __cplusplus
}
#endif


#endif
