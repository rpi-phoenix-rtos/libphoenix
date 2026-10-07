/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * math - C99/POSIX functions the phoenix libm lacked: gamma family, exp10,
 *        remainder/drem, logb/ilogb, scalb/significand (+ float variants).
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <math.h>
#include <limits.h>
#include "common.h"
#include "msun/msun.h"


/* tgamma, lgamma_r and remainder are the FreeBSD msun implementations (msun/),
 * with C99 errno reporting added here. */


/* The sign of gamma(x) after lgamma() (XSI); <math.h> names it signgam. */
int __signgam;


#ifndef FP_ILOGB0
#define FP_ILOGB0 (-INT_MAX)
#endif
#ifndef FP_ILOGBNAN
#define FP_ILOGBNAN INT_MAX
#endif


double tgamma(double x)
{
	return math_check1(__msun_tgamma(x), x);
}


double lgamma_r(double x, int *signp)
{
	return math_check1(__msun_lgamma_r(x, signp), x);
}


double lgamma(double x)
{
	return lgamma_r(x, &signgam);
}


double exp10(double x)
{
	return pow(10.0, x);
}


double remainder(double x, double y)
{
	return math_check2(__msun_remainder(x, y), x, y);
}


double drem(double x, double y)
{
	return remainder(x, y);
}


/* radix-2 exponent as a floating value: x = m * 2^logb(x), 1 <= |m| < 2. */
double logb(double x)
{
	int e;

	if (isnan(x)) {
		return x;
	}
	if (isinf(x)) {
		return INFINITY;
	}
	if (x == 0.0) {
		return -INFINITY;
	}

	(void)frexp(x, &e); /* x = m * 2^e, 0.5 <= |m| < 1 */
	return (double)(e - 1);
}


int ilogb(double x)
{
	int e;

	if (x == 0.0) {
		return FP_ILOGB0;
	}
	if (isnan(x)) {
		return FP_ILOGBNAN;
	}
	if (isinf(x)) {
		return INT_MAX;
	}

	(void)frexp(x, &e);
	return e - 1;
}


/* obsolete BSD: scalb(x, n) = x * 2^n */
double scalb(double x, double n)
{
	return scalbn(x, (int)n);
}


/* obsolete BSD: significand(x) = mantissa in [1, 2) */
double significand(double x)
{
	if (x == 0.0 || isinf(x) || isnan(x)) {
		return x;
	}
	return scalbn(x, -ilogb(x));
}


/* --- float variants (compute in double; math.h declares several of these) --- */

float tgammaf(float x)
{
	return math_check1f((float)tgamma((double)x), x);
}


float lgammaf_r(float x, int *signp)
{
	return (float)lgamma_r((double)x, signp);
}


float lgammaf(float x)
{
	return (float)lgamma((double)x);
}


float exp10f(float x)
{
	return math_check1f((float)exp10((double)x), x);
}


float remainderf(float x, float y)
{
	return (float)remainder((double)x, (double)y);
}


float dremf(float x, float y)
{
	return (float)remainder((double)x, (double)y);
}


float logbf(float x)
{
	return (float)logb((double)x);
}


int ilogbf(float x)
{
	return ilogb((double)x);
}
