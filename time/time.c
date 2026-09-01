/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * time
 *
 * Copyright 2017, 2023 Phoenix Systems
 * Author: Andrzej Asztemborski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <limits.h>

#include "../common/util.h"


char *tzname[2];


long timezone;


int daylight;


static const char wdayasc[8][10] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "???" };


static const char monasc[13][10] = { "January", "February", "March", "April", "May",
	"June", "July", "August", "September", "October", "November", "December", "???" };


static int daysofmonth(int month, int leap)
{
	static const int lut[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (month == 1 && leap)
		return 29;
	else
		return lut[month];
}


static inline int isleap(int year)
{
	return !(year % 400) || (!(year % 4) && (year % 100));
}


void tzset(void)
{
	static char tznamestore[2][4];

	/* TODO - env parsing */

	strcpy(tznamestore[0], "UTC");
	tznamestore[1][0] = '\0';
	tzname[0] = tznamestore[0];
	tzname[1] = tznamestore[1];
	timezone = 0;
	daylight = 0;
}


time_t time(time_t *tp)
{
	int err;
	time_t now, offs;

	err = gettime(&now, &offs);
	if (err < 0) {
		return (time_t)SET_ERRNO(err);
	}

	now += offs;

	/* microseconds to seconds */
	now /= 1000 * 1000;

	if (tp != NULL) {
		*tp = now;
	}

	return now;
}


int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	int err;
	time_t now, offs;

	if (tp == NULL) {
		return SET_ERRNO(-EINVAL);
	}

	if (clk_id != CLOCK_REALTIME && clk_id != CLOCK_MONOTONIC && clk_id != CLOCK_MONOTONIC_RAW) {
		return SET_ERRNO(-EINVAL);
	}

	err = gettime(&now, &offs);
	if (err < 0) {
		return SET_ERRNO(err);
	}

	if (clk_id == CLOCK_REALTIME) {
		now += offs;
	}

	tp->tv_sec = now / (1000 * 1000);
	now -= tp->tv_sec * 1000 * 1000;
	tp->tv_nsec = now * 1000;

	return EOK;
}


int clock_settime(clockid_t clock_id, const struct timespec *tp)
{
	if (clock_id != CLOCK_REALTIME || !__timespecValid(tp) || tp->tv_sec < 0) {
		return SET_ERRNO(-EINVAL);
	}

	int err;
	time_t now, time = tp->tv_sec * 1000 * 1000 + tp->tv_nsec / 1000;

	err = gettime(&now, NULL);
	if (err < 0) {
		return SET_ERRNO(err);
	}

	time -= now;

	return SET_ERRNO(settime(time));
}


char *asctime_r(const struct tm *tp, char *buf)
{
	int wday, mon;

	wday = tp->tm_wday < 0 || tp->tm_wday > 6 ? 7 : tp->tm_wday;
	mon = tp->tm_mon < 0 || tp->tm_mon > 11 ? 12 : tp->tm_mon;

	sprintf(buf, "%.3s %.3s %d %02d:%02d:%02d %d\n", wdayasc[wday], monasc[mon],
		tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec, tp->tm_year + 1900);

	return buf;
}


char *asctime(const struct tm *tp)
{
	static char buff[32];

	return asctime_r(tp, buff);
}


double difftime(time_t t1, time_t t2)
{
	return (double)t1 - (double)t2;
}


