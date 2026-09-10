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
#ifdef LIBC_STARTUP_TRACE
#include <sys/debug.h>
#define LIBC_TRACE(s) debug("libc-init: " s "\n")
#else
#define LIBC_TRACE(s) ((void)0)
#endif


extern void _malloc_init(void);
extern int _env_init(void);
extern void _signals_init(void);
extern void _file_init(void);
extern void _errno_init(void);
extern void _atexit_init(void);
extern void _init_array(void);
extern void _pthread_init(void);


void _libc_init(void)
{
	LIBC_TRACE("enter");
	_atexit_init();
	LIBC_TRACE("atexit");
	_errno_init();
	LIBC_TRACE("errno");
	_malloc_init();
	LIBC_TRACE("malloc");
	_env_init();
	LIBC_TRACE("env");
	_signals_init();
	LIBC_TRACE("signals");
	/* The suspect: _file_init() ends in isatty(1) -> tcgetattr(), the only
	 * blocking IPC in this function. A trace that stops after "signals" and
	 * before "file" localises the hang to that ioctl. */
	_file_init();
	LIBC_TRACE("file");
	_pthread_init();
	LIBC_TRACE("pthread -> init_array");
}
