/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * pow, sqrt, cbrt
 *
 * pow, cbrt and hypot, and sqrt on targets without a square root
 * instruction, are the FreeBSD msun implementations (msun/), with C99 errno
 * reporting added here.
 *
 * Copyright 2017, 2026 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h> /* Needed for __ieee754_sqrt */
#include <math.h>
#include <float.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include "common.h"
#include "msun/msun.h"


double pow(double x, double y)
{
	return math_check2(__msun_pow(x, y), x, y);
}


float powf(float x, float y)
{
	return math_check2f((float)pow((double)x, (double)y), x, y);
}


double sqrt(double x)
{
	if (isnan(x) != 0) {
		return NAN;
	}

	if (x < 0.0) {
		errno = EDOM;
		return NAN;
	}

	if ((x == 0.0) || (x == INFINITY)) {
		return x;
	}

#ifdef __IEEE754_SQRT
	return __ieee754_sqrt(x);
#else
	/* correctly rounded, bit by bit in integer arithmetic */
	return __msun_sqrt(x);
#endif
}


float sqrtf(float x)
{
#ifdef __IEEE754_SQRTF
	if (isnan(x) != 0) {
		return NAN;
	}

	if (x < 0.0f) {
		errno = EDOM;
		return NAN;
	}

	if ((x == 0.0f) || (x == INFINITY)) {
		return x;
	}

	return __ieee754_sqrtf(x);
#else
	return (float)sqrt(x);
#endif
}


double hypot(double x, double y)
{
	return math_check2(__msun_hypot(x, y), x, y);
}


float hypotf(float x, float y)
{
	return math_check2f((float)hypot((double)x, (double)y), x, y);
}


double cbrt(double x)
{
	return __msun_cbrt(x);
}


float cbrtf(float x)
{
	return (float)cbrt((double)x);
}
