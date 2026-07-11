/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * nl_langinfo
 *
 * Copyright 2026 Phoenix Systems
 * Author: Witold Bołt
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <langinfo.h>
#include <stddef.h>


/* Phoenix supports the C/POSIX locale only. CODESET reports ASCII
 * (ANSI_X3.4-1968), not UTF-8: libphoenix's multibyte layer (mbrtowc/mbtowc/
 * wcrtomb) maps each byte 1:1 to a wchar_t with no UTF-8 decoder, so ncurses/mc/
 * vim — which read CODESET to decide whether to enter multibyte mode — must stay
 * on the single-byte path or they misrender. Switch this to "UTF-8" only once the
 * restartable wide-char set actually decodes UTF-8. The remaining items are the
 * standard C-locale strings and match strftime()/localeconv(). */
static const char *const c_locale[_NL_ITEM_COUNT] = {
	[CODESET] = "ANSI_X3.4-1968",
	[D_T_FMT] = "%a %b %e %H:%M:%S %Y",
	[D_FMT] = "%m/%d/%y",
	[T_FMT] = "%H:%M:%S",
	[T_FMT_AMPM] = "%I:%M:%S %p",
	[AM_STR] = "AM",
	[PM_STR] = "PM",

	[DAY_1] = "Sunday",
	[DAY_2] = "Monday",
	[DAY_3] = "Tuesday",
	[DAY_4] = "Wednesday",
	[DAY_5] = "Thursday",
	[DAY_6] = "Friday",
	[DAY_7] = "Saturday",

	[ABDAY_1] = "Sun",
	[ABDAY_2] = "Mon",
	[ABDAY_3] = "Tue",
	[ABDAY_4] = "Wed",
	[ABDAY_5] = "Thu",
	[ABDAY_6] = "Fri",
	[ABDAY_7] = "Sat",

	[MON_1] = "January",
	[MON_2] = "February",
	[MON_3] = "March",
	[MON_4] = "April",
	[MON_5] = "May",
	[MON_6] = "June",
	[MON_7] = "July",
	[MON_8] = "August",
	[MON_9] = "September",
	[MON_10] = "October",
	[MON_11] = "November",
	[MON_12] = "December",

	[ABMON_1] = "Jan",
	[ABMON_2] = "Feb",
	[ABMON_3] = "Mar",
	[ABMON_4] = "Apr",
	[ABMON_5] = "May",
	[ABMON_6] = "Jun",
	[ABMON_7] = "Jul",
	[ABMON_8] = "Aug",
	[ABMON_9] = "Sep",
	[ABMON_10] = "Oct",
	[ABMON_11] = "Nov",
	[ABMON_12] = "Dec",

	[RADIXCHAR] = ".",
	[THOUSEP] = "",
	[YESEXPR] = "^[yY]",
	[NOEXPR] = "^[nN]",
	[CRNCYSTR] = "",
};


char *nl_langinfo(nl_item item)
{
	const char *s = NULL;

	if (item >= 0 && item < _NL_ITEM_COUNT) {
		s = c_locale[item];
	}

	/* POSIX: return a pointer to an empty string for an invalid item. */
	return (char *)(s != NULL ? s : "");
}