static int epochDaysToYears(int days, int *yearDay)
{
	/* Move day 0 from 1970-01-01 to 2000-02-29. 2000-02-29 closest date when all periods restart.
	 * Algorithm also works correctly when days < 0.
	 */
	int year = 2000, leapDay = 0, fullPeriods;
	days -= 11016;

	static const struct {
		int nDays;         /* Number of days this period has */
		int nYears;        /* Number of years this period has */
		int isZeroDayLeap; /* Is day 0 of this period a leap day (unless a larger period was tried before) */
	} periods[4] = {
		{ 146097, 400, 1 },
		{ 36524, 100, 0 },
		{ 1461, 4, 1 },
		{ 365, 1, 0 },
	};

	for (int i = 0; i < sizeof(periods) / sizeof(*periods); i++) {
		fullPeriods = days / periods[i].nDays;
		days = days % periods[i].nDays;
		if (days < 0) {
			days += periods[i].nDays;
			fullPeriods -= 1;
		}

		year += fullPeriods * periods[i].nYears;
		leapDay = periods[i].isZeroDayLeap;
		if (days == 0) {
			break;
		}
	}

	/* Day 0 is now Feb 29th if leapDay == 1, otherwise it's Feb 28th (leap years have two 0-days!)
	 * Day 1 is always March 1st, but which year day depends on if the year is a leap year.
	 * Day 307 and higher are next year, but before Feb 28th (before possible leap day, so leap year or not doesn't matter)
	 */
	if (days >= 307) {
		year += 1;
		*yearDay = days - 307;
	}
	else if (days == 0) {
		*yearDay = 58 + leapDay;
	}
	else {
		*yearDay = days + (isleap(year) ? 59 : 58);
	}

	return year;
}


struct tm *gmtime_r(const time_t *timep, struct tm *res)
{
	/* We need to be able to represent days as int */
	if ((sizeof(time_t) > sizeof(int)) && (*timep > ((time_t)INT_MAX * (24 * 60 * 60)))) {
		errno = EOVERFLOW;
		return NULL;
	}

	int seconds;
	int days, month, year, isYearLeap;

	days = *timep / (24 * 60 * 60);
	seconds = *timep - (time_t)days * (24 * 60 * 60);
	if (seconds < 0) {
		days--;
		seconds += (24 * 60 * 60);
	}

	res->tm_hour = seconds / (60 * 60);
	seconds = seconds % (60 * 60);

	res->tm_min = seconds / 60;
	res->tm_sec = seconds % 60;

	res->tm_wday = (days + 4) % 7;
	if (res->tm_wday < 0) {
		res->tm_wday += 7;
	}

	year = epochDaysToYears(days, &days);
	isYearLeap = isleap(year);
	res->tm_year = year - 1900;
	res->tm_yday = days;

	for (month = 0; month < 12; month++) {
		int monthLength = daysofmonth(month, isYearLeap);
		if (days >= monthLength) {
			days -= monthLength;
		}
		else {
			break;
		}
	}

	res->tm_mon = month;
	res->tm_mday = days + 1;
	res->tm_isdst = 0;

	return res;
}


struct tm *gmtime(const time_t *timep)
{
	static struct tm tmp;

	return gmtime_r(timep, &tmp);
}


struct tm *localtime_r(const time_t *timep, struct tm *res)
{
	/* TODO - use timezone information */
	return gmtime_r(timep, res);
}


struct tm *localtime(const time_t *timep)
{
	static struct tm tmp;

	return localtime_r(timep, &tmp);
}


char *ctime_r(const time_t *timep, char *buf)
{
	struct tm t, *tp;

	tp = localtime_r(timep, &t);

	return (tp == NULL) ? NULL : asctime_r(&t, buf);
}


char *ctime(const time_t *timep)
{
	static char buff[32];

	return ctime_r(timep, buff);
}

/* Calculate number of leap days between 1970-01-01 and year-01-01 */
static int leapcount(int year)
{
	/* Center on 2000-01-01 for the calculations. There are 8 leap days between 1970-01-01 and that date.
	 * Also subtract 1 because while 2000 is a leap year, its leap day isn't counted yet at 2000-01-01.
	 */
	int leap_days = 8;
	year -= 2001;
	if (year < 0) {
		/* If year is negative, push it into the positive and compensate by subtracting the appropriate
		 * number of leap days from the result. This avoids dealing with the C division operator on negative numbers.
		 */
		int periods = (year - 399) / 400;
		year -= periods * 400;
		leap_days += periods * 97;
	}

	leap_days += year / 400;
	leap_days -= year / 100;
	leap_days += year / 4;

	return leap_days;
}


