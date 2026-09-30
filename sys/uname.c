/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/utsname.c
 *
 * Copyright 2025 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <unistd.h>
#include <sys/utsname.h>


extern int sys_uname(struct utsname *name);


int uname(struct utsname *name)
{
	int err = sys_uname(name);
	if (err < 0) {
		return SET_ERRNO(err);
	}

	/* The kernel name is empty until someone sets it: report the same name
	 * gethostname() does (which also stores it in the kernel when it can). */
	if (name->nodename[0] == '\0') {
		(void)gethostname(name->nodename, sizeof(name->nodename));
		name->nodename[sizeof(name->nodename) - 1] = '\0';
	}

	return err;
}
