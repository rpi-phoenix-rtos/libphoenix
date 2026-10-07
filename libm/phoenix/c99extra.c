/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * log1p, expm1, asinh, acosh, atanh, nextafter, nexttoward
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>

#include "common.h"
#include "msun/msun.h"


/* log1p, expm1, asinh, acosh and atanh are the FreeBSD msun implementations
 * (msun/), with C99 errno reporting added here. CPython's configure requires
 * all five ("requires C99 compatible libm"). */

double log1p(double x)
{
	return math_check1(__msun_log1p(x), x);
}


double expm1(double x)
{
	return math_check1(__msun_expm1(x), x);
}


double asinh(double x)
{
	return math_check1(__msun_asinh(x), x);
}


double acosh(double x)
{
	return math_check1(__msun_acosh(x), x);
}


double atanh(double x)
{
	return math_check1(__msun_atanh(x), x);
}


float log1pf(float x)
{
	return (float)log1p((double)x);
}


float expm1f(float x)
{
	return math_check1f((float)expm1((double)x), x);
}


/* The float hyperbolic inverses (declared in <math.h>, never defined here): through the double
 * versions above, as coshf()/sinhf()/tanhf() do (hyper.c), which also carries their domain
 * handling over (acoshf(x < 1) and atanhf(|x| > 1) are NaN, atanhf(+-1) is +-inf). WebKit's
 * ANGLE folds GLSL constant expressions with them. */
float asinhf(float x)
{
	return (float)asinh((double)x);
}


float acoshf(float x)
{
	return (float)acosh((double)x);
}


float atanhf(float x)
{
	return (float)atanh((double)x);
}


/* --- nextafter / nexttoward (C99). math module (math.nextafter/math.ulp) needs
 * nextafter; libphoenix lacked it. Step x by one ULP toward y via the IEEE-754
 * bit representation (musl approach). Verified vs glibc. --- */

#include <stdint.h>

double nextafter(double x, double y)
{
	union { double f; uint64_t i; } ux = { x }, uy = { y };
	uint64_t ax, ay;

	if (isnan(x) || isnan(y)) {
		return x + y;
	}
	if (ux.i == uy.i) {
		return y;
	}
	ax = ux.i & 0x7fffffffffffffffULL;
	ay = uy.i & 0x7fffffffffffffffULL;
	if (ax == 0) {
		if (ay == 0) {
			return y;
		}
		ux.i = (uy.i & 0x8000000000000000ULL) | 1; /* smallest subnormal, sign of y */
	}
	else if (ax > ay || (((ux.i ^ uy.i) & 0x8000000000000000ULL) != 0)) {
		ux.i--; /* |x|>|y| or opposite signs -> decrease magnitude */
	}
	else {
		ux.i++;
	}
	return ux.f;
}


double nexttoward(double x, long double y)
{
	if (isnan(x) || isnan(y)) {
		return x + (double)y;
	}
	if ((long double)x == y) {
		return (double)y;
	}
	return nextafter(x, (y > (long double)x) ? (double)INFINITY : -(double)INFINITY);
}
