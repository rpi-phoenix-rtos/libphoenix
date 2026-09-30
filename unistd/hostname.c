/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * unistd: gethostname(), sethostname()
 *
 * Copyright 2021 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>

#include "file-internal.h"


#ifndef HOSTNAME_FILE
#define HOSTNAME_FILE "/etc/hostname"
#endif

/* Used while neither the kernel nor /etc/hostname has a name. "localhost" is the
 * one name every resolver answers, so gethostbyname(gethostname()) still works. */
#define HOSTNAME_DEFAULT "localhost"


extern int sys_gethostname(char *name, size_t namelen);
extern int sys_sethostname(const char *name, size_t namelen);


WRAP_ERRNO_DEF(int, sethostname, (const char *name, size_t namelen), (name, namelen))


static int hostname_isSpace(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}


/* Reads the first word of HOSTNAME_FILE into buf (NUL-terminated), skipping blank
 * and '#' comment lines. Returns its length, 0 if the file is absent or names no host. */
static size_t hostname_fromFile(char *buf, size_t size)
{
	char *p, *end;
	ssize_t n;
	int fd;

	fd = __safe_open(HOSTNAME_FILE, O_RDONLY, 0);
	if (fd < 0) {
		return 0;
	}
	n = __safe_read(fd, buf, size - 1);
	(void)__safe_close(fd);
	if (n <= 0) {
		return 0;
	}
	buf[n] = '\0';

	p = buf;
	for (;;) {
		while (hostname_isSpace(*p)) {
			p++;
		}
		if (*p != '#') {
			break;
		}
		p = strchr(p, '\n');
		if (p == NULL) {
			return 0;
		}
	}

	end = p;
	while ((*end != '\0') && !hostname_isSpace(*end) && (*end != '#')) {
		end++;
	}
	*end = '\0';
	memmove(buf, p, (size_t)(end - p) + 1);

	return (size_t)(end - p);
}


/*
 * The kernel starts with an empty hostname and nothing in the boot sequence sets
 * one, so an empty kernel name is resolved here: from /etc/hostname, which is then
 * stored in the kernel so that uname() and every later caller see the same name,
 * else HOSTNAME_DEFAULT (not stored, so that a root mounted later still applies).
 * An explicit sethostname() always wins, as the kernel name is then non-empty.
 */
int gethostname(char *name, size_t namelen)
{
	char buf[HOST_NAME_MAX + 1];
	size_t len;
	int err;

	err = sys_gethostname(buf, sizeof(buf));
	if (err < 0) {
		return SET_ERRNO(err);
	}
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '\0') {
		len = hostname_fromFile(buf, sizeof(buf));
		if (len > 0) {
			(void)sys_sethostname(buf, len);
		}
		else {
			(void)strcpy(buf, HOSTNAME_DEFAULT);
		}
	}

	/* POSIX: a name that does not fit is truncated, termination unspecified */
	len = strlen(buf) + 1;
	(void)memcpy(name, buf, (len < namelen) ? len : namelen);

	return 0;
}
