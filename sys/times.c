/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/times.h
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/types.h>
#include <sys/times.h>
#include <time.h>


clock_t times(struct tms *buffer)
{
	struct timespec ts;

	if (buffer != NULL) {
		/* Per-process/thread CPU accounting is not available yet; zero the
		 * breakdown so callers read defined values (was left undefined). */
		buffer->tms_utime = 0;
		buffer->tms_stime = 0;
		buffer->tms_cutime = 0;
		buffer->tms_cstime = 0;
	}

	/* POSIX: return elapsed real time in clock ticks (sysconf(_SC_CLK_TCK)==100).
	 * Use the monotonic clock so successive calls advance (was a stub -> 0, so
	 * every elapsed-time measurement read 0). */
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
		return (clock_t)-1;
	}
	return (clock_t)((clock_t)ts.tv_sec * 100 + ts.tv_nsec / 10000000L);
}
