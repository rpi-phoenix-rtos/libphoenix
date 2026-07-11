/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * langinfo.h
 *
 * Copyright 2026 Phoenix Systems
 * Author: Witold Bołt
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LANGINFO_H_
#define _LANGINFO_H_


typedef int nl_item;


/* Language/locale information items for nl_langinfo(). Phoenix implements the
 * C/POSIX locale only, so nl_langinfo() returns the fixed C-locale strings. */
enum {
	CODESET,    /* character encoding name */
	D_T_FMT,    /* date+time format (strftime) */
	D_FMT,      /* date format */
	T_FMT,      /* time format */
	T_FMT_AMPM, /* 12-hour time format */
	AM_STR,     /* ante-meridiem affix */
	PM_STR,     /* post-meridiem affix */

	DAY_1, DAY_2, DAY_3, DAY_4, DAY_5, DAY_6, DAY_7,          /* Sunday..Saturday */
	ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7,

	MON_1, MON_2, MON_3, MON_4, MON_5, MON_6,                 /* January..December */
	MON_7, MON_8, MON_9, MON_10, MON_11, MON_12,
	ABMON_1, ABMON_2, ABMON_3, ABMON_4, ABMON_5, ABMON_6,
	ABMON_7, ABMON_8, ABMON_9, ABMON_10, ABMON_11, ABMON_12,

	RADIXCHAR,  /* radix (decimal point) character */
	THOUSEP,    /* thousands separator */
	YESEXPR,    /* affirmative response regex */
	NOEXPR,     /* negative response regex */
	CRNCYSTR,   /* currency symbol */

	_NL_ITEM_COUNT
};


#ifdef __cplusplus
extern "C" {
#endif


extern char *nl_langinfo(nl_item item);


#ifdef __cplusplus
}
#endif


#endif /* _LANGINFO_H_ */
