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

#include <sys/debug.h>


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
	debug("libc: atexit init enter\n");
	_atexit_init();
	debug("libc: errno init enter\n");
	_errno_init();
	debug("libc: malloc init enter\n");
	_malloc_init();
	debug("libc: env init enter\n");
	_env_init();
	debug("libc: signals init enter\n");
	_signals_init();
	debug("libc: file init enter\n");
	_file_init();
	debug("libc: pthread init enter\n");
	_pthread_init();
	debug("libc: init done\n");
}