static time_t _mktimeSkel(const struct tm *tp)
{
	int year = tp->tm_year - 70, leap, i, month;
	time_t res, days;

	year += tp->tm_mon / 12;
	month = tp->tm_mon % 12;
	if (month < 0) {
		year -= 1;
		month += 12;
	}

	days = (time_t)year * 365 + leapcount(year + 1970);
	leap = isleap(year + 1970);
	for (i = 0; i < month; ++i) {
		days += daysofmonth(i, leap);
	}

	days += tp->tm_mday - 1;
	res = days * 24 * 60 * 60;
	res += (tp->tm_hour * 60 + tp->tm_min) * 60;
	res += tp->tm_sec;

	return res;
}


time_t mktime(struct tm *tp)
{
	time_t res;

	tzset();

	res = _mktimeSkel(tp) + timezone - ((daylight && tp->tm_isdst > 0) ? 3600 : 0);
	if (localtime_r(&res, tp) == NULL) {
		return -1;
	}

	return res;
}


time_t timelocal(struct tm *tm)
{
	return mktime(tm);
}


time_t timegm(struct tm *tm)
{
	time_t res;

	res = _mktimeSkel(tm);
	if (gmtime_r(&res, tm) == NULL) {
		return -1;
	}

	return res;
}


static int strftime_weeksInYear(int y)
{
	int p = (y + y / 4 - y / 100 + y / 400) % 7;
	int p1 = ((y - 1) + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400) % 7;
	return ((p == 4) || (p1 == 3)) ? 53 : 52;
}


static int strftime_isoWeek(const struct tm *tm, int *isoYear)
{
	int year = tm->tm_year + 1900;
	int wday = (tm->tm_wday + 6) % 7; /* ISO weekday: Monday = 0 .. Sunday = 6 */
	int week = (tm->tm_yday - wday + 10) / 7;

	if (week < 1) {
		year -= 1;
		week = strftime_weeksInYear(year);
	}
	else if (week > strftime_weeksInYear(year)) {
		year += 1;
		week = 1;
	}

	if (isoYear != NULL) {
		*isoYear = year;
	}

	return week;
}


