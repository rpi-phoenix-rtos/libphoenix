/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * exp, frexp, ldexp, log, log10, modf, ceil, floor, fmod, fabs
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include "common.h"


double frexp(double x, int *exp)
{
	if (isnan(x) != 0) {
		return NAN;
	}

	if (isinf(x) != 0) {
		return x;
	}

	if (x == 0.0) {
		return x;
	}

	conv_t *conv = (conv_t *)&x;
	*exp = 0;

	if (conv->i.exponent == 0) {
		normalizeSub(&x, exp);
	}

	*exp += conv->i.exponent - 1022;
	conv->i.exponent = 1022;

	return x;
}


float frexpf(float x, int *exp)
{
	return (float)frexp((double)x, exp);
}


double ldexp(double x, int exp)
{
	if (isnan(x) != 0) {
		return NAN;
	}

	if (x == 0.0) {
		return x;
	}

	conv_t *conv = (conv_t *)&x;
	int exponent = 0;

	if (conv->i.exponent == 0) {
		normalizeSub(&x, &exponent);
	}

	exponent += conv->i.exponent + exp;

	if (exponent > 2046) {
		errno = ERANGE;
		return conv->i.sign ? -HUGE_VAL : HUGE_VAL;
	}

	/* If result is subnormal */
	if (exponent < 0) {
		createSub(&x, exponent);
		conv->i.exponent = 0;
	}
	else {
		conv->i.exponent = exponent;
	}

	return x;
}


float ldexpf(float x, int exp)
{
	return (float)ldexp((double)x, exp);
}


/* scalbn(x,n) = x * FLT_RADIX^n; FLT_RADIX == 2 on this target, so scalbn == ldexp. */
double scalbn(double x, int n)
{
	return ldexp(x, n);
}


float scalbnf(float x, int n)
{
	return ldexpf(x, n);
}


/* scalbln takes a long exponent; ldexp saturates (inf/0) for |exp| > 2046, so clamping the
 * long into int range preserves the result for any n. */
double scalbln(double x, long n)
{
	int e = (n > (long)INT_MAX) ? INT_MAX : ((n < (long)INT_MIN) ? INT_MIN : (int)n);
	return ldexp(x, e);
}


float scalblnf(float x, long n)
{
	int e = (n > (long)INT_MAX) ? INT_MAX : ((n < (long)INT_MIN) ? INT_MIN : (int)n);
	return ldexpf(x, e);
}


double log(double x)
{
	double tmp, pow, res;
	conv_t *conv = (conv_t *)&tmp;
	int exp = 0, i;

	if (isnan(x) != 0) {
		return NAN;
	}
	else if (x < 0.0) {
		errno = EDOM;
		return NAN;
	}
	else if (x == 0.0) {
		errno = ERANGE;
		return -HUGE_VAL;
	}
	else if (x == 1.0) {
		return 0.0;
	}
	else if (isinf(x) != 0) {
		return x;
	}

	tmp = x;

	exp = conv->i.exponent - 1022;

	if (conv->i.exponent == 0) {
		normalizeSub(&tmp, &exp);
	}

	conv->i.exponent = 1022;

	tmp = (tmp - 1.0) / (tmp + 1.0);

	for (i = 1, res = 0.0, pow = tmp * tmp; i < 16; ++i) {
		res += tmp / ((2 * i) - 1);
		tmp *= pow;
	}

	return ((2.0 * res) + (exp / M_LOG2E));
}


float logf(float x)
{
	return (float)log((double)x);
}


double log2(double x)
{
	return (log(x) / M_LN2);
}


/* Uses log10(x) = ln(x) / ln(10) identity */
double log10(double x)
{
	return (log(x) / M_LN10);
}


float log10f(float x)
{
	return (float)log10((double)x);
}


float log2f(float x)
{
	return (float)log2((double)x);
}


