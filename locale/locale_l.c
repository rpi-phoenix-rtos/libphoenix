/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * The *_l() functions (POSIX.1-2008, and strto*_l() from BSD/glibc): every
 * locale object is the C/POSIX locale (see newlocale.c), which is also the
 * global one, so each is its plain counterpart.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <ctype.h>
#include <langinfo.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>


#define LOCALE_CTYPE_L(name) \
	int name##_l(int c, locale_t locale) \
	{ \
		(void)locale; \
		return name(c); \
	}

LOCALE_CTYPE_L(isalnum)
LOCALE_CTYPE_L(isalpha)
LOCALE_CTYPE_L(isblank)
LOCALE_CTYPE_L(iscntrl)
LOCALE_CTYPE_L(isdigit)
LOCALE_CTYPE_L(isgraph)
LOCALE_CTYPE_L(islower)
LOCALE_CTYPE_L(isprint)
LOCALE_CTYPE_L(ispunct)
LOCALE_CTYPE_L(isspace)
LOCALE_CTYPE_L(isupper)
LOCALE_CTYPE_L(isxdigit)
LOCALE_CTYPE_L(tolower)
LOCALE_CTYPE_L(toupper)


#define LOCALE_WCTYPE_L(type, name) \
	type name##_l(wint_t wc, locale_t locale) \
	{ \
		(void)locale; \
		return name(wc); \
	}

LOCALE_WCTYPE_L(int, iswalnum)
LOCALE_WCTYPE_L(int, iswalpha)
LOCALE_WCTYPE_L(int, iswblank)
LOCALE_WCTYPE_L(int, iswcntrl)
LOCALE_WCTYPE_L(int, iswdigit)
LOCALE_WCTYPE_L(int, iswgraph)
LOCALE_WCTYPE_L(int, iswlower)
LOCALE_WCTYPE_L(int, iswprint)
LOCALE_WCTYPE_L(int, iswpunct)
LOCALE_WCTYPE_L(int, iswspace)
LOCALE_WCTYPE_L(int, iswupper)
LOCALE_WCTYPE_L(int, iswxdigit)
LOCALE_WCTYPE_L(wint_t, towlower)
LOCALE_WCTYPE_L(wint_t, towupper)


wctype_t wctype_l(const char *property, locale_t locale)
{
	(void)locale;
	return wctype(property);
}


int iswctype_l(wint_t wc, wctype_t desc, locale_t locale)
{
	(void)locale;
	return iswctype(wc, desc);
}


wctrans_t wctrans_l(const char *property, locale_t locale)
{
	(void)locale;
	return wctrans(property);
}


wint_t towctrans_l(wint_t wc, wctrans_t desc, locale_t locale)
{
	(void)locale;
	return towctrans(wc, desc);
}


int strcoll_l(const char *str1, const char *str2, locale_t locale)
{
	(void)locale;
	return strcoll(str1, str2);
}


size_t strxfrm_l(char *dest, const char *src, size_t n, locale_t locale)
{
	(void)locale;
	return strxfrm(dest, src, n);
}


char *strerror_l(int errnum, locale_t locale)
{
	(void)locale;
	return strerror(errnum);
}


int strcasecmp_l(const char *s1, const char *s2, locale_t locale)
{
	(void)locale;
	return strcasecmp(s1, s2);
}


int strncasecmp_l(const char *s1, const char *s2, size_t n, locale_t locale)
{
	(void)locale;
	return strncasecmp(s1, s2, n);
}


int wcscoll_l(const wchar_t *ws1, const wchar_t *ws2, locale_t locale)
{
	(void)locale;
	return wcscoll(ws1, ws2);
}


size_t strftime_l(char *__restrict s, size_t maxsize, const char *__restrict format, const struct tm *__restrict timeptr,
		locale_t locale)
{
	(void)locale;
	return strftime(s, maxsize, format, timeptr);
}


char *nl_langinfo_l(nl_item item, locale_t locale)
{
	(void)locale;
	return nl_langinfo(item);
}


float strtof_l(const char *__restrict str, char **__restrict endptr, locale_t locale)
{
	(void)locale;
	return strtof(str, endptr);
}


double strtod_l(const char *__restrict str, char **__restrict endptr, locale_t locale)
{
	(void)locale;
	return strtod(str, endptr);
}


long double strtold_l(const char *__restrict str, char **__restrict endptr, locale_t locale)
{
	(void)locale;
	return strtold(str, endptr);
}


long int strtol_l(const char *nptr, char **endptr, int base, locale_t locale)
{
	(void)locale;
	return strtol(nptr, endptr, base);
}


unsigned long int strtoul_l(const char *nptr, char **endptr, int base, locale_t locale)
{
	(void)locale;
	return strtoul(nptr, endptr, base);
}


long long int strtoll_l(const char *nptr, char **endptr, int base, locale_t locale)
{
	(void)locale;
	return strtoll(nptr, endptr, base);
}


unsigned long long int strtoull_l(const char *nptr, char **endptr, int base, locale_t locale)
{
	(void)locale;
	return strtoull(nptr, endptr, base);
}
