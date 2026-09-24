/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * strtoul, strtol
 *
 * Copyright 2017-2018, 2023 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Michal Miroslaw, Gerard Swiderski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>


static unsigned long int strtoul_common(const char *nptr, char **endptr, int base, int isUnsigned)
{
	unsigned long int cutoff, result = 0;
	int cutlim, t, width = 0, negative = 0, overflow = 0;
	const char *sptr = nptr;

	while (isspace(*sptr) != 0) {
		sptr++;
	}

	if (*sptr == '-') {
		negative = 1;
		sptr++;
	}
	else if (*sptr == '+') {
		sptr++;
	}

	if ((base == 16 || base == 0) && sptr[0] == '0' && (sptr[1] | 0x20) == 'x' && isxdigit(sptr[2])) {
		base = 16;
		sptr += 2;
	}

	if (base == 0 && sptr[0] == '0') {
		base = 8;
	}
	else if (base == 0) {
		base = 10;
	}

	if (base < 2 || base > (11 + 'z' - 'a')) {
		errno = EINVAL;
		return 0;
	}

	if (isUnsigned != 0) {
		cutoff = ULONG_MAX;
	}
	else {
		/* `-LONG_MIN` overflows a signed long, which is undefined behaviour --
		 * it happens to wrap to the right magnitude on two's complement, but the
		 * compiler is entitled to assume it cannot happen. Compute |LONG_MIN| in
		 * the unsigned type the value is used in. */
		cutoff = (negative != 0) ? ((unsigned long int)LONG_MAX + 1UL) : (unsigned long int)LONG_MAX;
	}

	cutlim = (int)(cutoff % base);
	cutoff /= base;

	while (isalnum(*sptr) != 0) {
		t = *sptr - '0';
		if (t > 9) {
			t = (*sptr | 0x20) - 'a' + 10;
		}

		if (t >= base) {
			break;
		}

		/* C17 7.22.1.4: the subject sequence is the LONGEST initial subsequence
		 * of the expected form -- overflow does not shorten it. Keep consuming
		 * digits (without accumulating) so `sptr`, and therefore *endptr, ends
		 * up past the whole number. Breaking out here left *endptr at `nptr`
		 * below, which means "no conversion performed": a caller walking a list
		 * of numbers with endptr could not advance past an out-of-range one. */
		if (overflow == 0 && (result > cutoff || (result == cutoff && t > cutlim))) {
			overflow = 1;
		}

		if (overflow == 0) {
			result = result * (unsigned long int)base + t;
		}
		width++;
		sptr++;
	}

	if (overflow != 0) {
		errno = ERANGE;
		if (isUnsigned != 0) {
			result = ULONG_MAX;
		}
		else {
			result = (negative != 0) ? LONG_MIN : LONG_MAX;
		}
	}
	else if (width == 0) {
		errno = EINVAL;
	}
	else if (negative != 0) {
		result = -result;
	}

	if (endptr != NULL) {
		*endptr = (char *)(width > 0 ? sptr : nptr);
	}

	return result;
}


unsigned long int strtoul(const char *nptr, char **endptr, int base)
{
	return strtoul_common(nptr, endptr, base, 1);
}


long int strtol(const char *nptr, char **endptr, int base)
{
	return (long int)strtoul_common(nptr, endptr, base, 0);
}


int atoi(const char *str)
{
	return (int)strtol(str, NULL, 10);
}


long int atol(const char *str)
{
	return strtol(str, NULL, 10);
}
