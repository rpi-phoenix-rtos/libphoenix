/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * progname.c — getprogname()/setprogname() (BSD/POSIX-2024)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdlib.h>


/* Program name captured by crt0 (_startc) from argv[0]. */
extern const char *argv_progname;


const char *getprogname(void)
{
	const char *name = argv_progname;
	const char *p;

	if (name == NULL) {
		return "";
	}

	/* BSD getprogname() returns the last path component (short name). */
	for (p = name; *p != '\0'; p++) {
		if (*p == '/') {
			name = p + 1;
		}
	}

	return name;
}


void setprogname(const char *progname)
{
	argv_progname = progname;
}
