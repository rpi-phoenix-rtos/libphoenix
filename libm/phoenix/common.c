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

#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include "common.h"

void normalizeSub(double *x, int *exp)
{
	conv_t *conv = (conv_t *)x;

	if (conv->i.mantisa == 0) {
		return;
	}

	while ((conv->i.mantisa & 0xffffff0000000LL) == 0) {
		conv->i.mantisa <<= 24;
		*exp -= 24;
	}

	while ((conv->i.mantisa & 0xff00000000000LL) == 0) {
		conv->i.mantisa <<= 8;
		*exp -= 8;
	}

	while ((conv->i.mantisa & 0xf000000000000LL) == 0) {
		conv->i.mantisa <<= 4;
		*exp -= 4;
	}

	while ((conv->i.mantisa & 0x8000000000000LL) == 0) {
		conv->i.mantisa <<= 1;
		*exp -= 1;
	}

	/* Subnormals have explicit MSB bit, have to remove it */
	conv->i.mantisa <<= 1;
	*exp -= 1;

	conv->i.exponent = 1;
}
