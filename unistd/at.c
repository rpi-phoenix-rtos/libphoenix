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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


/* fd -> canonical path, recorded by the kernel at open() time (see posix.c). */
extern int sys_fdpath(int fd, char *buf, size_t size);


/*
 * Resolve an *at() (dirfd, path) pair to a single absolute-or-cwd-relative path.
 * Returns a malloc()'d string the caller must free(), or NULL with errno set.
 *
 * The path buffers are heap-allocated on purpose: PATH_MAX-sized stack arrays in
 * these wrappers (called from deep, allocation-heavy code like coreutils' fts)
 * add up quickly and can overflow the user stack -- keep the frames small.
 *
 * An absolute path, or dirfd == AT_FDCWD, is used verbatim (the underlying
 * path-based syscall then resolves it against '/' or the cwd exactly as the plain
 * call would). Otherwise the directory fd's canonical path is prepended:
 * dirpath + '/' + path -- an absolute result, so the base function's
 * resolve_path() still handles '.', '..' and symlinks in `path`.
 */
static char *at_resolve(int dirfd, const char *path)
{
	char *dbuf, *out;
	size_t dlen, plen;
	int r;

	if (path == NULL) {
		errno = EFAULT;
		return NULL;
	}

	if ((path[0] == '/') || (dirfd == AT_FDCWD)) {
		out = strdup(path);
		if (out == NULL) {
			errno = ENOMEM;
		}
		return out;
	}

	dbuf = malloc(PATH_MAX);
	if (dbuf == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	r = sys_fdpath(dirfd, dbuf, PATH_MAX);
	if (r < 0) {
		free(dbuf);
		/* -EBADF (no such fd) or -ENOENT (fd has no path: socket/pipe/...). */
		errno = -r;
		return NULL;
	}

	dlen = strlen(dbuf);
	plen = strlen(path);

	out = malloc(dlen + 1U + plen + 1U);
	if (out == NULL) {
		free(dbuf);
		errno = ENOMEM;
		return NULL;
	}

	memcpy(out, dbuf, dlen);
	out[dlen] = '/';
	memcpy(out + dlen + 1U, path, plen + 1U);
	free(dbuf);
	return out;
}


int openat(int dirfd, const char *path, int oflag, ...)
{
	char *full;
	mode_t mode = 0;
	int r;

	if ((oflag & O_CREAT) != 0) {
		va_list ap;
		va_start(ap, oflag);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = open(full, oflag, mode);
	free(full);
	return r;
}


int unlinkat(int dirfd, const char *path, int flag)
{
	char *full;
	int r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = ((flag & AT_REMOVEDIR) != 0) ? rmdir(full) : unlink(full);
	free(full);
	return r;
}


int fstatat(int dirfd, const char *path, struct stat *buf, int flag)
{
	char *full;
	int r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = ((flag & AT_SYMLINK_NOFOLLOW) != 0) ? lstat(full, buf) : stat(full, buf);
	free(full);
	return r;
}


int faccessat(int dirfd, const char *path, int mode, int flag)
{
	char *full;
	int r;

	(void)flag; /* AT_EACCESS not distinguished (no euid/ruid split); AT_SYMLINK_NOFOLLOW rare */

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = access(full, mode);
	free(full);
	return r;
}


int fchmodat(int dirfd, const char *path, mode_t mode, int flag)
{
	char *full;
	int r;

	(void)flag; /* AT_SYMLINK_NOFOLLOW (lchmod) not implemented; chmod follows links */

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = chmod(full, mode);
	free(full);
	return r;
}


int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flag)
{
	char *full;
	int r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = ((flag & AT_SYMLINK_NOFOLLOW) != 0) ? lchown(full, owner, group) : chown(full, owner, group);
	free(full);
	return r;
}


int mkdirat(int dirfd, const char *path, mode_t mode)
{
	char *full;
	int r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = mkdir(full, mode);
	free(full);
	return r;
}


int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev)
{
	char *full;
	int r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = mknod(full, mode, dev);
	free(full);
	return r;
}


int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath)
{
	char *oldfull, *newfull;
	int r;

	oldfull = at_resolve(olddirfd, oldpath);
	if (oldfull == NULL) {
		return -1;
	}
	newfull = at_resolve(newdirfd, newpath);
	if (newfull == NULL) {
		free(oldfull);
		return -1;
	}

	r = rename(oldfull, newfull);
	free(oldfull);
	free(newfull);
	return r;
}


ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz)
{
	char *full;
	ssize_t r;

	full = at_resolve(dirfd, path);
	if (full == NULL) {
		return -1;
	}

	r = readlink(full, buf, bufsiz);
	free(full);
	return r;
}


int symlinkat(const char *target, int newdirfd, const char *linkpath)
{
	char *full;
	int r;

	full = at_resolve(newdirfd, linkpath);
	if (full == NULL) {
		return -1;
	}

	r = symlink(target, full);
	free(full);
	return r;
}


int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flag)
{
	char *oldfull, *newfull;
	int r;

	(void)flag; /* AT_SYMLINK_FOLLOW: link() behaviour on the resolved paths */

	oldfull = at_resolve(olddirfd, oldpath);
	if (oldfull == NULL) {
		return -1;
	}
	newfull = at_resolve(newdirfd, newpath);
	if (newfull == NULL) {
		free(oldfull);
		return -1;
	}

	r = link(oldfull, newfull);
	free(oldfull);
	free(newfull);
	return r;
}
