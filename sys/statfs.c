/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/statfs.c
 *
 * statfs()/fstatfs() implemented as a thin mapping over the statvfs syscall.
 * Phoenix exposes no per-filesystem "magic" type, so f_type is reported as 0
 * (unknown); every other field comes from statvfs.
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/statfs.h>
#include <sys/statvfs.h>
#include <string.h>


static void statfs_from_statvfs(const struct statvfs *v, struct statfs *b)
{
	memset(b, 0, sizeof(*b));
	b->f_type = 0; /* unknown: Phoenix has no filesystem-type magic */
	b->f_bsize = (long)v->f_bsize;
	b->f_frsize = (long)v->f_frsize;
	b->f_blocks = v->f_blocks;
	b->f_bfree = v->f_bfree;
	b->f_bavail = v->f_bavail;
	b->f_files = v->f_files;
	b->f_ffree = v->f_ffree;
	b->f_namelen = (long)v->f_namemax;
	b->f_flags = (long)v->f_flag;
	b->f_fsid.__val[0] = (int)v->f_fsid;
	b->f_fsid.__val[1] = 0;
}


int statfs(const char *path, struct statfs *buf)
{
	struct statvfs v;

	if (statvfs(path, &v) != 0) {
		return -1;
	}
	statfs_from_statvfs(&v, buf);
	return 0;
}


int fstatfs(int fd, struct statfs *buf)
{
	struct statvfs v;

	if (fstatvfs(fd, &v) != 0) {
		return -1;
	}
	statfs_from_statvfs(&v, buf);
	return 0;
}
