/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * math.h common
 *
 * Copyright 2017 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _LIBPHOENIX_MATH_COMMON_H_
#define _LIBPHOENIX_MATH_COMMON_H_

#include <errno.h>
#include <math.h>
#include <stdint.h>


typedef union {
	struct {
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
		uint64_t mantisa : 52;
		uint16_t exponent : 11;
		uint8_t sign : 1;
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
		uint8_t sign : 1;
		uint16_t exponent : 11;
		uint64_t mantisa : 52;
#else
#error "Unsupported byte order"
#endif
	} i;
	double d;
} conv_t;


extern void normalizeSub(double *x, int *exp);


/* C99 7.12.1 error reporting (MATH_ERRNO) for the result r of an Annex F
 * implementation (the msun sources in msun/, which report errors through
 * the floating-point exceptions only): a NaN from non-NaN arguments is a
 * domain error, an infinity from finite arguments is a pole or an overflow
 * (range) error. */
static inline double math_check1(double r, double x)
{
	if (isnan(r) != 0) {
		if (isnan(x) == 0) {
			errno = EDOM;
		}
	}
	else if ((isinf(r) != 0) && (isfinite(x) != 0)) {
		errno = ERANGE;
	}

	return r;
}


static inline double math_check2(double r, double x, double y)
{
	if (isnan(r) != 0) {
		if ((isnan(x) == 0) && (isnan(y) == 0)) {
			errno = EDOM;
		}
	}
	else if ((isinf(r) != 0) && (isfinite(x) != 0) && (isfinite(y) != 0)) {
		errno = ERANGE;
	}

	return r;
}


/* The float functions are computed in double and rounded: this adds the
 * overflow of that final rounding, which the double function cannot see. */
static inline float math_check1f(float r, float x)
{
	return (float)math_check1((double)r, (double)x);
}


static inline float math_check2f(float r, float x, float y)
{
	return (float)math_check2((double)r, (double)x, (double)y);
}


#endif
