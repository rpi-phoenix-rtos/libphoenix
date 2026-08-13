/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wctype.h
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_WCTYPE_H_
#define _LIBPHOENIX_WCTYPE_H_

#include <wchar.h> /* wint_t, wchar_t, WEOF */


#ifdef __cplusplus
extern "C" {
#endif


/* wctype_t / wctrans_t are opaque tokens returned by wctype()/wctrans() and
 * consumed by iswctype()/towctrans(). Phoenix operates in the C/POSIX locale,
 * so classification covers the single-byte (ASCII/C-locale) range. */
typedef int wctype_t;
typedef int wctrans_t;


/* Character classification (single-argument). */
int iswalnum(wint_t wc);
int iswalpha(wint_t wc);
int iswblank(wint_t wc);
int iswcntrl(wint_t wc);
int iswdigit(wint_t wc);
int iswgraph(wint_t wc);
int iswlower(wint_t wc);
int iswprint(wint_t wc);
int iswpunct(wint_t wc);
int iswspace(wint_t wc);
int iswupper(wint_t wc);
int iswxdigit(wint_t wc);

/* Extensible classification. */
wctype_t wctype(const char *property);
int iswctype(wint_t wc, wctype_t desc);

/* Case mapping. */
wint_t towlower(wint_t wc);
wint_t towupper(wint_t wc);

/* Extensible mapping. */
wctrans_t wctrans(const char *property);
wint_t towctrans(wint_t wc, wctrans_t desc);


#ifdef __cplusplus
}
#endif


#endif /* _LIBPHOENIX_WCTYPE_H_ */
