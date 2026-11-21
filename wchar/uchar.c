/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * uchar.h conversions (see <uchar.h>)
 *
 * char32_t and wchar_t are both UTF-32 here, so the char32_t conversions are
 * mbrtowc() and wcrtomb(). A char16_t is one UTF-16 code unit: a character
 * above U+FFFF is a surrogate pair, which only UTF-8 can produce or accept.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <uchar.h>
#include <wchar.h>


/* Half of a surrogate pair carried between calls in mbstate_t.count: the
 * flag is outside the bits mbrtowc() uses for a sequence in progress. */
#define UCHAR_HALF_PENDING (1 << 30)


static int uchar_halfPending(const mbstate_t *ps)
{
	return ((ps->count & UCHAR_HALF_PENDING) != 0) ? 1 : 0;
}


size_t mbrtoc16(char16_t *__restrict pc16, const char *__restrict s, size_t n, mbstate_t *__restrict ps)
{
	static mbstate_t internal;
	unsigned long code;
	wchar_t wc = 0;
	size_t ret;

	if (ps == NULL) {
		ps = &internal;
	}
	if (s == NULL) {
		pc16 = NULL;
		s = "";
		n = 1;
	}

	/* the low surrogate of the character completed by the previous call */
	if (uchar_halfPending(ps) != 0) {
		if (pc16 != NULL) {
			*pc16 = (char16_t)(ps->count & 0xffff);
		}
		ps->count = 0;
		return (size_t)-3;
	}

	ret = mbrtowc(&wc, s, n, ps);
	if ((ret != (size_t)-1) && (ret != (size_t)-2)) {
		code = (unsigned long)wc;
		if (code > 0xffffUL) {
			code -= 0x10000UL;
			ps->count = UCHAR_HALF_PENDING | (int)(0xdc00UL | (code & 0x3ffUL));
			code = 0xd800UL | (code >> 10);
		}
		if (pc16 != NULL) {
			*pc16 = (char16_t)code;
		}
	}

	return ret;
}


size_t c16rtomb(char *__restrict s, char16_t c16, mbstate_t *__restrict ps)
{
	static mbstate_t internal;
	char buf[4];
	unsigned long code = c16;
	unsigned long high;

	if (ps == NULL) {
		ps = &internal;
	}
	if (s == NULL) {
		s = buf;
		code = 0;
	}

	if (MB_CUR_MAX == 1u) {
		return wcrtomb(s, (wchar_t)code, ps); /* fails on surrogates */
	}

	if (uchar_halfPending(ps) != 0) {
		high = (unsigned long)ps->count & 0x3ffUL;
		ps->count = 0;
		if ((code < 0xdc00UL) || (code > 0xdfffUL)) {
			errno = EILSEQ; /* a high surrogate not followed by a low one */
			return (size_t)-1;
		}
		code = 0x10000UL + ((high << 10) | (code & 0x3ffUL));
	}
	else if ((code >= 0xd800UL) && (code <= 0xdbffUL)) {
		ps->count = UCHAR_HALF_PENDING | (int)code;
		return 0; /* nothing to write until the low surrogate */
	}

	/* a lone low surrogate is rejected here, as by wcrtomb() */
	return wcrtomb(s, (wchar_t)code, ps);
}


size_t mbrtoc32(char32_t *__restrict pc32, const char *__restrict s, size_t n, mbstate_t *__restrict ps)
{
	static mbstate_t internal;
	wchar_t wc = 0;
	size_t ret;

	if (ps == NULL) {
		ps = &internal;
	}
	if (s == NULL) {
		return mbrtowc(NULL, NULL, 0, ps);
	}

	ret = mbrtowc(&wc, s, n, ps);
	if ((pc32 != NULL) && (ret != (size_t)-1) && (ret != (size_t)-2)) {
		*pc32 = (char32_t)wc;
	}

	return ret;
}


size_t c32rtomb(char *__restrict s, char32_t c32, mbstate_t *__restrict ps)
{
	return wcrtomb(s, (wchar_t)c32, ps);
}
