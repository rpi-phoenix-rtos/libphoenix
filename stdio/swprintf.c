/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * swprintf.c - wide-character formatted output
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>


/* Longest rendering of one conversion: a long double in %f can run to a few
 * hundred digits, and a width field can ask for more. Anything past this is
 * truncated rather than smashing the stack; the return value still reports the
 * overflow, so a caller that checks it is not misled. */
#define SWP_CONVMAX 512

/* A conversion spec is ASCII by construction (flags, digits, length modifiers
 * and the conversion character), so it is rebuilt as a narrow string and handed
 * to snprintf. Generous enough for flags + width + precision + modifier. */
#define SWP_SPECMAX 64


typedef struct {
	wchar_t *ws;
	size_t n;   /* capacity INCLUDING the terminating null, 0 = write nothing */
	size_t o;   /* wide characters produced so far, excluding the null */
	int overflowed;
} swp_out_t;


static void swp_put(swp_out_t *out, wchar_t wc)
{
	if ((out->n != 0u) && ((out->o + 1u) < out->n)) {
		out->ws[out->o] = wc;
	}
	else {
		out->overflowed = 1;
	}
	out->o++;
}


/* Widen an ASCII rendering produced by snprintf. Every byte snprintf writes for
 * a numeric or pointer conversion is in the basic execution character set, so a
 * cast is the correct widening here -- no conversion state is involved. */
static void swp_putNarrow(swp_out_t *out, const char *s)
{
	size_t i;

	for (i = 0; s[i] != '\0'; i++) {
		swp_put(out, (wchar_t)(unsigned char)s[i]);
	}
}


/* %s in a WIDE printf takes a multibyte string (C99 7.24.2.4), not a wide one:
 * it is converted as if by repeated mbrtowc(). %ls takes the wchar_t *. Getting
 * this backwards is a portability trap -- Irrlicht passes wchar_t * to %s, which
 * is why SuperTuxKart carries its own shim; this implementation follows the
 * standard rather than that usage. */
static void swp_putMbs(swp_out_t *out, const char *s, int hasPrec, int prec)
{
	mbstate_t st;
	size_t left = (size_t)-1;
	wchar_t wc;
	size_t k;

	if (s == NULL) {
		swp_putNarrow(out, "(null)");
		return;
	}

	(void)memset(&st, 0, sizeof(st));
	if (hasPrec != 0) {
		left = (prec < 0) ? 0u : (size_t)prec;
	}

	while (left != 0u) {
		k = mbrtowc(&wc, s, MB_LEN_MAX, &st);
		if ((k == 0u) || (k == (size_t)-1) || (k == (size_t)-2)) {
			break;
		}
		swp_put(out, wc);
		s += k;
		if (left != (size_t)-1) {
			left--;
		}
	}
}


static void swp_putWcs(swp_out_t *out, const wchar_t *s, int hasPrec, int prec)
{
	size_t i;
	size_t lim;

	if (s == NULL) {
		swp_putNarrow(out, "(null)");
		return;
	}

	lim = (hasPrec != 0) ? ((prec < 0) ? 0u : (size_t)prec) : (size_t)-1;
	for (i = 0; (i < lim) && (s[i] != L'\0'); i++) {
		swp_put(out, s[i]);
	}
}


/* Length modifiers, in the order the standard lists them. */
typedef enum {
	swp_none = 0,
	swp_hh,
	swp_h,
	swp_l,
	swp_ll,
	swp_j,
	swp_z,
	swp_t,
	swp_L
} swp_len_t;


