/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Private header of the FreeBSD msun sources in libm/phoenix/msun/
 *
 * Derived from FreeBSD lib/msun/src/math_private.h (freebsd-src commit
 * 77a7a48a1cb003831ff1d4342b1974fc7f79381e), reduced to what the imported
 * double-precision sources use: IEEE-754 word access, STRICT_ASSIGN, rnint,
 * irint, nan_mix, _2sumF, FFLOOR, subnormal_ilogb and the kernel prototypes.
 * The __msun_* functions are the msun implementations; the public C99
 * functions in the libm/phoenix sources wrap them to add errno reporting (see
 * libm/phoenix/common.h).
 *
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 *
 * SPDX-License-Identifier: SunMicrosystems
 */

#ifndef _LIBPHOENIX_MATH_MSUN_H_
#define _LIBPHOENIX_MATH_MSUN_H_

#include <float.h>
#include <stdint.h>
#include <sys/types.h>


/* msun's internal kernels keep their fdlibm names in the sources, but are
 * linked under private ones: ports that bundle their own fdlibm (SuperTuxKart's
 * inputs do) define the same __kernel_* symbols, and a static link must not
 * resolve either copy against the other. */
#define __kernel_sin       __msun_kernel_sin
#define __kernel_cos       __msun_kernel_cos
#define __kernel_tan       __msun_kernel_tan
#define __kernel_sinpi     __msun_kernel_sinpi
#define __kernel_cospi     __msun_kernel_cospi
#define __kernel_rem_pio2  __msun_kernel_rem_pio2
#define __ieee754_rem_pio2 __msun_ieee754_rem_pio2


/* A union which permits us to convert between a double and two 32 bit ints. */
typedef union {
	double value;
	struct {
#if __FLOAT_WORD_ORDER__ == __ORDER_LITTLE_ENDIAN__
		u_int32_t lsw;
		u_int32_t msw;
#elif __FLOAT_WORD_ORDER__ == __ORDER_BIG_ENDIAN__
		u_int32_t msw;
		u_int32_t lsw;
#else
#error "Unsupported floating-point word order"
#endif
	} parts;
	struct {
		u_int64_t w;
	} xparts;
} ieee_double_shape_type;


/* Get two 32 bit ints from a double. */
#define EXTRACT_WORDS(ix0, ix1, d) \
	do { \
		ieee_double_shape_type ew_u; \
		ew_u.value = (d); \
		(ix0) = ew_u.parts.msw; \
		(ix1) = ew_u.parts.lsw; \
	} while (0)

/* Get a 64-bit int from a double. */
#define EXTRACT_WORD64(ix, d) \
	do { \
		ieee_double_shape_type ew_u; \
		ew_u.value = (d); \
		(ix) = ew_u.xparts.w; \
	} while (0)

/* Get the more significant 32 bit int from a double. */
#define GET_HIGH_WORD(i, d) \
	do { \
		ieee_double_shape_type gh_u; \
		gh_u.value = (d); \
		(i) = gh_u.parts.msw; \
	} while (0)

/* Get the less significant 32 bit int from a double. */
#define GET_LOW_WORD(i, d) \
	do { \
		ieee_double_shape_type gl_u; \
		gl_u.value = (d); \
		(i) = gl_u.parts.lsw; \
	} while (0)

/* Set a double from two 32 bit ints. */
#define INSERT_WORDS(d, ix0, ix1) \
	do { \
		ieee_double_shape_type iw_u; \
		iw_u.parts.msw = (ix0); \
		iw_u.parts.lsw = (ix1); \
		(d) = iw_u.value; \
	} while (0)

/* Set a double from a 64-bit int. */
#define INSERT_WORD64(d, ix) \
	do { \
		ieee_double_shape_type iw_u; \
		iw_u.xparts.w = (ix); \
		(d) = iw_u.value; \
	} while (0)

/* Set the more significant 32 bits of a double from an int. */
#define SET_HIGH_WORD(d, v) \
	do { \
		ieee_double_shape_type sh_u; \
		sh_u.value = (d); \
		sh_u.parts.msw = (v); \
		(d) = sh_u.value; \
	} while (0)

/* Set the less significant 32 bits of a double from an int. */
#define SET_LOW_WORD(d, v) \
	do { \
		ieee_double_shape_type sl_u; \
		sl_u.value = (d); \
		sl_u.parts.lsw = (v); \
		(d) = sl_u.value; \
	} while (0)


