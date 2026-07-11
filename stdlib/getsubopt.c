/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * getsubopt
 *
 * Copyright 2026 Phoenix Systems
 * Author: Witold Bołt
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 *
 */

#include <stdlib.h>
#include <string.h>


int getsubopt(char **optionp, char *const *tokens, char **valuep)
{
	char *s = *optionp;
	int i;

	*valuep = NULL;
	if (*s == '\0') {
		return -1;
	}

	/* Advance *optionp past this suboption, NUL-terminating it at the ','. */
	for (*optionp = s; **optionp != '\0' && **optionp != ','; ++(*optionp)) {
		;
	}
	if (**optionp != '\0') {
		*(*optionp)++ = '\0';
	}

	/* Match "name" or "name=value" against the token list. */
	for (i = 0; tokens[i] != NULL; ++i) {
		size_t n = strlen(tokens[i]);
		if (strncmp(s, tokens[i], n) == 0) {
			if (s[n] == '=') {
				*valuep = s + n + 1;
				return i;
			}
			if (s[n] == '\0') {
				return i;
			}
		}
	}

	/* Unrecognised suboption: point *valuep at the whole token. */
	*valuep = s;
	return -1;
}
