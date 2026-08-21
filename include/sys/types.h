/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * types.h
 *
 * Copyright 2018, 2019, 2024 Phoenix Systems
 * Author: Jan Sikorski, Marcin Baran, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <arch.h>
#include <stddef.h>
#include <stdint.h>

#include <phoenix/types.h>

#ifdef __cplusplus
/* Do NOT pull in <atomic> here: this header is reached (via <pthread.h>) by
 * libstdc++'s own -std=gnu++98 source TUs, and <atomic> hard-errors under c++98
 * (bits/c++0x_warning.h). Since _ATOMIC is a plain int in C++ (below), <atomic>
 * is not needed anyway. */
/* In C++ these POSIX types (pthread_mutex_t/cond_t/rwlock_t below) must stay
 * COPYABLE: gcc-16's libstdc++ <ext/concurrence.h> copy-initializes the underlying
 * __gthread_* type from PTHREAD_MUTEX_INITIALIZER, and std::atomic has a DELETED
 * copy ctor — which made libstdc++ fail to compile under gcc-16 ("use of deleted
 * function std::atomic<int>::atomic(const std::atomic<int>&)"). Use a plain,
 * layout-compatible int (same size/alignment as the C `_Atomic int`); the actual
 * atomic accesses on these fields live entirely in libphoenix's C sources
 * (pthread.c), never in C++, so no C++ TU ever needs atomic semantics here. */
#define _ATOMIC(type) type
#else
#include <stdatomic.h>
#define _ATOMIC(type) _Atomic(type)
#endif

typedef int clock_t;
typedef int clockid_t;

typedef unsigned int useconds_t;
typedef int suseconds_t;

typedef char *caddr_t;

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;

typedef struct pthread_attr_t {
	void *stackaddr;
	int schedpolicy;
	int priority;
	int detachstate;
	int inheritsched;
	size_t stacksize;
	size_t guardsize;
	/* scope is always PTHREAD_SCOPE_SYSTEM */
} pthread_attr_t;

typedef uintptr_t pthread_t;

typedef struct {
	handle_t mutexh;
	_ATOMIC(int) initialized;
} pthread_mutex_t;

typedef struct {
	handle_t lock;
	handle_t readCond;
	handle_t writeCond;
	size_t readActive;
	size_t writeActive;
	size_t writeWaiting;
	_ATOMIC(int) initialized;
} pthread_rwlock_t;


typedef struct {
	int pshared;
} pthread_rwlockattr_t;


typedef struct lockAttr pthread_mutexattr_t;


typedef struct {
	handle_t condh;
	_ATOMIC(int) initialized;
} pthread_cond_t;


typedef struct pthread_condattr_t {
	int pshared;
	clockid_t clock_id;
} pthread_condattr_t;

typedef struct __pthread_key_t *pthread_key_t;

typedef uint32_t pthread_once_t;

/* BSD legacy types permitted by POSIX */
typedef uint8_t   u_int8_t;
typedef uint16_t  u_int16_t;
typedef uint32_t  u_int32_t;
typedef uint64_t  u_int64_t;
typedef int register_t;

#endif
