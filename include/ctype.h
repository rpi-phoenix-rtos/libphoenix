/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * ctypes.h
 *
 * Copyright 2017, 2023 Phoenix Systems
 * Author: Pawel Pisarczyk, Adrian Kepka, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_CTYPES_H_
#define _LIBPHOENIX_CTYPES_H_

#include <sys/_locale_t.h>


#ifdef __cplusplus
extern "C" {
#endif


/* This function checks whether the passed character is lowercase letter. */
int islower(int c);
static inline int __islower(int c)
{
	return (((c) >= 'a' && (c) <= 'z') ? 1 : 0);
}
#define islower(c)   __islower(c)

/* This function checks whether the passed character is an uppercase letter. */
int isupper(int c);
static inline int __isupper(int c)
{
	return (((c) >= 'A' && (c) <= 'Z') ? 1 : 0);
}
#define isupper(c)   __isupper(c)


/* This function checks whether the passed character is alphabetic. */
int isalpha(int c);
static inline int __isalpha(int c)
{
	return (((islower(c) != 0) || (isupper(c) != 0)) ? 1 : 0);
}
#define isalpha(c)   __isalpha(c)


/* This function checks whether the passed character is control character. */
int iscntrl(int c);
static inline int __iscntrl(int c)
{
	return (((((c) >= 0x00) && ((c) <= 0x1f)) || (c) == 0x7f) ? 1 : 0);
}
#define iscntrl(c)   __iscntrl(c)


/* This function checks whether the passed character is decimal digit. */
int isdigit(int c);
static inline int __isdigit(int c)
{
	return (((c) >= '0' && (c) <= '9') ? 1 : 0);
}
#define isdigit(c)   __isdigit(c)


/* This function checks whether the passed character is alphanumeric. */
int isalnum(int c);
static inline int __isalnum(int c)
{
	return (((isalpha(c) != 0) || (isdigit(c) != 0)) ? 1 : 0);
}
#define isalnum(c)   __isalnum(c)


/* This function checks whether the passed character is printable. */
int isprint(int c);
static inline int __isprint(int c)
{
	return ((((c) > 0x1f) && ((c) < 0x7f)) ? 1 : 0);
}
#define isprint(c)   __isprint(c)


/* This function checks whether the passed character has graphical representation using locale. */
int isgraph(int c);
static inline int __isgraph(int c)
{
	return ((((c) != ' ') && (isprint(c) != 0)) ? 1 : 0);
}
#define isgraph(c)   __isgraph(c)


/* This function checks whether the passed character is a punctuation character. */
int ispunct(int c);
static inline int __ispunct(int c)
{
	return (((isgraph(c) != 0) && (isalnum(c) == 0)) ? 1 : 0);
}
#define ispunct(c)   __ispunct(c)


/* This function checks whether the passed character is white-space. */
int isspace(int c);
static inline int __isspace(int c)
{
	return (((c) == ' ' || (c) == '\f' || (c) == '\t' || (c) == '\n' || (c) == '\r' || (c) == '\v') ? 1 : 0);
}
#define isspace(c)   __isspace(c)


/* This function checks whether the passed character is a hexadecimal digit. */
int isxdigit(int c);
static inline int __isxdigit(int c)
{
	return (((((c) >= '0') && ((c) <= '9')) || (((c) >= 'a') && ((c) <= 'f')) || (((c) >= 'A') && ((c) <= 'F'))) ? 1 : 0);
}
#define isxdigit(c)   __isxdigit(c)


/* This function checks whether the passed character is a blank character. */
int isblank(int c);
static inline int __isblank(int c)
{
	return ((((c) == ' ') || ((c) == '\t')) ? 1 : 0);
}
#define isblank(c)   __isblank(c)


/* This function converts uppercase letters to lowercase. */
int tolower(int c);
static inline int __tolower(int c)
{
	return ((isupper(c) != 0) ? ((c) - 'A' + 'a') : c);
}
#define tolower(c)   __tolower(c)


/* This function converts lowercase letters to uppercase. */
int toupper(int c);
static inline int __toupper(int c)
{
	return ((islower(c) != 0) ? ((c) - 'a' + 'A') : c);
}
#define toupper(c)   __toupper(c)


/* This function test if character is representable as ASCII value. */
int isascii(int c);
static inline int __isascii(int c)
{
	return ((((c) < 0) || ((c) > 0x7f)) ? 0 : 1);
}
#define isascii(c)   __isascii(c)


/* The same in a given locale (see <locale.h>) */
int isalnum_l(int c, locale_t locale);
int isalpha_l(int c, locale_t locale);
int isblank_l(int c, locale_t locale);
int iscntrl_l(int c, locale_t locale);
int isdigit_l(int c, locale_t locale);
int isgraph_l(int c, locale_t locale);
int islower_l(int c, locale_t locale);
int isprint_l(int c, locale_t locale);
int ispunct_l(int c, locale_t locale);
int isspace_l(int c, locale_t locale);
int isupper_l(int c, locale_t locale);
int isxdigit_l(int c, locale_t locale);
int tolower_l(int c, locale_t locale);
int toupper_l(int c, locale_t locale);


#ifdef __cplusplus
}
#endif


#endif
