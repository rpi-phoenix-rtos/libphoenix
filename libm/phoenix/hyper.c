/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * hyperbolic functions
 *
 * sinh, cosh and tanh are the FreeBSD msun implementations (msun/), with C99
 * errno reporting added here.
 *
 * Copyright 2018, 2026 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <float.h>
#include <errno.h>
#include "common.h"
#include "msun/msun.h"


double cosh(double x)
{
	return math_check1(__msun_cosh(x), x);
}


float coshf(float x)
{
	return math_check1f((float)cosh((double)x), x);
}


double sinh(double x)
{
	return math_check1(__msun_sinh(x), x);
}


float sinhf(float x)
{
	return math_check1f((float)sinh((double)x), x);
}


double tanh(double x)
{
	return math_check1(__msun_tanh(x), x);
}


float tanhf(float x)
{
	return (float)tanh((double)x);
}
