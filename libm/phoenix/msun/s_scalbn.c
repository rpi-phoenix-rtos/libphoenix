/*
 * Phoenix-RTOS libphoenix: imported from FreeBSD lib/msun/src/s_scalbn.c
 * (freebsd-src commit 77a7a48a1cb003831ff1d4342b1974fc7f79381e). Local changes:
 * - removed the long double __weak_reference aliases
 * - scalbn renamed to __msun_scalbn; the public function, which adds C99 errno reporting, is in libm/phoenix/exp.c
 * The original copyright and licence notice follows unchanged.
 */

/*
 * Copyright (c) 2005-2020 Rich Felker, et al.
 *
 * SPDX-License-Identifier: MIT
 *
 * Please see https://git.musl-libc.org/cgit/musl/tree/COPYRIGHT
 * for all contributors to musl.
 */
#include <float.h>
#include <math.h>
#include <stdint.h>

double __msun_scalbn(double x, int n)
{
	union {double f; uint64_t i;} u;
	double_t y = x;

	if (n > 1023) {
		y *= 0x1p1023;
		n -= 1023;
		if (n > 1023) {
			y *= 0x1p1023;
			n -= 1023;
			if (n > 1023)
				n = 1023;
		}
	} else if (n < -1022) {
		/* make sure final n < -53 to avoid double
		   rounding in the subnormal range */
		y *= 0x1p-1022 * 0x1p53;
		n += 1022 - 53;
		if (n < -1022) {
			y *= 0x1p-1022 * 0x1p53;
			n += 1022 - 53;
			if (n < -1022)
				n = -1022;
		}
	}
	u.i = (uint64_t)(0x3ff+n)<<52;
	x = y * u.f;
	return x;
}

