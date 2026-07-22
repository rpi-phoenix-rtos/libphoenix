/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Compatibility source file for libmcs header file
 *
 * Copyright 2025 Phoenix Systems
 * Author: Mikolaj Matalowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>

/* These libmcs-compat helpers are commonly ALSO defined by the bundled software
 * libm that some ports carry (e.g. MicroPython's lib/libm_dbl/ defines __signbitd),
 * which caused a "multiple definition of `__signbitd'" link error against libphoenix.
 * Mark them WEAK so a port's own strong definition overrides them without a clash;
 * libphoenix still provides them when nothing else does. */
#define WK __attribute__((weak))


WK int __fpclassifyf(float x)
{
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x);
}


WK int __fpclassifyd(double x)
{
	return __builtin_fpclassify(FP_NAN, FP_INFINITE, FP_NORMAL, FP_SUBNORMAL, FP_ZERO, x);
}


WK float nanf(const char *unused)
{
	(void)unused;
	return __builtin_nanf("");
}


WK int __signbitf(float x)
{
	return __builtin_signbitf(x);
}


WK int __signbitd(double x)
{
	return __builtin_signbit(x);
}


const float __inff = __builtin_inff();
const double __infd = __builtin_inf();
