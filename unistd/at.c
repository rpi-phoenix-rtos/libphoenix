/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * unistd - the POSIX *at() family (openat, unlinkat, fstatat, ...)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


/* fd -> canonical path, recorded by the kernel at open() time (see posix.c). */
extern int sys_fdpath(int fd, char *buf, size_t size);


/*
 * Resolve an *at() (dirfd, path) pair to a single path in `out` (must be
 * PATH_MAX bytes). An absolute path, or dirfd == AT_FDCWD, is used verbatim (the
 * underlying path-based syscall then resolves it against '/' or the cwd exactly
 * as the plain call would). Otherwise the directory fd's canonical path is
 * prepended: dirpath + '/' + path. The combined path is absolute, so the base
 * function's resolve_path() still handles '.', '..' and symlinks in `path`.
 */
static int at_path(int dirfd, const char *path, char *out)
{
	char dbuf[PATH_MAX];
	size_t dlen, plen;
	int r;

	if (path == NULL) {
		return SET_ERRNO(-EFAULT);
	}

	if ((path[0] == '/') || (dirfd == AT_FDCWD)) {
		if (strlen(path) >= PATH_MAX) {
			return SET_ERRNO(-ENAMETOOLONG);
		}
		strcpy(out, path);
		return 0;
	}

	r = sys_fdpath(dirfd, dbuf, sizeof(dbuf));
	if (r < 0) {
		/* -EBADF (no such fd) or -ENOENT (fd has no path: socket/pipe/...). */
		return SET_ERRNO(r);
	}

	dlen = strlen(dbuf);
	plen = strlen(path);
	if ((dlen + 1U + plen) >= PATH_MAX) {
		return SET_ERRNO(-ENAMETOOLONG);
	}

	memcpy(out, dbuf, dlen);
	out[dlen] = '/';
	memcpy(out + dlen + 1U, path, plen + 1U);
	return 0;
}


int openat(int dirfd, const char *path, int oflag, ...)
{
	char full[PATH_MAX];
	mode_t mode = 0;

	if ((oflag & O_CREAT) != 0) {
		va_list ap;
		va_start(ap, oflag);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return open(full, oflag, mode);
}


int unlinkat(int dirfd, const char *path, int flag)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return ((flag & AT_REMOVEDIR) != 0) ? rmdir(full) : unlink(full);
}


int fstatat(int dirfd, const char *path, struct stat *buf, int flag)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return ((flag & AT_SYMLINK_NOFOLLOW) != 0) ? lstat(full, buf) : stat(full, buf);
}


int faccessat(int dirfd, const char *path, int mode, int flag)
{
	char full[PATH_MAX];

	(void)flag; /* AT_EACCESS not distinguished (no euid/ruid split); AT_SYMLINK_NOFOLLOW rare */

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return access(full, mode);
}


int fchmodat(int dirfd, const char *path, mode_t mode, int flag)
{
	char full[PATH_MAX];

	(void)flag; /* AT_SYMLINK_NOFOLLOW (lchmod) not implemented; chmod follows links */

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return chmod(full, mode);
}


int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flag)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return ((flag & AT_SYMLINK_NOFOLLOW) != 0) ? lchown(full, owner, group) : chown(full, owner, group);
}


int mkdirat(int dirfd, const char *path, mode_t mode)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return mkdir(full, mode);
}


int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return mknod(full, mode, dev);
}


int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	char oldfull[PATH_MAX], newfull[PATH_MAX];

	if (at_path(olddirfd, oldpath, oldfull) != 0) {
		return -1;
	}
	if (at_path(newdirfd, newpath, newfull) != 0) {
		return -1;
	}

	return rename(oldfull, newfull);
}


ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz)
{
	char full[PATH_MAX];

	if (at_path(dirfd, path, full) != 0) {
		return -1;
	}

	return readlink(full, buf, bufsiz);
}


int symlinkat(const char *target, int newdirfd, const char *linkpath)
{
	char full[PATH_MAX];

	if (at_path(newdirfd, linkpath, full) != 0) {
		return -1;
	}

	return symlink(target, full);
}


int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flag)
{
	char oldfull[PATH_MAX], newfull[PATH_MAX];

	(void)flag; /* AT_SYMLINK_FOLLOW: link() behaviour on the resolved paths */

	if (at_path(olddirfd, oldpath, oldfull) != 0) {
		return -1;
	}
	if (at_path(newdirfd, newpath, newfull) != 0) {
		return -1;
	}

	return link(oldfull, newfull);
}
