/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wchar functions
 *
 * Copyright 2022, 2026 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <wchar.h>
#include <inttypes.h>
#include <stdio.h> /* EOF */
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "wchar-internal.h"


/*
 * Two encodings exist, selected by setlocale(LC_CTYPE):
 *
 * - the C/POSIX locale (the default): a single-byte encoding in which every
 *   byte converts to the wchar_t of the same value and back, so 0x80-0xff are
 *   U+0080-U+00FF and nothing above U+00FF can be written;
 * - UTF-8 (any "*.UTF-8" locale): strict RFC 3629. Overlong forms, the
 *   surrogates U+D800-U+DFFF and anything above U+10FFFF are encoding errors,
 *   and a sequence is rejected at the first byte that cannot continue it
 *   (Unicode 16.0, table 3-7).
 *
 * The flag is read once per call. setlocale() is not thread-safe (POSIX), so
 * a program changes LC_CTYPE before it starts converting in other threads.
 */
static int wchar_utf8;


void _wchar_setUtf8(int utf8)
{
	wchar_utf8 = (utf8 != 0) ? 1 : 0;
}


size_t __ctype_get_mb_cur_max(void)
{
	return (wchar_utf8 != 0) ? 4u : 1u;
}


/*
 * A UTF-8 sequence in progress is kept in mbstate_t.count alone, so that
 * mbsinit() is "count == 0" for both encodings: bits 24-25 hold how many bytes
 * have been taken (1-3) and bits 16-23, 8-15 and 0-7 hold those bytes in order.
 */
#define UTF8_MAX_SEQ 4u


static unsigned int utf8_stateLen(const mbstate_t *ps)
{
	return ((unsigned int)ps->count >> 24) & 0x3u;
}


static void utf8_stateLoad(const mbstate_t *ps, unsigned char *seq)
{
	unsigned int i, len = utf8_stateLen(ps);

	for (i = 0; i < len; i++) {
		seq[i] = (unsigned char)((unsigned int)ps->count >> (16u - (8u * i)));
	}
}


static void utf8_stateStore(mbstate_t *ps, const unsigned char *seq, unsigned int len)
{
	unsigned int i, st = len << 24;

	for (i = 0; i < len; i++) {
		st |= (unsigned int)seq[i] << (16u - (8u * i));
	}
	ps->count = (int)st;
}


/* Length of the sequence that a lead byte starts; 0 for a byte that cannot
 * start one (a continuation byte, C0/C1 which could only start an overlong
 * form, and F5-FF which could only start a code point above U+10FFFF) */
static unsigned int utf8_seqLen(unsigned char lead)
{
	unsigned int len = 0;

	if (lead < 0x80u) {
		len = 1;
	}
	else if ((lead >= 0xc2u) && (lead <= 0xdfu)) {
		len = 2;
	}
	else if ((lead >= 0xe0u) && (lead <= 0xefu)) {
		len = 3;
	}
	else if ((lead >= 0xf0u) && (lead <= 0xf4u)) {
		len = 4;
	}

	return len;
}


/* Whether b can be byte number pos (1-3) of a sequence starting with lead.
 * The second byte carries the restrictions: E0 needs A0-BF (no overlong),
 * ED needs 80-9F (no surrogates), F0 needs 90-BF (no overlong), F4 needs 80-8F
 * (nothing above U+10FFFF). Every other continuation byte is 80-BF. */
static int utf8_isNext(unsigned char lead, unsigned int pos, unsigned char b)
{
	unsigned char lo = 0x80u, hi = 0xbfu;

	if (pos == 1u) {
		if (lead == 0xe0u) {
			lo = 0xa0u;
		}
		else if (lead == 0xedu) {
			hi = 0x9fu;
		}
		else if (lead == 0xf0u) {
			lo = 0x90u;
		}
		else if (lead == 0xf4u) {
			hi = 0x8fu;
		}
	}

	return ((b >= lo) && (b <= hi)) ? 1 : 0;
}


/*
 * Decodes one character from at most n bytes of s, continuing the sequence
 * pending in *ps. Returns what mbrtowc() returns: the number of bytes of s that
 * completed the character (0 for the null character), (size_t)-2 if all n bytes
 * were taken and the character is still incomplete (kept in *ps), or (size_t)-1
 * with errno EILSEQ. *ps is in the initial state after any return but -2.
 */
