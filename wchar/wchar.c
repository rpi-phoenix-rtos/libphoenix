/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wchar functions
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* TODO: add implementations for remaining wchar functions */
#include <wchar.h>
#include <stdlib.h>


wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n)
{
	size_t i;

	for (i = 0; (i < n) && (src[i] != L'\0'); i++) {
		dest[i] = src[i];
	}
	for (; i < n; i++) {
		dest[i] = L'\0';
	}

	return dest;
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t (no multibyte shift state). */
int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* stateless encoding */
	}
	if (n == 0) {
		return -1;
	}
	if (*s == '\0') {
		if (pwc != NULL) {
			*pwc = L'\0';
		}
		return 0;
	}
	if (pwc != NULL) {
		*pwc = (wchar_t)(unsigned char)*s;
	}

	return 1;
}


wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
	wchar_t *d = dest;

	while ((*d++ = *src++) != L'\0') {
	}

	return dest;
}


wchar_t *wcscat(wchar_t *dest, const wchar_t *src)
{
	wchar_t *d = dest;

	while (*d != L'\0') {
		d++;
	}
	while ((*d++ = *src++) != L'\0') {
	}

	return dest;
}


wchar_t *wcschr(const wchar_t *ws, wchar_t wc)
{
	for (; *ws != L'\0'; ws++) {
		if (*ws == wc) {
			return (wchar_t *)ws;
		}
	}

	return (wc == L'\0') ? (wchar_t *)ws : NULL;
}


wchar_t *wcsrchr(const wchar_t *ws, wchar_t wc)
{
	const wchar_t *last = NULL;

	for (;; ws++) {
		if (*ws == wc) {
			last = ws;
		}
		if (*ws == L'\0') {
			break;
		}
	}

	return (wchar_t *)last;
}


int wcsncmp(const wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (ws1[i] != ws2[i]) {
			return (ws1[i] < ws2[i]) ? -1 : 1;
		}
		if (ws1[i] == L'\0') {
			break;
		}
	}

	return 0;
}


wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		dest[i] = src[i];
	}

	return dest;
}


wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n)
{
	size_t i;

	if (dest <= src) {
		for (i = 0; i < n; i++) {
			dest[i] = src[i];
		}
	}
	else {
		for (i = n; i > 0; i--) {
			dest[i - 1] = src[i - 1];
		}
	}

	return dest;
}


wchar_t *wmemset(wchar_t *ws, wchar_t wc, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		ws[i] = wc;
	}

	return ws;
}


int wmemcmp(const wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		if (ws1[i] != ws2[i]) {
			return (ws1[i] < ws2[i]) ? -1 : 1;
		}
	}

	return 0;
}


int wcscmp(const wchar_t *ws1, const wchar_t *ws2)
{
	int i, ret = 1;

	for (i = 0; ws1[i] == ws2[i]; i++) {
		if (ws1[i] == L'\0') {
			ret = 0;
			break;
		}
	}

	if (ret != 0) {
		if (ws1[i] > ws2[i]) {
			ret = 1;
		}
		else {
			ret = -1;
		}
	}

	return ret;
}
