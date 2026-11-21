/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * setlocale(), localeconv()
 *
 * Copyright 2018, 2024, 2026 Phoenix Systems
 * Author: Michal Miroslaw
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */
#include <limits.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "../wchar/wchar-internal.h"

/* for regex/collate.c */
char *_PathLocale;


static const struct lconv fixed_locale = {
	.decimal_point = ".",
	.thousands_sep = "",
	.grouping = "",
	.mon_decimal_point = "",
	.mon_thousands_sep = "",
	.mon_grouping = "",
	.positive_sign = "",
	.negative_sign = "",
	.currency_symbol = "",
	.int_curr_symbol = "",
	.frac_digits = CHAR_MAX,
	.p_cs_precedes = CHAR_MAX,
	.n_cs_precedes = CHAR_MAX,
	.p_sep_by_space = CHAR_MAX,
	.n_sep_by_space = CHAR_MAX,
	.p_sign_posn = CHAR_MAX,
	.n_sign_posn = CHAR_MAX,
	.int_frac_digits = CHAR_MAX,
	.int_p_cs_precedes = CHAR_MAX,
	.int_n_cs_precedes = CHAR_MAX,
	.int_p_sep_by_space = CHAR_MAX,
	.int_n_sep_by_space = CHAR_MAX,
	.int_p_sign_posn = CHAR_MAX,
	.int_n_sign_posn = CHAR_MAX,
};


struct lconv *localeconv(void)
{
	return (struct lconv *)&fixed_locale;
}


/*
 * setlocale()
 *
 * Every category accepts the C/POSIX locale ("C", "POSIX") and any UTF-8 one:
 * a name of the form <language>[_<territory>].<codeset>[@<modifier>] whose
 * codeset is UTF-8 (spelled "UTF-8" or "utf8", in any case), e.g. "C.UTF-8",
 * "POSIX.utf8" or "pl_PL.UTF-8". A UTF-8 name changes only the character
 * encoding (LC_CTYPE); everything else stays as in the C locale, since no
 * locale data exists. Other names fail, as they did before UTF-8 existed.
 */

#define LOCALE_NAME_MAX 32 /* longest accepted name, with the terminator */


static const struct {
	int category;
	const char *env;
} locale_categories[] = {
	{ LC_CTYPE, "LC_CTYPE" },
	{ LC_NUMERIC, "LC_NUMERIC" },
	{ LC_TIME, "LC_TIME" },
	{ LC_COLLATE, "LC_COLLATE" },
	{ LC_MONETARY, "LC_MONETARY" },
	{ LC_MESSAGES, "LC_MESSAGES" },
};


#define LOCALE_COUNT (sizeof(locale_categories) / sizeof(locale_categories[0]))
#define LOCALE_CTYPE 0u /* index of LC_CTYPE in locale_categories[] */


static struct {
	char names[LOCALE_COUNT][LOCALE_NAME_MAX];
	/* LC_ALL of mixed categories: "LC_CTYPE=<name>;LC_NUMERIC=<name>;..." */
	char all[LOCALE_COUNT * (sizeof("LC_MESSAGES=;") + LOCALE_NAME_MAX)];
} locale_common = {
	/* the implementation-defined name the C locale had always been given */
	.names = { "POSIX", "POSIX", "POSIX", "POSIX", "POSIX", "POSIX" },
};


/* Index in locale_categories[], LOCALE_COUNT for LC_ALL, -1 if unknown */
static int locale_index(int category)
{
	size_t i;

	if (category == LC_ALL) {
		return (int)LOCALE_COUNT;
	}
	for (i = 0; i < LOCALE_COUNT; i++) {
		if (locale_categories[i].category == category) {
			return (int)i;
		}
	}

	return -1;
}


static int locale_isAlpha(char c)
{
	return (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))) ? 1 : 0;
}


static int locale_isAlnum(char c)
{
	return ((locale_isAlpha(c) != 0) || ((c >= '0') && (c <= '9'))) ? 1 : 0;
}


static int locale_lower(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ? (c - 'A' + 'a') : c;
}


/* Whether s starts with the codeset name UTF-8 or UTF8 (any case) and returns
 * the end of it in *end */
static int locale_isUtf8Codeset(const char *s, const char **end)
{
	static const char utf[] = "utf";

	if ((locale_lower(s[0]) != utf[0]) || (locale_lower(s[1]) != utf[1]) || (locale_lower(s[2]) != utf[2])) {
		return 0;
	}
	s += 3;
	if (*s == '-') {
		s++;
	}
	if (*s != '8') {
		return 0;
	}
	*end = s + 1;

	return 1;
}


