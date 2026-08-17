/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/statfs.h
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_SYS_STATFS_H_
#define _LIBPHOENIX_SYS_STATFS_H_

#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifndef __fsid_t_defined
#define __fsid_t_defined
typedef struct {
	int __val[2];
} fsid_t;
#endif


/* Linux-compatible struct statfs. Phoenix has no per-filesystem "magic" type, so
 * statfs() reports f_type as 0 (unknown); all other fields are mapped from the
 * (working) statvfs syscall. Provided so portable software that expects the BSD/
 * Linux statfs()/<sys/vfs.h> interface (e.g. GNU coreutils `stat`) builds. */
struct statfs {
	long f_type;         /* filesystem type (0 = unknown on Phoenix) */
	long f_bsize;        /* optimal transfer block size */
	fsblkcnt_t f_blocks; /* total data blocks in filesystem */
	fsblkcnt_t f_bfree;  /* free blocks */
	fsblkcnt_t f_bavail; /* free blocks available to unprivileged user */
	fsfilcnt_t f_files;  /* total file nodes */
	fsfilcnt_t f_ffree;  /* free file nodes */
	fsid_t f_fsid;       /* filesystem id */
	long f_namelen;      /* maximum length of filenames */
	long f_frsize;       /* fragment size */
	long f_flags;        /* mount flags */
	long f_spare[4];
};


extern int statfs(const char *path, struct statfs *buf);
extern int fstatfs(int fd, struct statfs *buf);


#ifdef __cplusplus
}
#endif


#endif
