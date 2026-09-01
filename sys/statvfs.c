/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * statvfs
 *
 * Copyright 2022, 2025 Phoenix Systems
 * Author: Lukasz Kosinski, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/file.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/statvfs.h>


extern int sys_statvfs(const char *path, int fd, struct statvfs *buf);


int statvfs(const char *path, struct statvfs *buf)
{
	char *canonical = resolve_path(path, NULL, 1, 0);
	if (canonical == NULL) {
		/* errno set by resolve_path() */
		return -1;
	}
	int res = sys_statvfs(canonical, -1, buf);

	/* POSIX: a pathname with a trailing '/' must name a directory. resolve_path()
	 * strips trailing slashes, so enforce it here (root "/" excluded by len > 1). */
	if (res == 0) {
		size_t len = strlen(path);
		if ((len > 1) && (path[len - 1] == '/')) {
			struct stat st;
			if ((stat(canonical, &st) == 0) && (S_ISDIR(st.st_mode) == 0)) {
				res = -ENOTDIR;
			}
		}
	}

	free(canonical);

	return set_errno(res);
}


int fstatvfs(int fildes, struct statvfs *buf)
{
	/* A negative fd is never valid; guard it here because sys_statvfs() treats
	 * fd == -1 as "use the path argument" (see statvfs() above), which would
	 * otherwise mis-handle fstatvfs(-1) instead of failing with EBADF. */
	if (fildes < 0) {
		return set_errno(-EBADF);
	}

	int res = sys_statvfs(NULL, fildes, buf);

	return set_errno(res);
}
