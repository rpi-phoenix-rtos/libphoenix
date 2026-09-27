/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * execinfo.h - call stack backtraces (the de facto glibc/BSD interface)
 *
 * backtrace() records the return addresses of the active calls, innermost
 * first, starting with the caller of backtrace(). It walks the frame-pointer
 * chain, so a frame is found only for a function that keeps a frame record:
 * code built with -fno-omit-frame-pointer. The tree's default aarch64 flags
 * omit the frame pointer, so there the result is exact for the first frame and
 * may stop early or skip frames beyond it. Architectures without a walker
 * return 0 frames.
 *
 * backtrace_symbols() and backtrace_symbols_fd() do not symbolize: each frame
 * is printed as "0x<hex address>" (resolve it host-side with addr2line against
 * the unstripped ELF).
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS RPi4 port
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _EXECINFO_H_
#define _EXECINFO_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Stores up to size return addresses in buffer; returns the number stored. */
extern int backtrace(void **buffer, int size);


/* Returns an array of size strings describing buffer's addresses, allocated as
 * ONE malloc() block (the caller frees only the returned pointer), or NULL. */
extern char **backtrace_symbols(void *const *buffer, int size);


/* Writes the same strings, one per line, to fd. Uses no heap memory. */
extern void backtrace_symbols_fd(void *const *buffer, int size, int fd);


#ifdef __cplusplus
}
#endif

#endif