size_t strftime(char *__restrict s, size_t maxsize, const char *__restrict format, const struct tm *__restrict timeptr)
{
	size_t size = 0;
	const char *c = format;
	struct tm time;
	char buf[64];
	int isoYear;

	while (size < maxsize) {
		if (*c != '%') {
			if (*c == '\0') {
				s[size] = '\0';
				return size;
			}
			s[size++] = *c++;
			continue;
		}

		/* '%' -> optional flags, then an optional decimal field width */
		c++;
		int padOverride = 0; /* 0 = use the specifier's default pad char */
		for (;;) {
			if (*c == '0') {
				padOverride = '0';
			}
			else if (*c == '_') {
				padOverride = ' ';
			}
			else if ((*c == '-') || (*c == '^') || (*c == '#')) {
				/* accepted but not otherwise honoured */
			}
			else {
				break;
			}
			c++;
		}

		int width = 0;
		while ((*c >= '0') && (*c <= '9')) {
			width = width * 10 + (*c - '0');
			c++;
		}

		/* Numeric specifiers put the RAW number in buf and set a default minimum
		 * width + pad char; string/composite specifiers put their natural text in
		 * buf with defMinWidth 0. The field is padded to max(width, defMinWidth). */
		int defMinWidth = 0;
		int defPad = '0';
		int skip = 0;
		buf[0] = '\0';

		switch (*c) {
			case 'a':
				snprintf(buf, sizeof(buf), "%.3s", wdayasc[timeptr->tm_wday < 7 ? timeptr->tm_wday : 7]);
				defPad = ' ';
				break;
			case 'A':
				snprintf(buf, sizeof(buf), "%s", wdayasc[timeptr->tm_wday < 7 ? timeptr->tm_wday : 7]);
				defPad = ' ';
				break;
			case 'b':
			case 'h':
				snprintf(buf, sizeof(buf), "%.3s", monasc[timeptr->tm_mon < 12 ? timeptr->tm_mon : 12]);
				defPad = ' ';
				break;
			case 'B':
				snprintf(buf, sizeof(buf), "%s", monasc[timeptr->tm_mon < 12 ? timeptr->tm_mon : 12]);
				defPad = ' ';
				break;
			case 'c':
				snprintf(buf, sizeof(buf), "%.3s %.3s %2u %02u:%02u:%02u %u",
						wdayasc[timeptr->tm_wday < 7 ? timeptr->tm_wday : 7],
						monasc[timeptr->tm_mon < 12 ? timeptr->tm_mon : 12],
						timeptr->tm_mday, timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec,
						1900 + timeptr->tm_year);
				defPad = ' ';
				break;
			case 'C':
				snprintf(buf, sizeof(buf), "%u", (1900 + timeptr->tm_year) / 100);
				defMinWidth = 2;
				break;
			case 'd':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_mday);
				defMinWidth = 2;
				break;
			case 'e':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_mday);
				defMinWidth = 2;
				defPad = ' ';
				break;
			case 'D':
			case 'x':
				snprintf(buf, sizeof(buf), "%02u/%02u/%02u",
						timeptr->tm_mon < 12 ? timeptr->tm_mon + 1 : 13, timeptr->tm_mday, timeptr->tm_year % 100);
				defPad = ' ';
				break;
			case 'F':
				snprintf(buf, sizeof(buf), "%u-%02u-%02u", 1900 + timeptr->tm_year,
						timeptr->tm_mon < 12 ? timeptr->tm_mon + 1 : 13, timeptr->tm_mday);
				defPad = ' ';
				break;
			case 'H':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_hour);
				defMinWidth = 2;
				break;
			case 'I':
				snprintf(buf, sizeof(buf), "%u", (timeptr->tm_hour % 12 == 0) ? 12u : (unsigned)(timeptr->tm_hour % 12));
				defMinWidth = 2;
				break;
			case 'j':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_yday + 1);
				defMinWidth = 3;
				break;
			case 'm':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_mon < 12 ? timeptr->tm_mon + 1 : 13);
				defMinWidth = 2;
				break;
			case 'M':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_min);
				defMinWidth = 2;
				break;
			case 'p':
				snprintf(buf, sizeof(buf), "%s", timeptr->tm_hour < 12 ? "AM" : "PM");
				defPad = ' ';
				break;
			case 'R':
				snprintf(buf, sizeof(buf), "%02u:%02u", timeptr->tm_hour, timeptr->tm_min);
				defPad = ' ';
				break;
			case 'r':
				snprintf(buf, sizeof(buf), "%02u:%02u:%02u %s",
						(timeptr->tm_hour % 12 == 0) ? 12u : (unsigned)(timeptr->tm_hour % 12),
						timeptr->tm_min, timeptr->tm_sec, timeptr->tm_hour < 12 ? "AM" : "PM");
				defPad = ' ';
				break;
			case 'S':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_sec);
				defMinWidth = 2;
				break;
			case 's':
				memcpy(&time, timeptr, sizeof(struct tm));
				snprintf(buf, sizeof(buf), "%llu", mktime(&time));
				defMinWidth = 1;
				break;
			case 'T':
			case 'X':
				snprintf(buf, sizeof(buf), "%02u:%02u:%02u", timeptr->tm_hour, timeptr->tm_min, timeptr->tm_sec);
				defPad = ' ';
				break;
			case 'u':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_wday == 0 ? 7u : (unsigned)timeptr->tm_wday);
				defMinWidth = 1;
				break;
			case 'w':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_wday < 7 ? timeptr->tm_wday : 7);
				defMinWidth = 1;
				break;
			case 'U':
				snprintf(buf, sizeof(buf), "%u", (unsigned)((timeptr->tm_yday + 7 - timeptr->tm_wday) / 7));
				defMinWidth = 2;
				break;
			case 'W':
				snprintf(buf, sizeof(buf), "%u",
						(unsigned)((timeptr->tm_yday + 7 - (timeptr->tm_wday == 0 ? 6 : timeptr->tm_wday - 1)) / 7));
				defMinWidth = 2;
				break;
			case 'V':
				snprintf(buf, sizeof(buf), "%u", (unsigned)strftime_isoWeek(timeptr, NULL));
				defMinWidth = 2;
				break;
			case 'g':
				(void)strftime_isoWeek(timeptr, &isoYear);
				snprintf(buf, sizeof(buf), "%u", (unsigned)(isoYear % 100));
				defMinWidth = 2;
				break;
			case 'G':
				(void)strftime_isoWeek(timeptr, &isoYear);
				snprintf(buf, sizeof(buf), "%u", (unsigned)isoYear);
				defMinWidth = 1;
				break;
			case 'Y':
				snprintf(buf, sizeof(buf), "%u", 1900 + timeptr->tm_year);
				defMinWidth = 1;
				break;
			case 'y':
				snprintf(buf, sizeof(buf), "%u", timeptr->tm_year % 100);
				defMinWidth = 2;
				break;
			case 'z':
				snprintf(buf, sizeof(buf), "+0000");
				defPad = ' ';
				break;
			case 'n':
				buf[0] = '\n';
				buf[1] = '\0';
				defPad = ' ';
				break;
			case 't':
				buf[0] = '\t';
				buf[1] = '\0';
				defPad = ' ';
				break;
			case 'Z':
				skip = 1; /* no timezone name available -> emit nothing */
				break;
			case '%':
				buf[0] = '%';
				buf[1] = '\0';
				defPad = ' ';
				break;
			case '\0':
				/* trailing '%': emit it literally and stop */
				if (size + 1 >= maxsize) {
					return 0;
				}
				s[size++] = '%';
				s[size] = '\0';
				return size;
			default:
				/* unknown specifier: emit "%<c>" verbatim */
				snprintf(buf, sizeof(buf), "%%%c", *c);
				defPad = ' ';
				break;
		}

		c++; /* consume the specifier character */

		if (skip != 0) {
			continue;
		}

		size_t len = strlen(buf);
		size_t fieldWidth = ((size_t)width > (size_t)defMinWidth) ? (size_t)width : (size_t)defMinWidth;
		int padCh = (padOverride != 0) ? padOverride : defPad;
		size_t pad = (fieldWidth > len) ? (fieldWidth - len) : 0;

		/* need room for pad + len chars plus the terminating NUL */
		if ((pad + len) >= (maxsize - size)) {
			return 0;
		}

		while (pad-- > 0) {
			s[size++] = (char)padCh;
		}
		size_t i;
		for (i = 0; i < len; i++) {
			s[size++] = buf[i];
		}
	}

	return 0;
}


