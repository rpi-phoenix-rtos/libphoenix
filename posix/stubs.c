/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * POSIX implementation - stubs
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <locale.h>
#include <string.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <grp.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>


static struct {
	char buff[6];
} common_setlocale = { .buff = "POSIX" };


char* setlocale(int category, const char* locale)
{
	if (category != LC_ALL &&
		category != LC_COLLATE &&
		category != LC_CTYPE &&
		category != LC_MONETARY &&
		category != LC_NUMERIC &&
		category != LC_TIME) {
		return NULL;
	}

	if (locale == NULL) {
		return common_setlocale.buff;
	}

	if (strcmp(locale, "POSIX") == 0 || strcmp(locale, "") == 0) {
		strcpy(common_setlocale.buff, "POSIX");
		return common_setlocale.buff;
	}
	else if (strcmp(locale, "C") == 0) {
		strcpy(common_setlocale.buff, "C");
		return common_setlocale.buff;
	}

	return NULL;
}


int fchmod(int fd, mode_t mode)
{
	return 0;
}


int fchown(int fd, uid_t owner, gid_t group)
{
	return 0;
}


int setgid(gid_t gid)
{
	return 0;
}


int setuid(uid_t uid)
{
	return 0;
}


int setegid(gid_t gid)
{
	return 0;
}


int seteuid(uid_t uid)
{
	return 0;
}


int fchdir(int fildes)
{
	return 0;
}


char *ttyname(int fildes)
{
	return NULL;
}


int ttyname_r(int fildes, char *name, size_t namesize)
{
	if (namesize > 0) {
		name[0] = '\0';
	}

	return 0;
}


int chroot(const char *path)
{
	return 0;
}


int getrlimit(int resource, struct rlimit *rlp)
{
	/* Phoenix does not enforce per-process resource limits: report every
	 * resource as unlimited. (Previously this left *rlp uninitialized while
	 * returning success, so callers read a garbage soft limit.) */
	(void)resource;
	if (rlp != NULL) {
		rlp->rlim_cur = RLIM_INFINITY;
		rlp->rlim_max = RLIM_INFINITY;
	}
	return 0;
}


int setrlimit(int resource, const struct rlimit *rlp)
{
	return 0;
}


dev_t makedev(unsigned int maj, unsigned int min)
{
	/* glibc-compatible dev_t packing (mknod ignores dev on Phoenix, so the only
	 * constraint is that makedev/major/minor agree — they were all `return 0`). */
	return ((dev_t)(maj & 0xfffu) << 8) | (dev_t)(min & 0xffu)
		| ((dev_t)(min & 0xfff00u) << 12);
}


int getrusage(int who, struct rusage *usage)
{
	return 0;
}


void sync(void)
{
}


unsigned int major(dev_t dev)
{
	return (unsigned int)((dev >> 8) & 0xfffu);
}


unsigned int minor(dev_t dev)
{
	return (unsigned int)((dev & 0xffu) | ((dev >> 12) & 0xfff00u));
}


int flock(int fd, int operation)
{
	return 0;
}


long ulimit(int __cmd, ...)
{
	return 0;
}


int wctomb(char *str, wchar_t wchar)
{
	/* C/POSIX locale: 1:1 byte mapping (mirrors wcrtomb). str==NULL queries
	 * whether encodings are state-dependent; they are not, so return 0.
	 * (Was a stub returning 0 unconditionally — encoded nothing.) */
	if (str == NULL) {
		return 0;
	}
	if ((unsigned long)wchar > 0xffUL) {
		errno = EILSEQ;
		return -1;
	}
	*str = (char)wchar;
	return 1;
}


gid_t getgid(void)
{
	return 0;
}


gid_t getegid(void)
{
	return 0;
}


int getgroups(int size, gid_t list[])
{
	return 0;
}


int setgroups(size_t size, const gid_t *list)
{
	return 0;
}


int initgroups(const char *user, gid_t group)
{
	return 0;
}


int issetugid(void)
{
	return 0;
}
