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

#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


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


/* C/POSIX locale: every non-null byte is a complete 1-byte character. */
int mblen(const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* stateless encoding */
	}
	if (n == 0) {
		return -1;
	}

	return (*s == '\0') ? 0 : 1;
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t. */
size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	size_t i;

	if (pwcs == NULL) {
		return strlen(s);
	}
	for (i = 0; i < n; i++) {
		pwcs[i] = (wchar_t)(unsigned char)s[i];
		if (s[i] == '\0') {
			return i;
		}
	}

	return i;
}


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte. */
size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	size_t i;

	if (s == NULL) {
		for (i = 0; pwcs[i] != L'\0'; i++) {
			if ((unsigned long)pwcs[i] > 0xffUL) {
				errno = EILSEQ;
				return (size_t)-1;
			}
		}
		return i;
	}
	for (i = 0; i < n; i++) {
		if (pwcs[i] == L'\0') {
			s[i] = '\0';
			return i;
		}
		if ((unsigned long)pwcs[i] > 0xffUL) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		s[i] = (char)pwcs[i];
	}

	return i;
}


/* C/POSIX locale is a stateless single-byte encoding, so the conversion state
 * (ps) is never consulted and mbsinit() always reports the initial state. */
int mbsinit(const mbstate_t *ps)
{
	return (ps == NULL) || (ps->count == 0);
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t (no multibyte shift state). */
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	(void)ps;

	if (s == NULL) {
		return 0; /* stateless encoding: return to initial state */
	}
	if (n == 0) {
		return (size_t)-2; /* incomplete: no bytes available */
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


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte. */
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	(void)ps;

	if (s == NULL) {
		return 1; /* as if for a wide null character */
	}
	if ((unsigned long)wc > 0xffUL) {
		errno = EILSEQ;
		return (size_t)-1;
	}
	*s = (char)wc;

	return 1;
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t. */
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps)
{
	const char *s = *src;
	size_t i;

	(void)ps;

	if (dst == NULL) {
		return strlen(s);
	}
	for (i = 0; i < len; i++) {
		dst[i] = (wchar_t)(unsigned char)s[i];
		if (s[i] == '\0') {
			*src = NULL;
			return i;
		}
	}
	*src = s + i;

	return i;
}


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte. */
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps)
{
	const wchar_t *s = *src;
	size_t i;

	(void)ps;

	if (dst == NULL) {
		for (i = 0; s[i] != L'\0'; i++) {
			if ((unsigned long)s[i] > 0xffUL) {
				errno = EILSEQ;
				return (size_t)-1;
			}
		}
		return i;
	}
	for (i = 0; i < len; i++) {
		if ((unsigned long)s[i] > 0xffUL) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		if (s[i] == L'\0') {
			dst[i] = '\0';
			*src = NULL;
			return i;
		}
		dst[i] = (char)s[i];
	}
	*src = s + i;

	return i;
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


size_t wcslen(const wchar_t *ws)
{
	const wchar_t *s = ws;

	while (*s != L'\0') {
		s++;
	}

	return (size_t)(s - ws);
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