static size_t utf8_decode(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	unsigned char seq[UTF8_MAX_SEQ];
	unsigned int have = utf8_stateLen(ps);
	unsigned int len = 0, i;
	unsigned long code;
	size_t used = 0;

	utf8_stateLoad(ps, seq);
	if (have != 0u) {
		len = utf8_seqLen(seq[0]);
	}

	while (used < n) {
		unsigned char b = (unsigned char)s[used];

		if (have == 0u) {
			len = utf8_seqLen(b);
			if (len == 1u) {
				if (pwc != NULL) {
					*pwc = (wchar_t)b;
				}
				return (b == 0u) ? 0u : 1u;
			}
			if (len == 0u) {
				break;
			}
		}
		else if (utf8_isNext(seq[0], have, b) == 0) {
			break;
		}
		seq[have] = b;
		have++;
		used++;

		if (have == len) {
			code = (unsigned long)seq[0] & (0x7fUL >> len);
			for (i = 1; i < len; i++) {
				code = (code << 6) | ((unsigned long)seq[i] & 0x3fUL);
			}
			if (pwc != NULL) {
				*pwc = (wchar_t)code;
			}
			ps->count = 0;
			return used;
		}
	}

	if (used == n) {
		utf8_stateStore(ps, seq, have);
		return (size_t)-2;
	}

	ps->count = 0;
	errno = EILSEQ;
	return (size_t)-1;
}


/* Encodes one character into s (room for UTF8_MAX_SEQ bytes). Returns the
 * number of bytes, or 0 for a value that is not a Unicode scalar value. */
static size_t utf8_encode(char *s, wchar_t wc)
{
	unsigned long code = (unsigned long)wc;
	size_t len, i;

	if (code < 0x80UL) {
		s[0] = (char)code;
		return 1;
	}
	if (code < 0x800UL) {
		len = 2;
	}
	else if (code < 0x10000UL) {
		if ((code >= 0xd800UL) && (code <= 0xdfffUL)) {
			return 0;
		}
		len = 3;
	}
	else if (code <= 0x10ffffUL) {
		len = 4;
	}
	else {
		return 0;
	}

	for (i = len - 1u; i > 0u; i--) {
		s[i] = (char)(0x80UL | (code & 0x3fUL));
		code >>= 6;
	}
	/* lead byte: len ones, a zero, then the remaining bits */
	s[0] = (char)(((0xff00UL >> len) & 0xffUL) | code);

	return len;
}


static size_t utf8_mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps)
{
	const char *s = *src;
	mbstate_t st = *ps;
	size_t nconv = 0;
	size_t k;
	wchar_t wc;

	while ((dst == NULL) || (nconv < len)) {
		/* A null byte ends a character or fails a pending one, so no byte
		 * past the terminator is ever read, whatever the limit. */
		k = utf8_decode(&wc, s, UTF8_MAX_SEQ, &st);
		if ((k == (size_t)-1) || (k == (size_t)-2)) {
			if (dst != NULL) {
				*src = s;
			}
			errno = EILSEQ;
			return (size_t)-1;
		}
		if (k == 0u) {
			if (dst != NULL) {
				dst[nconv] = L'\0';
				*src = NULL;
				*ps = st;
			}
			return nconv;
		}
		if (dst != NULL) {
			dst[nconv] = wc;
		}
		nconv++;
		s += k;
	}

	*src = s;
	*ps = st;

	return nconv;
}


static size_t utf8_wcsrtombs(char *dst, const wchar_t **src, size_t len)
{
	const wchar_t *ws = *src;
	char mb[UTF8_MAX_SEQ];
	size_t total = 0;
	size_t k;

	for (;;) {
		if ((dst != NULL) && (total == len)) {
			break; /* full: the next character is not even examined */
		}
		if (*ws == L'\0') {
			if (dst != NULL) {
				dst[total] = '\0';
				*src = NULL;
			}
			return total;
		}
		k = utf8_encode(mb, *ws);
		if (k == 0u) {
			if (dst != NULL) {
				*src = ws;
			}
			errno = EILSEQ;
			return (size_t)-1;
		}
		if (dst != NULL) {
			if (k > (len - total)) {
				break; /* never write part of a character */
			}
			(void)memcpy(&dst[total], mb, k);
		}
		total += k;
		ws++;
	}

	*src = ws;

	return total;
}


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


/* C/POSIX locale: each byte maps 1:1 to a wchar_t (no multibyte shift state).
 * UTF-8: see utf8_decode() and utf8_encode(). */
