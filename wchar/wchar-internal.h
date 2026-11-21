/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal interface between setlocale() and the multibyte conversions
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_WCHAR_H_
#define _LIBPHOENIX_INTERNAL_WCHAR_H_


/* Selects the character encoding of LC_CTYPE: UTF-8 if utf8 != 0, otherwise
 * the single-byte C/POSIX encoding. Called by setlocale() only. The setting
 * lives with the conversions rather than with setlocale() so that a program
 * that converts (printf("%ls") does) does not link setlocale() and getenv(). */
void _wchar_setUtf8(int utf8);


#endif
