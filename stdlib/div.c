/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stdlib/div.c
 *
 * Copyright 2022 Phoenix Systems
 * Author: Dawid Szpejna
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <inttypes.h>
#include <stdlib.h>


__EXPORT_INLINE int abs(int x);
__EXPORT_INLINE long int labs(long int x);
__EXPORT_INLINE long long int llabs(long long int x);


div_t div(int num, int den)
{
	div_t result = { .quot = num / den, .rem = num % den };

	/* Correcting if the result is wrong */
	if (num >= 0 && result.rem < 0) {
		result.quot++;
		result.rem -= den;
	}
	else if (num < 0 && result.rem > 0) {
		result.quot--;
		result.rem += den;
	}

	return result;
}


ldiv_t ldiv(long int num, long int den)
{
	ldiv_t result = { .quot = num / den, .rem = num % den };

	/* Correcting if the result is wrong */
	if (num >= 0 && result.rem < 0) {
		result.quot++;
		result.rem -= den;
	}
	else if (num < 0 && result.rem > 0) {
		result.quot--;
		result.rem += den;
	}

	return result;
}


lldiv_t lldiv(long long int num, long long int den)
{
	lldiv_t result = { .quot = num / den, .rem = num % den };

	/* Correcting if the result is wrong */
	if (num >= 0 && result.rem < 0) {
		result.quot++;
		result.rem -= den;
	}

	return result;
}


intmax_t imaxabs(intmax_t j)
{
	return (j < 0) ? -j : j;
}


imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom)
{
	/* C99 6.5.5p6: integer division truncates toward zero, so / and % already
	 * give the quotient and remainder 7.8.2.2 asks for. */
	imaxdiv_t result = { .quot = numer / denom, .rem = numer % denom };

	return result;
}
