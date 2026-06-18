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


/* TODO: missing function definition */
extern size_t wcsrtombs(char *__restrict dst, const wchar_t **__restrict src, size_t len, mbstate_t *__restrict ps);


#ifdef __cplusplus
}
#endif


#endif /* _LIBPHOENIX_WCHAR_H_ */
