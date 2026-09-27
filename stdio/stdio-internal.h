/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Internal stdio interface: FILEs backed by functions instead of a descriptor
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTERNAL_STDIO_H_
#define _LIBPHOENIX_INTERNAL_STDIO_H_


#include <stdio.h>
#include <sys/types.h>


/* The descriptor primitives of a FILE, for a stream that has no descriptor
 * (open_memstream, fmemopen). Each behaves like the system call it replaces:
 *   read  -- up to `size` bytes; 0 at end of data; -1 + errno on error.
 *            NULL: the stream is not readable.
 *   write -- up to `size` bytes; returns the count accepted, 0 when nothing
 *            more fits (reported as ENOSPC); -1 + errno on error.
 *            NULL: the stream is not writable.
 *   seek  -- like lseek(); returns the new offset or -1 + errno.
 *            NULL: the stream is not seekable (ESPIPE).
 *   close -- releases the cookie; 0 or -1 + errno. May be NULL.
 * The stdio buffer, ungetc(), ftell() adjustment and flushing work on top of
 * these exactly as they do on top of a descriptor. */
typedef struct {
	ssize_t (*read)(void *cookie, char *buf, size_t size);
	ssize_t (*write)(void *cookie, const char *buf, size_t size);
	off_t (*seek)(void *cookie, off_t offset, int whence);
	int (*close)(void *cookie);
} file_ops_t;


/* Open a buffered FILE on `ops`. `mode` is an fopen() mode string; it sets
 * the stream's read/write permissions (O_APPEND semantics, if any, are the
 * write hook's job). Returns NULL with errno set (EINVAL, ENOMEM); the cookie
 * then still belongs to the caller. After success, fclose() calls ops->close. */
FILE *_file_openOps(const char *mode, const file_ops_t *ops, void *cookie);


#endif
