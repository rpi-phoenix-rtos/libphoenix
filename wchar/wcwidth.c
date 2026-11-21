/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * wcwidth(), wcswidth()
 *
 * A file of its own so that the Unicode width table (8.5 kB) is linked only
 * into programs that ask for column widths.
 *
 * Copyright 2022, 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>


/* a run of code points: first code point in bits 2-22, width + 1 in bits 0-1 */
#define WCWIDTH_RUN(first, width) ((((uint32_t)(first)) << 2) | (uint32_t)((width) + 1))

#include "wcwidth-table.h"


/* Width of a code point in a UTF-8 locale: the run containing it, found by
 * binary search for the last run that starts at or before it */
static int wcwidth_unicode(unsigned long code)
{
	size_t lo = 0, hi = sizeof(wcwidth_runs) / sizeof(wcwidth_runs[0]), mid;

	if (code > 0x10ffffUL) {
		return -1;
	}

	/* wcwidth_runs[0] starts at U+0000, so the run is wcwidth_runs[lo] */
	while ((hi - lo) > 1u) {
		mid = lo + ((hi - lo) / 2u);
		if ((unsigned long)(wcwidth_runs[mid] >> 2) <= code) {
			lo = mid;
		}
		else {
			hi = mid;
		}
	}

	return (int)(wcwidth_runs[lo] & 0x3u) - 1;
}


int wcwidth(wchar_t wc)
{
	unsigned long code = (unsigned long)wc;

	if (MB_CUR_MAX != 1u) {
		return wcwidth_unicode(code);
	}

	/* C/POSIX locale: NUL is 0 columns, the C0/C1 control ranges are not
	 * printable, and everything else is treated as a single column. */
	if (code == 0UL) {
		return 0;
	}
	if ((code < 0x20UL) || ((code >= 0x7fUL) && (code < 0xa0UL))) {
		return -1;
	}

	return 1;
}


int wcswidth(const wchar_t *pwcs, size_t n)
{
	int w = 0, cw;

	while ((n-- > 0u) && (*pwcs != L'\0')) {
		cw = wcwidth(*pwcs++);
		if (cw < 0) {
			return -1;
		}
		w += cw;
	}

	return w;
}