/* Whether name is accepted and, if so, whether it selects UTF-8 */
static int locale_check(const char *name, int *utf8)
{
	const char *p = name;

	if (strlen(name) >= LOCALE_NAME_MAX) {
		return 0;
	}
	if ((strcmp(name, "C") == 0) || (strcmp(name, "POSIX") == 0)) {
		*utf8 = 0;
		return 1;
	}

	/* <language>[_<territory>] */
	if (locale_isAlpha(*p) == 0) {
		return 0;
	}
	while (locale_isAlpha(*p) != 0) {
		p++;
	}
	if (*p == '_') {
		p++;
		if (locale_isAlnum(*p) == 0) {
			return 0;
		}
		while (locale_isAlnum(*p) != 0) {
			p++;
		}
	}

	/* .<codeset> */
	if ((*p != '.') || (locale_isUtf8Codeset(p + 1, &p) == 0)) {
		return 0;
	}

	/* [@<modifier>] */
	if (*p == '@') {
		p++;
		if (locale_isAlnum(*p) == 0) {
			return 0;
		}
		while ((locale_isAlnum(*p) != 0) || (*p == '_') || (*p == '-')) {
			p++;
		}
	}
	if (*p != '\0') {
		return 0;
	}
	*utf8 = 1;

	return 1;
}


/* The name "" stands for in category i: the first of the environment
 * variables LC_ALL, LC_<category>, LANG that is set and not empty, else the
 * implementation default (POSIX.1-2024, XBD 8.2) */
static const char *locale_fromEnv(size_t i)
{
	const char *vars[] = { "LC_ALL", locale_categories[i].env, "LANG" };
	const char *val;
	size_t v;

	for (v = 0; v < (sizeof(vars) / sizeof(vars[0])); v++) {
		val = getenv(vars[v]);
		if ((val != NULL) && (val[0] != '\0')) {
			return val;
		}
	}

	return "POSIX";
}


/* Sets names[i] to the name locale stands for; returns 0 if it is not accepted */
static int locale_resolve(char names[][LOCALE_NAME_MAX], size_t i, const char *locale)
{
	int utf8;

	if (locale[0] == '\0') {
		locale = locale_fromEnv(i);
	}
	if (locale_check(locale, &utf8) == 0) {
		return 0;
	}
	(void)strcpy(names[i], locale);

	return 1;
}


/* Sets names[] from what setlocale(LC_ALL, NULL) returns for mixed categories */
static int locale_resolveAll(char names[][LOCALE_NAME_MAX], const char *all)
{
	char name[LOCALE_NAME_MAX];
	const char *eq, *end;
	size_t i, len;

	while (*all != '\0') {
		eq = strchr(all, '=');
		if (eq == NULL) {
			return 0;
		}
		for (i = 0; i < LOCALE_COUNT; i++) {
			len = strlen(locale_categories[i].env);
			if (((size_t)(eq - all) == len) && (strncmp(all, locale_categories[i].env, len) == 0)) {
				break;
			}
		}
		end = strchr(eq, ';');
		if (end == NULL) {
			end = eq + strlen(eq);
		}
		len = (size_t)(end - (eq + 1));
		if ((i == LOCALE_COUNT) || (len == 0u) || (len >= sizeof(name))) {
			return 0;
		}
		(void)memcpy(name, eq + 1, len);
		name[len] = '\0';
		if (locale_resolve(names, i, name) == 0) {
			return 0;
		}
		all = (*end == ';') ? (end + 1) : end;
	}

	return 1;
}


/* Copies s to dst without its terminator; returns its length */
static size_t locale_append(char *dst, const char *s)
{
	size_t len = strlen(s);

	(void)memcpy(dst, s, len);

	return len;
}


static char *locale_query(int idx)
{
	size_t i, len = 0;

	if (idx != (int)LOCALE_COUNT) {
		return locale_common.names[idx];
	}
	for (i = 1; i < LOCALE_COUNT; i++) {
		if (strcmp(locale_common.names[i], locale_common.names[0]) != 0) {
			break;
		}
	}
	if (i == LOCALE_COUNT) {
		return locale_common.names[0];
	}

	for (i = 0; i < LOCALE_COUNT; i++) {
		if (i != 0u) {
			locale_common.all[len++] = ';';
		}
		len += locale_append(&locale_common.all[len], locale_categories[i].env);
		locale_common.all[len++] = '=';
		len += locale_append(&locale_common.all[len], locale_common.names[i]);
	}
	locale_common.all[len] = '\0';

	return locale_common.all;
}


char *setlocale(int category, const char *locale)
{
	char names[LOCALE_COUNT][LOCALE_NAME_MAX];
	int idx = locale_index(category);
	int ok = 1, utf8 = 0;
	size_t i;

	if (idx < 0) {
		return NULL;
	}
	if (locale == NULL) {
		return locale_query(idx);
	}

	/* nothing changes unless every category accepts its new name */
	(void)memcpy(names, locale_common.names, sizeof(names));
	if (idx != (int)LOCALE_COUNT) {
		ok = locale_resolve(names, (size_t)idx, locale);
	}
	else if (strchr(locale, '=') != NULL) {
		ok = locale_resolveAll(names, locale);
	}
	else {
		for (i = 0; (i < LOCALE_COUNT) && (ok != 0); i++) {
			ok = locale_resolve(names, i, locale);
		}
	}
	if (ok == 0) {
		return NULL;
	}

	(void)memcpy(locale_common.names, names, sizeof(names));
	(void)locale_check(locale_common.names[LOCALE_CTYPE], &utf8);
	_wchar_setUtf8(utf8);

	return locale_query(idx);
}
