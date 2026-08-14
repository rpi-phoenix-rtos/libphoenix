/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * floorl, ceill, llroundl (128-bit IEEE quad long double)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>

/* libmcs supplies float (mathf) and double (mathd) rounding functions but no
 * long-double (mathl) implementations, so the C99 *l variants declared in
 * <math.h> were undefined -> link failures for ports that use `long double`
 * (e.g. Redis: llroundl in hyperloglog.c, ceill in timeout.c).
 *
 * These use the classic "add then subtract 2^(MANT_DIG-1)" trick: for a value
 * with |x| < 2^112 the addition of 2^112 forces rounding to the nearest integer
 * (ULP at 2^112 is exactly 1), and subtracting it back recovers that integer;
 * a final +/-1 adjustment turns round-to-nearest into floor/ceil. Values with
 * |x| >= 2^112 (and NaN/Inf) are already integral and returned unchanged. This
 * is correct in the default round-to-nearest mode for all magnitudes.
 *
 * The add/subtract is sign-dependent: for x >= 0 use (x + B) - B, but for x < 0
 * use (x - B) + B. Adding B to a negative x lands the sum just below B, where
 * the ULP is 1/2 (not 1), so it would round to a half-integer; subtracting B
 * first keeps the magnitude >= B where the ULP is 1. (Verified vs glibc.) */

#define QUAD_2P112 0x1p112L /* 2^112 = 2^(LDBL_MANT_DIG-1) for IEEE binary128 */

long double floorl(long double x)
{
	long double r;

	if (x != x || __builtin_fabsl(x) >= QUAD_2P112) {
		return x;
	}

	r = (x >= 0.0L) ? ((x + QUAD_2P112) - QUAD_2P112) : ((x - QUAD_2P112) + QUAD_2P112);
	if (r > x) {
		r -= 1.0L;
	}

	return r;
}


long double ceill(long double x)
{
	long double r;

	if (x != x || __builtin_fabsl(x) >= QUAD_2P112) {
		return x;
	}

	r = (x >= 0.0L) ? ((x + QUAD_2P112) - QUAD_2P112) : ((x - QUAD_2P112) + QUAD_2P112);
	if (r < x) {
		r += 1.0L;
	}

	return r;
}


long long llroundl(long double x)
{
	long double r;

	/* round half away from zero */
	if (x >= 0.0L) {
		r = floorl(x);
		if ((x - r) >= 0.5L) {
			r += 1.0L;
		}
	}
	else {
		r = ceill(x);
		if ((r - x) >= 0.5L) {
			r -= 1.0L;
		}
	}

	return (long long)r;
}