int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* stateless encoding */
	}
	if (wchar_utf8 != 0) {
		mbstate_t st = { 0 };
		size_t k = utf8_decode(pwc, s, n, &st);

		if (k == (size_t)-2) {
			errno = EILSEQ; /* incomplete within n bytes */
			return -1;
		}
		return (k == (size_t)-1) ? -1 : (int)k;
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


/* C/POSIX locale: every non-null byte is a complete 1-byte character.
 * UTF-8: see utf8_decode() and utf8_encode(). */
int mblen(const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* stateless encoding */
	}
	if (wchar_utf8 != 0) {
		return mbtowc(NULL, s, n);
	}
	if (n == 0) {
		return -1;
	}

	return (*s == '\0') ? 0 : 1;
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t.
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n)
{
	size_t i;

	if (wchar_utf8 != 0) {
		mbstate_t st = { 0 };
		return utf8_mbsrtowcs(pwcs, &s, n, &st);
	}
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


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte.
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	size_t i;

	if (wchar_utf8 != 0) {
		return utf8_wcsrtombs(s, &pwcs, n);
	}
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


/* The C/POSIX locale is a stateless single-byte encoding, which never leaves
 * the initial state; a UTF-8 sequence in progress makes count nonzero. */
int mbsinit(const mbstate_t *ps)
{
	return (ps == NULL) || (ps->count == 0);
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t (no multibyte shift state).
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
	static mbstate_t internal;

	if (wchar_utf8 != 0) {
		if (ps == NULL) {
			ps = &internal;
		}
		if (s == NULL) {
			return utf8_decode(NULL, "", 1, ps); /* back to the initial state */
		}
		return utf8_decode(pwc, s, n, ps);
	}
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


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte.
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	size_t k;

	(void)ps; /* both encodings write without shift states */

	if (s == NULL) {
		return 1; /* as if for a wide null character */
	}
	if (wchar_utf8 != 0) {
		k = utf8_encode(s, wc);
		if (k == 0u) {
			errno = EILSEQ;
			return (size_t)-1;
		}
		return k;
	}
	if ((unsigned long)wc > 0xffUL) {
		errno = EILSEQ;
		return (size_t)-1;
	}
	*s = (char)wc;

	return 1;
}


/* C/POSIX locale: each byte maps 1:1 to a wchar_t.
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps)
{
	static mbstate_t internal;
	const char *s = *src;
	size_t i;

	if (wchar_utf8 != 0) {
		return utf8_mbsrtowcs(dst, src, len, (ps != NULL) ? ps : &internal);
	}
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


/* C/POSIX locale: each wchar_t in [0, 255] maps 1:1 to a byte.
 * UTF-8: see utf8_decode() and utf8_encode(). */
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps)
{
	const wchar_t *s = *src;
	size_t i;

	(void)ps; /* both encodings write without shift states */

	if (wchar_utf8 != 0) {
		return utf8_wcsrtombs(dst, src, len);
	}
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


size_t mbrlen(const char *__restrict s, size_t n, mbstate_t *__restrict ps)
{
	static mbstate_t internal; /* its own, not mbrtowc()'s (C17 7.29.6.3.1) */

	if (wchar_utf8 != 0) {
		return mbrtowc(NULL, s, n, (ps != NULL) ? ps : &internal);
	}
	if (s == NULL) {
		return 0;
	}
	if (n == 0) {
		return (size_t)-2; /* incomplete */
	}
	return (*s == '\0') ? 0 : 1;
}


int wcscoll(const wchar_t *ws1, const wchar_t *ws2)
{
	return wcscmp(ws1, ws2); /* C/POSIX locale: collation order == code-point order */
}


int wctob(wint_t c)
{
	/* A single byte is ASCII in UTF-8, any of 0x00-0xff in the C locale. */
	unsigned long limit = (wchar_utf8 != 0) ? 0x80UL : 0x100UL;

	return ((c != WEOF) && ((unsigned long)c < limit)) ? (int)c : EOF;
}


wint_t btowc(int c)
{
	unsigned char b = (unsigned char)c;

	/* c is a byte as unsigned char or as (signed) char */
	if ((c == EOF) || (c < -128) || (c > 255)) {
		return WEOF;
	}
	if ((wchar_utf8 != 0) && (b >= 0x80u)) {
		return WEOF; /* a byte of a multibyte sequence, not a character */
	}

	return (wint_t)b;
}


/* str == NULL asks whether the encoding has shift states: neither has. */
int wctomb(char *str, wchar_t wc)
{
	mbstate_t st = { 0 };
	size_t k;

	if (str == NULL) {
		return 0;
	}
	k = wcrtomb(str, wc, &st);

	return (k == (size_t)-1) ? -1 : (int)k;
}


wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
	while (n-- > 0) {
		if (*s == c) {
			return (wchar_t *)s;
		}
		s++;
	}
	return NULL;
}


