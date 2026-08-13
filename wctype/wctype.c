/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wctype.c — C/POSIX-locale wide-character classification + case mapping.
 *
 * Phoenix is a single-byte (C/POSIX locale) libc: wide characters outside the
 * ASCII range have no locale class here, so classification defers to the
 * ASCII/C-locale rules (self-contained, not dependent on <ctype.h> macros).
 * Sufficient for portable software that requires <wctype.h> (e.g. GNU bash's
 * HANDLE_MULTIBYTE path).
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <wctype.h>
#include <string.h>


int iswdigit(wint_t wc)
{
	return ((wc >= L'0') && (wc <= L'9')) ? 1 : 0;
}


int iswupper(wint_t wc)
{
	return ((wc >= L'A') && (wc <= L'Z')) ? 1 : 0;
}


int iswlower(wint_t wc)
{
	return ((wc >= L'a') && (wc <= L'z')) ? 1 : 0;
}


int iswalpha(wint_t wc)
{
	return (iswupper(wc) != 0 || iswlower(wc) != 0) ? 1 : 0;
}


int iswalnum(wint_t wc)
{
	return (iswalpha(wc) != 0 || iswdigit(wc) != 0) ? 1 : 0;
}


int iswspace(wint_t wc)
{
	return (wc == L' ' || wc == L'\t' || wc == L'\n' || wc == L'\v' || wc == L'\f' || wc == L'\r') ? 1 : 0;
}


int iswblank(wint_t wc)
{
	return (wc == L' ' || wc == L'\t') ? 1 : 0;
}


int iswcntrl(wint_t wc)
{
	return ((wc >= 0 && wc < 0x20) || wc == 0x7f) ? 1 : 0;
}


int iswprint(wint_t wc)
{
	return (wc >= 0x20 && wc < 0x7f) ? 1 : 0;
}


int iswgraph(wint_t wc)
{
	return (wc > 0x20 && wc < 0x7f) ? 1 : 0;
}


int iswpunct(wint_t wc)
{
	return (iswgraph(wc) != 0 && iswalnum(wc) == 0) ? 1 : 0;
}


int iswxdigit(wint_t wc)
{
	return (iswdigit(wc) != 0 || (wc >= L'a' && wc <= L'f') || (wc >= L'A' && wc <= L'F')) ? 1 : 0;
}


/* Class tokens returned by wctype() and consumed by iswctype(). */
enum { WCT_NONE = 0, WCT_ALNUM, WCT_ALPHA, WCT_BLANK, WCT_CNTRL, WCT_DIGIT,
	WCT_GRAPH, WCT_LOWER, WCT_PRINT, WCT_PUNCT, WCT_SPACE, WCT_UPPER, WCT_XDIGIT };


wctype_t wctype(const char *property)
{
	if (property == NULL) {
		return WCT_NONE;
	}
	if (strcmp(property, "alnum") == 0) return WCT_ALNUM;
	if (strcmp(property, "alpha") == 0) return WCT_ALPHA;
	if (strcmp(property, "blank") == 0) return WCT_BLANK;
	if (strcmp(property, "cntrl") == 0) return WCT_CNTRL;
	if (strcmp(property, "digit") == 0) return WCT_DIGIT;
	if (strcmp(property, "graph") == 0) return WCT_GRAPH;
	if (strcmp(property, "lower") == 0) return WCT_LOWER;
	if (strcmp(property, "print") == 0) return WCT_PRINT;
	if (strcmp(property, "punct") == 0) return WCT_PUNCT;
	if (strcmp(property, "space") == 0) return WCT_SPACE;
	if (strcmp(property, "upper") == 0) return WCT_UPPER;
	if (strcmp(property, "xdigit") == 0) return WCT_XDIGIT;
	return WCT_NONE;
}


int iswctype(wint_t wc, wctype_t desc)
{
	switch (desc) {
		case WCT_ALNUM: return iswalnum(wc);
		case WCT_ALPHA: return iswalpha(wc);
		case WCT_BLANK: return iswblank(wc);
		case WCT_CNTRL: return iswcntrl(wc);
		case WCT_DIGIT: return iswdigit(wc);
		case WCT_GRAPH: return iswgraph(wc);
		case WCT_LOWER: return iswlower(wc);
		case WCT_PRINT: return iswprint(wc);
		case WCT_PUNCT: return iswpunct(wc);
		case WCT_SPACE: return iswspace(wc);
		case WCT_UPPER: return iswupper(wc);
		case WCT_XDIGIT: return iswxdigit(wc);
		default: return 0;
	}
}


wint_t towlower(wint_t wc)
{
	return (iswupper(wc) != 0) ? (wc + (L'a' - L'A')) : wc;
}


wint_t towupper(wint_t wc)
{
	return (iswlower(wc) != 0) ? (wc - (L'a' - L'A')) : wc;
}


/* Mapping tokens returned by wctrans() and consumed by towctrans(). */
enum { WCTR_NONE = 0, WCTR_TOLOWER, WCTR_TOUPPER };


wctrans_t wctrans(const char *property)
{
	if (property == NULL) {
		return WCTR_NONE;
	}
	if (strcmp(property, "tolower") == 0) return WCTR_TOLOWER;
	if (strcmp(property, "toupper") == 0) return WCTR_TOUPPER;
	return WCTR_NONE;
}


wint_t towctrans(wint_t wc, wctrans_t desc)
{
	switch (desc) {
		case WCTR_TOLOWER: return towlower(wc);
		case WCTR_TOUPPER: return towupper(wc);
		default: return wc;
	}
}
