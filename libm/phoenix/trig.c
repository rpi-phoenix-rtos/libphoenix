/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * cos, sin, tan, acos, asin, atan
 *
 * All are the FreeBSD msun implementations (msun/), with C99 errno reporting
 * added here.
 *
 * Copyright 2017, 2018, 2026 Phoenix Systems
 * Author: Aleksander Kaminski, Jakub Smolaga
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <math.h>
#include <errno.h>
#include "common.h"
#include "msun/msun.h"


double cos(double x)
{
	return math_check1(__msun_cos(x), x);
}


float cosf(float x)
{
	return (float)cos((double)x);
}


double sin(double x)
{
	return math_check1(__msun_sin(x), x);
}


float sinf(float x)
{
	return (float)sin((double)x);
}


double tan(double x)
{
	return math_check1(__msun_tan(x), x);
}


float tanf(float x)
{
	return (float)tan((double)x);
}


double acos(double x)
{
	return math_check1(__msun_acos(x), x);
}


float acosf(float x)
{
	return (float)acos((double)x);
}


double asin(double x)
{
	return math_check1(__msun_asin(x), x);
}


float asinf(float x)
{
	return (float)asin((double)x);
}


double atan(double x)
{
	return math_check1(__msun_atan(x), x);
}


float atanf(float x)
{
	return (float)atan((double)x);
}


double atan2(double y, double x)
{
	return math_check2(__msun_atan2(y, x), y, x);
}


float atan2f(float y, float x)
{
	return (float)atan2((double)y, (double)x);
}