double modf(double x, double *intpart)
{
	conv_t *conv = (conv_t *)&x;
	double tmp = x;
	int exp = conv->i.exponent - 1023;
	uint64_t m, mask = 0xfffffffffffffLL;

	if (isnan(x) != 0) {
		*intpart = NAN;
		return NAN;
	}

	if (exp > 52) {
		*intpart = x;
		return (conv->i.sign ? -0.0 : 0.0);
	}
	else if (exp < 0) {
		*intpart = conv->i.sign ? -0.0 : 0.0;
		return x;
	}

	conv->i.mantisa = conv->i.mantisa & ~(mask >> exp);
	*intpart = x;
	x = tmp;

	m = conv->i.mantisa;
	m &= mask >> exp;

	if (m == 0u) {
		return 0.0;
	}

	conv->i.mantisa = m & mask;
	normalizeSub(&x, &exp);

	conv->i.exponent = exp + 1023;

	return x;
}

float modff(float x, float *intpart)
{
	double ret, tmp;

	ret = modf(x, &tmp);
	*intpart = tmp;

	return ret;
}

/* Uses quick powering and Maclaurin series to calculate value of e^x */
double exp(double x)
{
	double res, resi, powx, e, factorial;
	int i;

	if (isnan(x) != 0) {
		return NAN;
	}

	/* Values of x greater than 709.79 will cause overflow, returning INFINITY */
	if (x > 709.79) {
		errno = ERANGE;
		return HUGE_VAL;
	}

	/* Get floor of exponent */
	x = modf(x, &e);

	/* Calculate most of the result */
	resi = quickPow(M_E, (int)e);

	/* Calculate rest of the result using Maclaurin series */
	factorial = 1.0;
	powx = x;
	res = 1.0;

	for (i = 2; i < 13; ++i) {
		if (powx == 0.0) {
			break;
		}
		res += powx / factorial;
		factorial *= i;
		powx *= x;
	}

	return (res * resi);
}


float expf(float x)
{
	return (float)exp((double)x);
}


/* Uses 2^x = e^(x * ln(2)) identity */
double exp2(double x)
{
	return exp(x * M_LN2);
}


float exp2f(float x)
{
	return (float)exp2((double)x);
}


double ceil(double x)
{
#ifdef __IEEE754_CEIL
	return __ieee754_ceil(x);
#else
	double ipart, fpart;

	if (isnan(x) != 0) {
		return NAN;
	}

	fpart = modf(x, &ipart);

	if ((x > 0.0) && ((fpart + x) != x)) {
		ipart += 1.0;
	}

	return ipart;
#endif
}


float ceilf(float x)
{
#ifdef __IEEE754_CEILF
	return __ieee754_ceilf(x);
#else
	return (float)ceil(x);
#endif
}


double floor(double x)
{
#ifdef __IEEE754_FLOOR
	return __ieee754_floor(x);
#else
	double ipart, fpart;

	if (isnan(x) != 0) {
		return NAN;
	}

	fpart = modf(x, &ipart);

	if ((x < 0.0) && ((fpart + x) != x)) {
		ipart -= 1.0;
	}

	return ipart;
#endif
}


float floorf(float x)
{
#ifdef __IEEE754_FLOORF
	return __ieee754_floorf(x);
#else
	return (float)floor(x);
#endif
}


double fmod(double number, double denom)
{
	double result, tquot;

	if (isnan(number) != 0 || isnan(denom) != 0) {
		return NAN;
	}

	if ((denom == 0.0) || (isinf(number) != 0)) {
		errno = EDOM;
		return NAN;
	}

	if (((number == 0.0) && (denom != 0.0)) ||
			((isinf(number) == 0) && (isinf(denom) != 0))) {
		return number;
	}

	modf(number / denom, &tquot);
	result = tquot * denom;

	return number - result;
}


float fmodf(float x, float y)
{
	return (float)fmod((double)x, (double)y);
}


double round(double x)
{
#ifdef __IEEE754_ROUND
	return __ieee754_round(x);
#else
	double ret, frac;

	if (isnan(x) != 0) {
		return NAN;
	}

	frac = modf(x, &ret);

	if (frac >= 0.5) {
		ret += 1.0;
	}
	else if (frac <= -0.5) {
		ret -= 1.0;
	}

	return ret;
#endif
}


float roundf(float x)
{
#ifdef __IEEE754_ROUNDF
	return __ieee754_roundf(x);
#else
	return (float)round(x);
#endif
}


