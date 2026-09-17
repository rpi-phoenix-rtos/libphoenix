/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * init
 *
 * Copyright 2021, 2023 Phoenix Systems
 * Author: Hubert Buczynski, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


/* Pre-main progress trace, OFF unless built with -DLIBC_STARTUP_TRACE.
 *
 * Why it exists: an app fails to start about 1 run in 8 on the RPi4 netboot
 * target, producing NO output at all after psh echoes the command and never
 * returning psh's prompt -- so it hangs before main(). The two pre-main steps are
 * _libc_init() below and _init_array(), and _init_array() is already ruled out
 * (that binary's .init_array holds 7 entries: 4 compiler-generated, 3 that only
 * construct a static object and register a destructor via __cxa_atexit -- no
 * syscalls). Inside _libc_init() exactly ONE call can block: _file_init() ends
 * with isatty(stdout->fd), i.e. tcgetattr(), an ioctl to the tty THROUGH A PORT.
 * Everything else here is calloc and mutexCreate.
 *
 * Why compile-time and not an env var: psh cannot set environment variables, so a
 * runtime-gated trace is unreachable for an app invoked bare. debug() is a raw
 * kernel syscall (ID(debug)) with no stdio, no malloc and no locking, so it is
 * safe to call before any of this initialisation has run -- which an fprintf is
 * emphatically not.
 *
 * Enable for a diagnostic build only; it prints per process and would be noise in
 * a shipped image.
 */
/* Two levels, because the full trace may be perturbing what it measures.
 *
 * LIBC_STARTUP_TRACE     -- all eight markers. Names the initialiser a stall
 *                           happens in, at the cost of 8 debug() syscalls per
 *                           process before anything else runs.
 * LIBC_STARTUP_TRACE_MIN -- the entry marker ONLY: one syscall.
 *
 * Why the minimal level exists: the fault this hunts (a launch that produces no
 * output at all, `premain-hang`) needs a COLD boot and a HEAVY launch, and the
 * full trace has now failed to reproduce it twice -- 0 events in 48 traced
 * cold+heavy launches against 9 in 214 untraced. That is suggestive of the
 * instrument masking a timing-sensitive race, though not significant on its own
 * (p = 0.37). Since no silent run has ever reached even the first marker, seven
 * of the eight are cost without information: the open question is only whether
 * the process reaches _libc_init AT ALL, or dies earlier in exec/loading. One
 * marker answers that with an eighth of the perturbation.
 */
#if defined(LIBC_STARTUP_TRACE) || defined(LIBC_STARTUP_TRACE_MIN)
#include <sys/debug.h>
#define LIBC_TRACE_ENTER() debug("libc-init: enter\n")
#else
#define LIBC_TRACE_ENTER() ((void)0)
#endif

#ifdef LIBC_STARTUP_TRACE
#define LIBC_TRACE(s) debug("libc-init: " s "\n")
#else
#define LIBC_TRACE(s) ((void)0)
#endif


extern void _malloc_init(void);
extern int _env_init(void);
extern void _file_init(void);
extern void _errno_init(void);
extern void _atexit_init(void);
extern void _init_array(void);
extern void _pthread_init(void);
extern void _stat_init(void);


void _libc_init(void)
{
	LIBC_TRACE_ENTER();
	_atexit_init();
	LIBC_TRACE("atexit");
	_errno_init();
	LIBC_TRACE("errno");
	_malloc_init();
	LIBC_TRACE("malloc");
	_env_init();
	LIBC_TRACE("env");
	/* The suspect: _file_init() ends in isatty(1) -> tcgetattr(), the only
	 * blocking IPC in this function. A trace that stops after "env" and before
	 * "file" localises the hang to that ioctl. (It used to say "after signals";
	 * _signals_init() is gone with the move of signal handling into the kernel.) */
	_file_init();
	LIBC_TRACE("file");
	_pthread_init();
	LIBC_TRACE("pthread");
	_stat_init();
	LIBC_TRACE("stat -> init_array");
}
