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


#define GEXTRA_PI 3.14159265358979323846


#ifndef FP_ILOGB0
#define FP_ILOGB0 (-INT_MAX)
#endif
#ifndef FP_ILOGBNAN
#define FP_ILOGBNAN INT_MAX
#endif


/* Lanczos approximation (g = 7, n = 9); ~1e-13 accuracy over the reals. */
static const double lanczos_g = 7.0;
static const double lanczos_c[9] = {
	0.99999999999980993,
	676.5203681218851,
	-1259.1392167224028,
	771.32342877765313,
	-176.61502916214059,
	12.507343278686905,
	-0.13857109526572012,
	9.9843695780195716e-6,
	1.5056327351493116e-7
};


double tgamma(double x)
{
	int i;
	double a, t;

	if (isnan(x)) {
		return x;
	}
	if (isinf(x)) {
		return (x > 0.0) ? INFINITY : NAN;
	}
	if (x == 0.0) {
		return copysign(INFINITY, x); /* ±0 -> ±inf (pole) */
	}
	if (x < 0.0 && x == floor(x)) {
		return NAN; /* poles at the negative integers */
	}

	if (x < 0.5) {
		/* reflection: G(x) = pi / (sin(pi x) G(1-x)) */
		return GEXTRA_PI / (sin(GEXTRA_PI * x) * tgamma(1.0 - x));
	}

	x -= 1.0;
	a = lanczos_c[0];
	t = x + lanczos_g + 0.5;
	for (i = 1; i < 9; i++) {
		a += lanczos_c[i] / (x + (double)i);
	}

	return sqrt(2.0 * GEXTRA_PI) * pow(t, x + 0.5) * exp(-t) * a;
}


double lgamma_r(double x, int *signp)
{
	int i, sign2;
	double a, t, xx, s, lg;

	*signp = 1;

	if (isnan(x)) {
		return x;
	}
	if (isinf(x)) {
		return INFINITY;
	}
	if (x == 0.0 || (x < 0.0 && x == floor(x))) {
		return INFINITY; /* poles */
	}

	if (x < 0.5) {
		/* reflection: ln|G(x)| = ln(pi/|sin(pi x)|) - ln|G(1-x)| */
		s = sin(GEXTRA_PI * x);
		lg = lgamma_r(1.0 - x, &sign2);
		*signp = (s < 0.0) ? -1 : 1;
		return log(GEXTRA_PI / fabs(s)) - lg;
	}

	xx = x - 1.0;
	a = lanczos_c[0];
	t = xx + lanczos_g + 0.5;
	for (i = 1; i < 9; i++) {
		a += lanczos_c[i] / (xx + (double)i);
	}

	/* G(x) = sqrt(2pi) t^(xx+0.5) e^-t a  =>  ln = 0.5 ln(2pi) + (xx+0.5) ln t - t + ln a */
	return 0.5 * log(2.0 * GEXTRA_PI) + (xx + 0.5) * log(t) - t + log(a);
}


double lgamma(double x)
{
	int sign;
	return lgamma_r(x, &sign);
}


double exp10(double x)
{
	return pow(10.0, x);
}


/* IEEE 754 remainder: r = x - n*y, n = round-to-nearest-even(x/y). |r| <= |y|/2. */
double remainder(double x, double y)
{
	double ay, r, q;

	if (isnan(x) || isnan(y)) {
		return x + y; /* propagate NaN */
	}
	if (isinf(x) || y == 0.0) {
		return (x - x) / (y - y); /* NaN, raises invalid */
	}
	if (isinf(y)) {
		return x;
	}

	ay = fabs(y);
	r = fmod(fabs(x), ay); /* 0 <= r < ay */

	if (2.0 * r > ay) {
		r -= ay;
	}
	else if (2.0 * r == ay) {
		/* tie: pick the even quotient */
		q = (fabs(x) - r) / ay;
		if (fmod(q, 2.0) != 0.0) {
			r -= ay;
		}
	}

	/* remainder is odd in x; r was computed from |x|, so negate for x < 0
	 * (NOT copysign: r may already be negative when |x| rounds up). */
	return (x < 0.0) ? -r : r;
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
	return (float)tgamma((double)x);
}


float lgammaf_r(float x, int *signp)
{
	return (float)lgamma_r((double)x, signp);
}


float lgammaf(float x)
{
	int sign;
	return (float)lgamma_r((double)x, &sign);
}


float exp10f(float x)
{
	return (float)exp10((double)x);
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