double trunc(double x)
{
#ifdef __IEEE754_TRUNC
	return __ieee754_trunc(x);
#else
	double ret;

	if (isnan(x) != 0) {
		return NAN;
	}

	modf(x, &ret);

	return ret;
#endif
}


float truncf(float x)
{
#ifdef __IEEE754_TRUNCF
	return __ieee754_truncf(x);
#else
	return (float)trunc(x);
#endif
}


double rint(double x)
{
#ifdef __IEEE754_RINT
	return __ieee754_rint(x);
#else
	double ipart, fpart, afpart;

	if (isnan(x) != 0) {
		return NAN;
	}

	fpart = modf(x, &ipart);
	afpart = fabs(fpart);

	if (afpart < 0.5) {
		return ipart;
	}

	if (afpart > 0.5) {
		return ipart + ((x < 0.0) ? -1.0 : 1.0);
	}

	/* Exactly halfway: round to even, matching the default FE_TONEAREST
	 * rounding mode. This is why rint() differs from round(), which rounds
	 * halves away from zero. */
	if (fmod(ipart, 2.0) != 0.0) {
		return ipart + ((x < 0.0) ? -1.0 : 1.0);
	}

	return ipart;
#endif
}


float rintf(float x)
{
#ifdef __IEEE754_RINTF
	return __ieee754_rintf(x);
#else
	return (float)rint((double)x);
#endif
}


double nearbyint(double x)
{
#ifdef __IEEE754_NEARBYINT
	return __ieee754_nearbyint(x);
#else
	/* Identical to rint() in this implementation: nearbyint() is only
	 * specified to differ by not raising FE_INEXACT, and this libm raises
	 * no floating-point exceptions. */
	return rint(x);
#endif
}


float nearbyintf(float x)
{
#ifdef __IEEE754_NEARBYINTF
	return __ieee754_nearbyintf(x);
#else
	return (float)nearbyint((double)x);
#endif
}


long int lrint(double x)
{
	return (long int)rint(x);
}


long long int llrint(double x)
{
	return (long long int)rint(x);
}


long int lrintf(float x)
{
	return (long int)rintf(x);
}


long long int llrintf(float x)
{
	return (long long int)rintf(x);
}


long int lround(double x)
{
	return (long int)round(x);
}


long long int llround(double x)
{
	return (long long int)round(x);
}


long int lroundf(float x)
{
	return (long int)roundf(x);
}


long long int llroundf(float x)
{
	return (long long int)roundf(x);
}


double fdim(double x, double y)
{
	if ((isnan(x) != 0) || (isnan(y) != 0)) {
		return NAN;
	}

	return (x > y) ? (x - y) : 0.0;
}


float fdimf(float x, float y)
{
	return (float)fdim((double)x, (double)y);
}


double fmax(double x, double y)
{
	if (isnan(x) != 0) {
		return y;
	}

	if (isnan(y) != 0) {
		return x;
	}

	return (x > y) ? x : y;
}


float fmaxf(float x, float y)
{
	return (float)fmax((double)x, (double)y);
}


double fmin(double x, double y)
{
	if (isnan(x) != 0) {
		return y;
	}

	if (isnan(y) != 0) {
		return x;
	}

	return (x < y) ? x : y;
}


float fminf(float x, float y)
{
	return (float)fmin((double)x, (double)y);
}


double copysign(double x, double y)
{
	conv_t *cx = (conv_t *)&x;
	conv_t *cy = (conv_t *)&y;

	cx->i.sign = cy->i.sign;

	return x;
}


float copysignf(float x, float y)
{
	return (float)copysign((double)x, (double)y);
}


double fabs(double x)
{
#ifdef __IEEE754_FABS
	return __ieee754_fabs(x);
#else
	if (isnan(x) != 0) {
		return NAN;
	}

	conv_t *conv = (conv_t *)&x;
	conv->i.sign = 0;

	return x;
#endif
}


float fabsf(float x)
{
#ifdef __IEEE754_FABSF
	return __ieee754_fabsf(x);
#else
	return (float)fabs(x);
#endif
}
