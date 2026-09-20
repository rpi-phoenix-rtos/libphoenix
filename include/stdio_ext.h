/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stdio_ext.h
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS RPi4 port
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_STDIO_EXT_H_
#define _LIBPHOENIX_STDIO_EXT_H_

#include <stdio.h>
#include <stddef.h>

/* Solaris/glibc extensions that expose a FILE's buffer state.
 *
 * These exist because gnulib needs them, and gnulib is what every GNU package
 * is built on. Where a platform does not provide them, gnulib compiles its own
 * copies -- and those are per-libc `#if` ladders over private FILE internals
 * that end in `#error Please port ... to your platform!`. Four ports (coreutils,
 * grep, tar, gzip) each carried a patch teaching that ladder about Phoenix's
 * FILE; implementing the functions here retires all of them, and means the next
 * GNU port does not need one.
 *
 * The FILE layout they read is private to stdio/file.c, which is where they are
 * implemented -- deliberately, so the knowledge stays in one place instead of
 * being copied into each port's patch.
 */

#ifdef __cplusplus
extern "C" {
#endif


/* Bytes written to the stream but not yet flushed to the fd. */
extern size_t __fpending(FILE *stream);


/* Bytes read into the stream's buffer but not yet consumed by the caller. */
extern size_t __freadahead(FILE *stream);


/* Non-zero if the last operation on the stream was a read (or it is read-only). */
extern int __freading(FILE *stream);


/* Non-zero if the last operation on the stream was a write (or it is write-only). */
extern int __fwriting(FILE *stream);


/* Non-zero if the stream permits reading / writing. */
extern int __freadable(FILE *stream);
extern int __fwritable(FILE *stream);


/* Set the stream's error indicator (the ferror() flag). */
extern void __fseterr(FILE *stream);


/* Discard buffered data without flushing it. */
extern void __fpurge(FILE *stream);


/* Non-zero if the stream is line buffered. */
extern int __flbf(FILE *stream);


/* Size of the stream's buffer in bytes. */
extern size_t __fbufsize(FILE *stream);


/* Pointer to the unconsumed buffered read data, with its length in *sizep.
 * Returns NULL when nothing is buffered. */
extern const char *__freadptr(FILE *stream, size_t *sizep);


/* Advance the read cursor over n buffered bytes, as if they had been read. */
extern void __freadseek(FILE *stream, size_t n);


#ifdef __cplusplus
}
#endif

#endif /* _LIBPHOENIX_STDIO_EXT_H_ */