/* Strict C99 assignment semantics where the FPU evaluates in extra precision
 * (ia32 is built with -mno-sse, i.e. x87, FLT_EVAL_METHOD == 2). */
#if !defined(FLT_EVAL_METHOD) || (FLT_EVAL_METHOD == 0)
#define STRICT_ASSIGN(type, lval, rval) ((lval) = (rval))
#else
#define STRICT_ASSIGN(type, lval, rval) \
	do { \
		volatile type __lval; \
		if (sizeof(type) >= sizeof(long double)) { \
			(lval) = (rval); \
		} \
		else { \
			__lval = (rval); \
			(lval) = __lval; \
		} \
	} while (0)
#endif


/* Round to the nearest integer for |x| < 2^52 in the default rounding mode. */
static inline double rnint(double x)
{
	double t;

	STRICT_ASSIGN(double, t, x + 0x1.8p52);
	return t - 0x1.8p52;
}


#define irint(x) ((int)(x))


#ifndef __always_inline
#define __always_inline inline __attribute__((__always_inline__))
#endif


/* Return a NaN derived from two operands, at least one of which is NaN.
 * FreeBSD computes this in long double; on the 128-bit long double targets
 * that is a soft-float call, and double is enough to propagate the NaN. */
#define nan_mix(x, y)           ((x) + (y))
#define nan_mix_op(x, y, op)    ((x) op (y))


/* Set (a, b) to the exact sum a + b as a head and a tail. |a| >= |b| or a == 0. */
#define _2sumF(a, b) \
	do { \
		__typeof(a) __w; \
		__w = (a) + (b); \
		(b) = ((a) - __w) + (b); \
		(a) = __w; \
	} while (0)


/* floor(x) for 1 <= |x| < 2^52 given the words ix, lx of x; sets j0 to the unbiased exponent. */
#define FFLOOR(x, j0, ix, lx) \
	do { \
		(j0) = (((ix) >> 20) & 0x7ff) - 0x3ff; \
		if ((j0) < 20) { \
			(ix) &= ~(0x000fffff >> (j0)); \
			(lx) = 0; \
		} \
		else { \
			(lx) &= ~((uint32_t)0xffffffff >> ((j0) - 20)); \
		} \
		INSERT_WORDS((x), (ix), (lx)); \
	} while (0)


/* For a subnormal double entity split into high and low parts, compute ilogb. */
static inline int32_t subnormal_ilogb(int32_t hi, int32_t lo)
{
	int32_t j;
	uint32_t i;

	j = -1022;
	if (hi == 0) {
		j -= 21;
		i = (uint32_t)lo;
	}
	else {
		i = (uint32_t)hi << 11;
	}

	for (; i < 0x7fffffff; i <<= 1) {
		j -= 1;
	}

	return j;
}


/* fdlibm kernel functions */
int __kernel_rem_pio2(double *x, double *y, int e0, int nx, int prec);
#ifndef INLINE_REM_PIO2
int __ieee754_rem_pio2(double x, double *y);
#endif
double __kernel_sin(double x, double y, int iy);
double __kernel_cos(double x, double y);
double __kernel_tan(double x, double y, int iy);
double __ldexp_exp(double x, int expt);


/* msun implementations behind the public functions */
double __msun_exp(double x);
double __msun_exp2(double x);
double __msun_expm1(double x);
double __msun_log(double x);
double __msun_log2(double x);
double __msun_log10(double x);
double __msun_log1p(double x);
double __msun_pow(double x, double y);
double __msun_cbrt(double x);
double __msun_hypot(double x, double y);
double __msun_sin(double x);
double __msun_cos(double x);
double __msun_tan(double x);
double __msun_asin(double x);
double __msun_acos(double x);
double __msun_atan(double x);
double __msun_atan2(double y, double x);
double __msun_sinh(double x);
double __msun_cosh(double x);
double __msun_tanh(double x);
double __msun_asinh(double x);
double __msun_acosh(double x);
double __msun_atanh(double x);
double __msun_lgamma_r(double x, int *signgamp);
double __msun_tgamma(double x);
double __msun_fmod(double x, double y);
double __msun_remainder(double x, double p);
double __msun_scalbn(double x, int n);
double __msun_sqrt(double x);
double __msun_sinpi(double x);
double __msun_cospi(double x);

#endif