int vswprintf(wchar_t *__restrict ws, size_t n, const wchar_t *__restrict format, va_list arg)
{
	swp_out_t out;
	char spec[SWP_SPECMAX];
	char conv[SWP_CONVMAX];
	const wchar_t *p = format;

	out.ws = ws;
	out.n = n;
	out.o = 0;
	out.overflowed = 0;

	if ((ws == NULL) && (n != 0u)) {
		return -1;
	}
	if (format == NULL) {
		return -1;
	}

	while (*p != L'\0') {
		size_t si = 0;
		swp_len_t len = swp_none;
		int hasPrec = 0;
		int prec = 0;
		int star;
		wchar_t c;

		if (*p != L'%') {
			swp_put(&out, *p);
			p++;
			continue;
		}

		spec[si++] = '%';
		p++;

		if (*p == L'%') {
			swp_put(&out, L'%');
			p++;
			continue;
		}

		/* flags */
		while ((*p == L'-') || (*p == L'+') || (*p == L' ') || (*p == L'#') || (*p == L'0')) {
			if (si < (SWP_SPECMAX - 8u)) {
				spec[si++] = (char)*p;
			}
			p++;
		}

		/* width: a literal run of digits, or '*' taking an int argument which is
		 * re-emitted as digits so the narrow snprintf does not need its own. */
		if (*p == L'*') {
			star = va_arg(arg, int);
			si += (size_t)snprintf(&spec[si], SWP_SPECMAX - si - 8u, "%d", star);
			p++;
		}
		else {
			while ((*p >= L'0') && (*p <= L'9')) {
				if (si < (SWP_SPECMAX - 8u)) {
					spec[si++] = (char)*p;
				}
				p++;
			}
		}

		/* precision: needed separately because %s/%ls are handled here rather
		 * than by snprintf, so the value has to be known to this code too. */
		if (*p == L'.') {
			hasPrec = 1;
			if (si < (SWP_SPECMAX - 8u)) {
				spec[si++] = '.';
			}
			p++;
			if (*p == L'*') {
				prec = va_arg(arg, int);
				si += (size_t)snprintf(&spec[si], SWP_SPECMAX - si - 8u, "%d", prec);
				p++;
			}
			else {
				prec = 0;
				while ((*p >= L'0') && (*p <= L'9')) {
					prec = (prec * 10) + (int)(*p - L'0');
					if (si < (SWP_SPECMAX - 8u)) {
						spec[si++] = (char)*p;
					}
					p++;
				}
			}
		}

		/* length modifier */
		if ((*p == L'h') && (p[1] == L'h')) {
			len = swp_hh;
			p += 2;
		}
		else if (*p == L'h') {
			len = swp_h;
			p++;
		}
		else if ((*p == L'l') && (p[1] == L'l')) {
			len = swp_ll;
			p += 2;
		}
		else if (*p == L'l') {
			len = swp_l;
			p++;
		}
		else if (*p == L'j') {
			len = swp_j;
			p++;
		}
		else if (*p == L'z') {
			len = swp_z;
			p++;
		}
		else if (*p == L't') {
			len = swp_t;
			p++;
		}
		else if (*p == L'L') {
			len = swp_L;
			p++;
		}

		c = *p;
		if (c == L'\0') {
			/* A spec running off the end of the format: emit nothing further,
			 * and report it rather than looping on the null. */
			break;
		}
		p++;

		switch (c) {
			case L'd':
			case L'i':
			case L'u':
			case L'o':
			case L'x':
			case L'X':
				/* Re-emit the length modifier so snprintf reads the same width
				 * from the vararg list that was actually passed. */
				switch (len) {
					case swp_hh: spec[si++] = 'h'; spec[si++] = 'h'; break;
					case swp_h:  spec[si++] = 'h'; break;
					case swp_l:  spec[si++] = 'l'; break;
					case swp_ll: spec[si++] = 'l'; spec[si++] = 'l'; break;
					case swp_j:  spec[si++] = 'j'; break;
					case swp_z:  spec[si++] = 'z'; break;
					case swp_t:  spec[si++] = 't'; break;
					default: break;
				}
				spec[si++] = (char)c;
				spec[si] = '\0';
				if (len == swp_ll) {
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, long long));
				}
				else if ((len == swp_l) || (len == swp_t)) {
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, long));
				}
				else if (len == swp_j) {
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, intmax_t));
				}
				else if (len == swp_z) {
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, size_t));
				}
				else {
					/* hh and h are promoted to int in the call, and snprintf
					 * re-narrows them from the same promoted value. */
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, int));
				}
				swp_putNarrow(&out, conv);
				break;

			case L'f':
			case L'F':
			case L'e':
			case L'E':
			case L'g':
			case L'G':
			case L'a':
			case L'A':
				if (len == swp_L) {
					spec[si++] = 'L';
					spec[si++] = (char)c;
					spec[si] = '\0';
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, long double));
				}
				else {
					spec[si++] = (char)c;
					spec[si] = '\0';
					(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, double));
				}
				swp_putNarrow(&out, conv);
				break;

			case L'c':
				if (len == swp_l) {
					swp_put(&out, (wchar_t)va_arg(arg, wint_t));
				}
				else {
					/* %c converts an int to unsigned char and then widens it. */
					swp_put(&out, (wchar_t)(unsigned char)va_arg(arg, int));
				}
				break;

			case L's':
				if (len == swp_l) {
					swp_putWcs(&out, va_arg(arg, wchar_t *), hasPrec, prec);
				}
				else {
					swp_putMbs(&out, va_arg(arg, char *), hasPrec, prec);
				}
				break;

			case L'p':
				spec[si++] = 'p';
				spec[si] = '\0';
				(void)snprintf(conv, sizeof(conv), spec, va_arg(arg, void *));
				swp_putNarrow(&out, conv);
				break;

			case L'n': {
				void *ptr = va_arg(arg, void *);
				/* The count so far, which is out->o even when the buffer has
				 * overflowed -- o keeps counting past the capacity. */
				switch (len) {
					case swp_hh: *(signed char *)ptr = (signed char)out.o; break;
					case swp_h:  *(short *)ptr = (short)out.o; break;
					case swp_l:  *(long *)ptr = (long)out.o; break;
					case swp_ll: *(long long *)ptr = (long long)out.o; break;
					case swp_j:  *(intmax_t *)ptr = (intmax_t)out.o; break;
					case swp_z:  *(size_t *)ptr = out.o; break;
					case swp_t:  *(ptrdiff_t *)ptr = (ptrdiff_t)out.o; break;
					default:     *(int *)ptr = (int)out.o; break;
				}
				break;
			}

			default:
				/* An unknown conversion is emitted literally, which is what the
				 * narrow printf here does and is friendlier than dropping it. */
				swp_put(&out, L'%');
				swp_put(&out, c);
				break;
		}
	}

	if (n != 0u) {
		/* Terminate whatever fitted, even on overflow: the contents are
		 * unspecified in that case, but leaving an unterminated buffer behind
		 * turns a reported error into a later out-of-bounds read. */
		ws[(out.o + 1u < n) ? out.o : (n - 1u)] = L'\0';
	}

	/* ⚠ NOT snprintf's contract. swprintf returns a NEGATIVE value if n or more
	 * wide characters were requested -- it does not report the would-be length.
	 * Code ported from the narrow family that grows a buffer on a large return
	 * will loop forever here. (C99 7.24.2.7.) */
	if ((out.overflowed != 0) || (n == 0u)) {
		return -1;
	}

	return (int)out.o;
}


int swprintf(wchar_t *__restrict ws, size_t n, const wchar_t *__restrict format, ...)
{
	va_list arg;
	int ret;

	va_start(arg, format);
	ret = vswprintf(ws, n, format, arg);
	va_end(arg);

	return ret;
}
