/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * uchar.h conversions for the C/POSIX locale (see <uchar.h>)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <uchar.h>


/* One byte to one code point of the same value; the return values of mbrtowc() */
static size_t uchar_toCode(unsigned long *code, const char *s, size_t n)
{
	if (s == NULL) {
		return 0; /* stateless: already in the initial state */
	}
	if (n == 0u) {
		return (size_t)-2;
	}

	*code = (unsigned char)*s;

	return (*code == 0u) ? 0u : 1u;
}


static size_t uchar_fromCode(char *s, unsigned long code)
{
	if (s == NULL) {
		return 1; /* as for a null character */
	}
	if (code > 0xffu) {
		errno = EILSEQ;
		return (size_t)-1;
	}

	*s = (char)code;

	return 1;
}


size_t mbrtoc16(char16_t *__restrict pc16, const char *__restrict s, size_t n, mbstate_t *__restrict ps)
{
	unsigned long code = 0;
	size_t ret = uchar_toCode(&code, s, n);

	(void)ps;
	if ((pc16 != NULL) && (s != NULL) && (n != 0u)) {
		*pc16 = (char16_t)code;
	}

	return ret;
}


size_t c16rtomb(char *__restrict s, char16_t c16, mbstate_t *__restrict ps)
{
	(void)ps;
	return uchar_fromCode(s, c16);
}


size_t mbrtoc32(char32_t *__restrict pc32, const char *__restrict s, size_t n, mbstate_t *__restrict ps)
{
	unsigned long code = 0;
	size_t ret = uchar_toCode(&code, s, n);

	(void)ps;
	if ((pc32 != NULL) && (s != NULL) && (n != 0u)) {
		*pc32 = (char32_t)code;
	}

	return ret;
}


size_t c32rtomb(char *__restrict s, char32_t c32, mbstate_t *__restrict ps)
{
	(void)ps;
	return uchar_fromCode(s, c32);
}
