/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * vsprintf.c
 *
 * Copyright 2017 Phoenix Systems
 * Author: Adrian Kepka
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "stdlib.h"
#include "stdio.h"


int vasprintf(char **strp, const char *fmt, va_list ap)
{
	va_list aq;
	int len;

	/* Size the output exactly: vsnprintf(NULL, 0, ...) counts without writing
	 * (the feed routine never touches the buffer when max_len == 0). The previous
	 * implementation malloc'd a fixed 1024 bytes and vsprintf'd unbounded into it,
	 * overflowing the heap for any result longer than 1024 (e.g. glib
	 * g_strdup_printf of a long string), which corrupted an adjacent chunk and
	 * crashed a later malloc. */
	va_copy(aq, ap);
	len = vsnprintf(NULL, 0, fmt, aq);
	va_end(aq);

	if (len < 0) {
		*strp = NULL;
		return -1;
	}

	*strp = malloc((size_t)len + 1);
	if (*strp == NULL)
		return -1;

	return vsnprintf(*strp, (size_t)len + 1, fmt, ap);
}


int asprintf(char **strp, const char *format, ...)
{
	va_list ap;
	int retVal;

	va_start(ap, format);
	retVal = vasprintf(strp, format, ap);
	va_end(ap);

	return retVal;
}