wchar_t *wcsdup(const wchar_t *s)
{
	size_t n = wcslen(s) + 1;
	wchar_t *p = malloc(n * sizeof(wchar_t));
	if (p != NULL) {
		wmemcpy(p, s, n);
	}
	return p;
}


/* --- C99 wide-char search/tokenize (were declared-missing) --- */

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *set)
{
	for (; *s != L'\0'; ++s) {
		if (wcschr(set, *s) != NULL) {
			return (wchar_t *)s;
		}
	}
	return NULL;
}


size_t wcsspn(const wchar_t *s, const wchar_t *set)
{
	const wchar_t *p = s;
	while (*p != L'\0' && wcschr(set, *p) != NULL) {
		++p;
	}
	return (size_t)(p - s);
}


size_t wcscspn(const wchar_t *s, const wchar_t *set)
{
	const wchar_t *p = s;
	while (*p != L'\0' && wcschr(set, *p) == NULL) {
		++p;
	}
	return (size_t)(p - s);
}


wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle)
{
	size_t n;

	if (*needle == L'\0') {
		return (wchar_t *)haystack;
	}
	n = wcslen(needle);
	for (; *haystack != L'\0'; ++haystack) {
		if (*haystack == *needle && wcsncmp(haystack, needle, n) == 0) {
			return (wchar_t *)haystack;
		}
	}
	return NULL;
}


wchar_t *wcstok(wchar_t *__restrict s, const wchar_t *__restrict delim, wchar_t **__restrict save)
{
	wchar_t *tok;

	if (s == NULL) {
		s = *save;
	}
	if (s == NULL) {
		return NULL;
	}
	while (*s != L'\0' && wcschr(delim, *s) != NULL) {
		++s;
	}
	if (*s == L'\0') {
		*save = NULL;
		return NULL;
	}
	tok = s;
	while (*s != L'\0' && wcschr(delim, *s) == NULL) {
		++s;
	}
	if (*s != L'\0') {
		*s = L'\0';
		*save = s + 1;
	}
	else {
		*save = NULL;
	}
	return tok;
}


/* --- C99 wide-string -> number. The numeric grammar is ASCII, so copy the
 * ASCII-range prefix to a narrow buffer, defer to strto*, and map endptr back
 * 1:1. (A wchar >= 128 can't be part of a number, so stop there -- strto* would
 * stop at the same offset.) --- */

static size_t wcstonum_narrow(const wchar_t *ws, char *buf, size_t cap)
{
	size_t i = 0;
	while (ws[i] != L'\0' && i < (cap - 1u) && (unsigned long)ws[i] < 128uL) {
		buf[i] = (char)ws[i];
		++i;
	}
	buf[i] = '\0';
	return i;
}

#define WCSTONUM_BUFSZ 512u

#define WCSTONUM_DEF(NAME, RET, CALL, ARGS) \
	RET NAME ARGS \
	{ \
		char buf[WCSTONUM_BUFSZ]; \
		char *cend; \
		RET r; \
		wcstonum_narrow(nptr, buf, sizeof(buf)); \
		r = CALL; \
		if (endptr != NULL) { \
			*endptr = (wchar_t *)nptr + (size_t)(cend - buf); \
		} \
		return r; \
	}

WCSTONUM_DEF(wcstol,   long,               strtol(buf, &cend, base),   (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstoul,  unsigned long,      strtoul(buf, &cend, base),  (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstoll,  long long,          strtoll(buf, &cend, base),  (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstoull, unsigned long long, strtoull(buf, &cend, base), (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstoimax, intmax_t,          strtoimax(buf, &cend, base), (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstoumax, uintmax_t,         strtoumax(buf, &cend, base), (const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base))
WCSTONUM_DEF(wcstod,   double,             strtod(buf, &cend),         (const wchar_t *__restrict nptr, wchar_t **__restrict endptr))
WCSTONUM_DEF(wcstof,   float,              strtof(buf, &cend),         (const wchar_t *__restrict nptr, wchar_t **__restrict endptr))
WCSTONUM_DEF(wcstold,  long double,        strtold(buf, &cend),        (const wchar_t *__restrict nptr, wchar_t **__restrict endptr))
