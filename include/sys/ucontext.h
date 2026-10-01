/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/ucontext.h
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_SYS_UCONTEXT_H_
#define _LIBPHOENIX_SYS_UCONTEXT_H_


/* ucontext_t and mcontext_t, the context an SA_SIGINFO handler receives. They
 * are defined by <signal.h>, as POSIX requires; this header exists for code
 * that includes it directly. The getcontext()/makecontext() family is not
 * provided, which is why there is no <ucontext.h>. */
#include <signal.h>


#endif
