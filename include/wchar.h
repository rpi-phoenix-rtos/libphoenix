/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wchar.h
 *
 * Copyright 2018, 2022 Phoenix Systems
 * Author: Michal Miroslaw, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_WCHAR_H_
#define _LIBPHOENIX_WCHAR_H_


#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


#define WEOF ((wchar_t)-1)


typedef int wint_t;


typedef struct
{
	int count;
	union {
		wint_t wch;
		unsigned char wchb[4];
	} value;
} mbstate_t;


extern int wcscmp(const wchar_t *ws1, const wchar_t *ws2);


/* TODO: missing function definition */
extern size_t wcslen(const wchar_t *ws);


extern wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n);
extern wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
extern wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
extern wchar_t *wcschr(const wchar_t *ws, wchar_t wc);
extern wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc);
extern int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);
extern wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
extern wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n);
extern wchar_t *wmemset(wchar_t *ws, wchar_t wc, size_t n);
extern int wmemcmp(const wchar_t *ws1, const wchar_t *ws2, size_t n);


extern size_t wcstombs(char *__restrict s, const wchar_t *__restrict pwcs, size_t n);


extern int mbsinit(const mbstate_t *ps);


extern size_t mbrtowc(wchar_t *__restrict pwc, const char *__restrict s, size_t n, mbstate_t *__restrict ps);


extern size_t wcrtomb(char *__restrict s, wchar_t wc, mbstate_t *__restrict ps);


extern size_t mbsrtowcs(wchar_t *__restrict dst, const char **__restrict src, size_t len, mbstate_t *__restrict ps);


extern size_t wcsrtombs(char *__restrict dst, const wchar_t **__restrict src, size_t len, mbstate_t *__restrict ps);


extern size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps);


extern int wcwidth(wchar_t wc);


extern int wcswidth(const wchar_t *pwcs, size_t n);


extern int wcscoll(const wchar_t *ws1, const wchar_t *ws2);


extern int wctob(wint_t c);


extern wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);


extern wchar_t *wcsdup(const wchar_t *s);


extern wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set);


extern size_t wcsspn(const wchar_t *s, const wchar_t *set);


extern size_t wcscspn(const wchar_t *s, const wchar_t *set);


extern wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);


extern wchar_t *wcstok(wchar_t *__restrict s, const wchar_t *__restrict delim, wchar_t **__restrict save);


extern long wcstol(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


extern unsigned long wcstoul(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


extern long long wcstoll(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


extern unsigned long long wcstoull(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


extern double wcstod(const wchar_t *__restrict nptr, wchar_t **__restrict endptr);


extern float wcstof(const wchar_t *__restrict nptr, wchar_t **__restrict endptr);


extern long double wcstold(const wchar_t *__restrict nptr, wchar_t **__restrict endptr);


#ifdef __cplusplus
}
#endif


#endif /* _LIBPHOENIX_WCHAR_H_ */
