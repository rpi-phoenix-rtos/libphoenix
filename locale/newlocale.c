/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Locale objects (POSIX.1-2008): newlocale(), duplocale(), freelocale(),
 * uselocale()
 *
 * Only the C/POSIX locale exists, so there is one locale object, which every
 * successful newlocale() and duplocale() returns and which freelocale()
 * leaves alone.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch.h>
#include <errno.h>
#include <locale.h>
#include <string.h>


struct __locale_struct {
	int unused;
};


static struct __locale_struct locale_c;


#ifdef __LIBPHOENIX_ARCH_TLS_SUPPORTED
static __thread locale_t locale_current = LC_GLOBAL_LOCALE;
#else
/* No thread-local storage: one current locale for the process, which is
 * indistinguishable while every locale is the C locale */
static locale_t locale_current = LC_GLOBAL_LOCALE;
#endif


locale_t newlocale(int categoryMask, const char *locale, locale_t base)
{
	(void)base; /* nothing to free or modify: it is locale_c, too */

	if (((categoryMask & ~LC_ALL_MASK) != 0) || (locale == NULL)) {
		errno = EINVAL;
		return (locale_t)0;
	}

	if ((locale[0] != '\0') && (strcmp(locale, "C") != 0) && (strcmp(locale, "POSIX") != 0)) {
		errno = ENOENT;
		return (locale_t)0;
	}

	return &locale_c;
}


locale_t duplocale(locale_t locobj)
{
	(void)locobj;
	return &locale_c;
}


void freelocale(locale_t locobj)
{
	(void)locobj;
}


locale_t uselocale(locale_t newloc)
{
	locale_t old = locale_current;

	if (newloc != (locale_t)0) {
		locale_current = newloc;
	}

	return old;
}