clock_t clock(void)
{
	return (clock_t)-1;
}

/* Parse up to maxdigits decimal digits (skipping leading blanks) from *sp into
 * *out; advance *sp. Returns digits consumed (0 = none = fail). */
static int strptime_num(const char **sp, int maxdigits, int *out)
{
	const char *s = *sp;
	int n = 0, v = 0;

	while (*s == ' ' || *s == '\t') {
		s++;
	}
	while (n < maxdigits && *s >= '0' && *s <= '9') {
		v = v * 10 + (*s - '0');
		s++;
		n++;
	}
	if (n == 0) {
		return 0;
	}
	*out = v;
	*sp = s;
	return n;
}


/* Case-insensitively match one of `count` names (full name or 3-char abbrev)
 * at *sp; on match advance *sp and return the index, else return -1. */
static int strptime_name(const char **sp, const char names[][10], int count)
{
	const char *s = *sp;
	int i, j, a, b;

	for (i = 0; i < count; i++) {
		for (j = 0; names[i][j] != '\0'; j++) {
			a = s[j]; b = names[i][j];
			if (a >= 'A' && a <= 'Z') { a += 32; }
			if (b >= 'A' && b <= 'Z') { b += 32; }
			if (a != b) { break; }
		}
		if (names[i][j] == '\0') { /* full-name match */
			*sp = s + j;
			return i;
		}
		for (j = 0; j < 3; j++) { /* 3-char abbreviation */
			a = s[j]; b = names[i][j];
			if (a >= 'A' && a <= 'Z') { a += 32; }
			if (b >= 'A' && b <= 'Z') { b += 32; }
			if (a != b) { break; }
		}
		if (j == 3) {
			*sp = s + 3;
			return i;
		}
	}
	return -1;
}


