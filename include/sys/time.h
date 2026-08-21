/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/time.h
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <sys/select.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};


extern int gettime(time_t *raw, time_t *offs);


extern int settime(time_t offs);


extern int gettimeofday(struct timeval *tp, void *tzp);


extern int stime(const time_t *t);


/* For compability with Linux software */
extern int settimeofday(const struct timeval *tv, void *tz);


extern int utimes(const char *filename, const struct timeval times[2]);


extern int futimes(int fd, const struct timeval tv[2]);


extern int lutimes(const char *filename, const struct timeval tv[2]);


extern int timerisset(struct timeval *tvp);


#ifdef __cplusplus
}
#endif


/* Per POSIX/BSD, timercmp's arguments are `struct timeval *` — use -> not .
 * (the previous `.` form silently broke every standard caller, e.g. readline). */
#define timercmp(a, b, CMP) \
	(((a)->tv_sec == (b)->tv_sec && (a)->tv_usec CMP (b)->tv_usec) || (a)->tv_sec CMP (b)->tv_sec)


/* Standard POSIX/BSD sys/time.h timeval helpers (were missing — ports that use
 * them, e.g. libevent/tmux/gettimeofday-diff code, failed to compile). */
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)

#define timeradd(a, b, result) \
	do { \
		(result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
		(result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
		if ((result)->tv_usec >= 1000000) { \
			++(result)->tv_sec; \
			(result)->tv_usec -= 1000000; \
		} \
	} while (0)

#define timersub(a, b, result) \
	do { \
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
		(result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
		if ((result)->tv_usec < 0) { \
			--(result)->tv_sec; \
			(result)->tv_usec += 1000000; \
		} \
	} while (0)


#endif /* _SYS_TIME_H_ */
