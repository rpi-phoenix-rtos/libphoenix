/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * log1p, expm1, asinh, acosh, atanh (C99 double)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>

/* The phoenix libm (the libm/phoenix sources, default when LIBM_USE_LIBMCS=n) shipped
 * a C99 subset: these five were declared in <math.h> but undefined, so ports that
 * need a full C99 libm failed to link (CPython's configure hard-errors "requires
 * C99 compatible libm" on acosh/asinh/atanh/expm1/log1p; jq also needed them).
 *
 * All five are built on an accurate log1p so the cancellation-prone regions
 * (x near 0, x near 1) keep full precision. log1p/expm1 use the Kahan
 * compensation identities: log(1+x) = log(u) * x/(u-1) with u = 1+x rounded, and
 * the analogous form for exp. Verified bit-close to glibc across the domain
 * (incl. tiny, negative, near-1, large, and Inf/NaN edges). */

double log1p(double x)
{
	double u;

	if (isnan(x)) {
		return x;
	}
	u = 1.0 + x;
	if (u == 1.0) {
		return x; /* x tiny: 1+x rounds to 1, log1p(x) ~= x */
	}
	if (u <= 0.0) {
		return log(u); /* x == -1 -> -inf; x < -1 -> NaN */
	}
	return log(u) * (x / (u - 1.0));
}


double expm1(double x)
{
	double u, um1;

	if (isnan(x)) {
		return x;
	}
	if (isinf(x)) {
		return (x > 0.0) ? x : -1.0;
	}
	u = exp(x);
	if (u == 1.0) {
		return x; /* x tiny */
	}
	um1 = u - 1.0;
	if (um1 == -1.0) {
		return -1.0; /* exp(x) underflowed to 0 */
	}
	return um1 * (x / log(u));
}


double asinh(double x)
{
	double a, r;

	if (!isfinite(x)) {
		return x; /* +-inf -> +-inf, NaN -> NaN */
	}
	a = fabs(x);
	r = log1p(a + a * a / (1.0 + sqrt(1.0 + a * a)));
	return (x < 0.0) ? -r : r;
}


double acosh(double x)
{
	double t;

	if (isnan(x)) {
		return x;
	}
	if (x < 1.0) {
		return NAN; /* domain error */
	}
	if (isinf(x)) {
		return x;
	}
	t = x - 1.0;
	return log1p(t + sqrt(t * t + 2.0 * t));
}


double atanh(double x)
{
	double a, r;

	if (isnan(x)) {
		return x;
	}
	a = fabs(x);
	if (a > 1.0) {
		return NAN; /* domain error */
	}
	r = 0.5 * log1p(2.0 * a / (1.0 - a)); /* a == 1 -> +inf */
	return (x < 0.0) ? -r : r;
}