char *strptime(const char *__restrict buf, const char *__restrict format, struct tm *__restrict tm)
{
	const char *b = buf, *f = format;
	int v, idx;

	while (*f != '\0') {
		if (*f == '%') {
			f++;
			switch (*f) {
				case 'Y': /* year with century */
					if (strptime_num(&b, 4, &v) == 0) { return NULL; }
					tm->tm_year = v - 1900;
					break;
				case 'y': /* year within century (POSIX: 69-99 => 19xx, 0-68 => 20xx) */
					if (strptime_num(&b, 2, &v) == 0) { return NULL; }
					tm->tm_year = (v < 69) ? (v + 100) : v;
					break;
				case 'm':
					if (strptime_num(&b, 2, &v) == 0 || v < 1 || v > 12) { return NULL; }
					tm->tm_mon = v - 1;
					break;
				case 'd':
				case 'e':
					if (strptime_num(&b, 2, &v) == 0 || v < 1 || v > 31) { return NULL; }
					tm->tm_mday = v;
					break;
				case 'H':
					if (strptime_num(&b, 2, &v) == 0 || v < 0 || v > 23) { return NULL; }
					tm->tm_hour = v;
					break;
				case 'M':
					if (strptime_num(&b, 2, &v) == 0 || v < 0 || v > 59) { return NULL; }
					tm->tm_min = v;
					break;
				case 'S':
					if (strptime_num(&b, 2, &v) == 0 || v < 0 || v > 60) { return NULL; }
					tm->tm_sec = v;
					break;
				case 'j':
					if (strptime_num(&b, 3, &v) == 0 || v < 1 || v > 366) { return NULL; }
					tm->tm_yday = v - 1;
					break;
				case 'a':
				case 'A':
					idx = strptime_name(&b, wdayasc, 7);
					if (idx < 0) { return NULL; }
					tm->tm_wday = idx;
					break;
				case 'b':
				case 'B':
				case 'h':
					idx = strptime_name(&b, monasc, 12);
					if (idx < 0) { return NULL; }
					tm->tm_mon = idx;
					break;
				case 'n':
				case 't':
					while (*b == ' ' || *b == '\t' || *b == '\n') { b++; }
					break;
				case '%':
					if (*b != '%') { return NULL; }
					b++;
					break;
				default: /* unsupported directive */
					return NULL;
			}
			f++;
		}
		else if (*f == ' ' || *f == '\t' || *f == '\n') {
			while (*b == ' ' || *b == '\t' || *b == '\n') { b++; }
			f++;
		}
		else {
			if (*b != *f) { return NULL; }
			b++;
			f++;
		}
	}

	return (char *)b;
}


extern int nsleep(time_t *sec, long *nsec, int clockid, int flags);


int nanosleep(const struct timespec *req, struct timespec *rem)
{
	time_t sec = req->tv_sec;
	long nsec = req->tv_nsec;
	int ret;

	ret = nsleep(&sec, &nsec, CLOCK_MONOTONIC, 0);

	if (ret == -EINTR && rem != NULL) {
		rem->tv_sec = sec;
		rem->tv_nsec = nsec;
	}

	return SET_ERRNO(ret);
}


/* clock_nanosleep() shall return **positive** errors codes directly, without setting errno */
int clock_nanosleep(clockid_t clock, int flags, const struct timespec *req, struct timespec *rem)
{
	time_t sec = req->tv_sec;
	long nsec = req->tv_nsec;

	if (clock != CLOCK_MONOTONIC) {
		/* Only CLOCK_MONOTONIC supported for now */
		return EINVAL;
	}

	int ret = nsleep(&sec, &nsec, clock, flags);

	if ((ret == -EINTR) && (rem != NULL) && ((flags & TIMER_ABSTIME) == 0)) {
		rem->tv_sec = sec;
		rem->tv_nsec = nsec;
	}

	return -ret; /* ret's either zero or negative */
}
