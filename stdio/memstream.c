/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Memory streams: open_memstream() and fmemopen() (POSIX.1-2008)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "stdio-internal.h"


/* Both are ordinary buffered FILEs (see stdio-internal.h) whose descriptor
 * primitives work on memory, so buffering, ungetc(), ftell() and fflush()
 * behave exactly as on a file. */


/* Resolve an lseek()-style request against a stream whose data is `len` bytes
 * long and whose position is `pos`. Returns the new position, or -1 with
 * errno = EINVAL for a bad `whence` or a result outside [0, limit]. */
static off_t memstream_seekTo(size_t pos, size_t len, size_t limit, off_t offset, int whence)
{
	off_t base;

	switch (whence) {
		case SEEK_SET:
			base = 0;
			break;
		case SEEK_CUR:
			base = (off_t)pos;
			break;
		case SEEK_END:
			base = (off_t)len;
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	if ((offset < -base) || ((offset > 0) && ((size_t)offset > limit - (size_t)base))) {
		errno = EINVAL;
		return -1;
	}

	return base + offset;
}


/* ------------------------------------------------------------------------
 * open_memstream(): a write-only stream into a buffer that grows as needed.
 *
 * Invariants: len <= cap - 1, and buf[len .. cap) is all zero bytes -- so
 * the data is always NUL-terminated, and seeking past the end then writing
 * leaves the gap zero-filled, as POSIX requires.
 * ------------------------------------------------------------------------ */

typedef struct {
	char **bufp;
	size_t *sizep;
	char *buf;
	size_t cap;
	size_t len; /* bytes of data: the furthest any write has reached */
	size_t pos;
} memstream_t;


/* POSIX: after fflush()/fclose(), *bufp is the buffer and *sizep the smaller
 * of the data length and the current position. Kept current on every write
 * and seek, which the flush of the FILE buffer always goes through. */
static void memstream_publish(memstream_t *ms)
{
	*ms->bufp = ms->buf;
	*ms->sizep = (ms->pos < ms->len) ? ms->pos : ms->len;
}


static ssize_t memstream_write(void *cookie, const char *data, size_t size)
{
	memstream_t *ms = cookie;
	size_t end, ncap;
	char *nbuf;

	if ((size > (size_t)SSIZE_MAX) || (ms->pos > (size_t)SSIZE_MAX - size)) {
		errno = EFBIG;
		return -1;
	}
	end = ms->pos + size;

	if (end >= ms->cap) {
		ncap = (ms->cap <= (size_t)SSIZE_MAX / 2u) ? 2u * ms->cap : (size_t)SSIZE_MAX;
		if (ncap <= end) {
			ncap = end + 1u;
		}
		nbuf = realloc(ms->buf, ncap);
		if (nbuf == NULL) {
			return -1; /* errno = ENOMEM from realloc() */
		}
		memset(nbuf + ms->cap, 0, ncap - ms->cap);
		ms->buf = nbuf;
		ms->cap = ncap;
	}

	memcpy(ms->buf + ms->pos, data, size);
	ms->pos = end;
	if (end > ms->len) {
		ms->len = end;
	}

	memstream_publish(ms);
	return (ssize_t)size;
}


static off_t memstream_seek(void *cookie, off_t offset, int whence)
{
	memstream_t *ms = cookie;
	off_t npos = memstream_seekTo(ms->pos, ms->len, (size_t)SSIZE_MAX, offset, whence);

	if (npos >= 0) {
		ms->pos = (size_t)npos;
		memstream_publish(ms);
	}

	return npos;
}


static int memstream_close(void *cookie)
{
	memstream_t *ms = cookie;

	memstream_publish(ms);
	/* The result is *sizep bytes: terminate it there, not only at len, so a
	 * caller that seeked back before closing still gets a C string of that
	 * length (as glibc does). The buffer now belongs to the caller. */
	ms->buf[*ms->sizep] = '\0';
	free(ms);

	return 0;
}


static const file_ops_t memstream_ops = {
	.read = NULL, /* write-only */
	.write = memstream_write,
	.seek = memstream_seek,
	.close = memstream_close,
};


FILE *open_memstream(char **bufp, size_t *sizep)
{
	memstream_t *ms;
	FILE *f;

	if ((bufp == NULL) || (sizep == NULL)) {
		errno = EINVAL;
		return NULL;
	}

	ms = calloc(1, sizeof(*ms));
	if (ms == NULL) {
		return NULL;
	}

	ms->cap = 64u;
	ms->buf = calloc(1, ms->cap);
	if (ms->buf == NULL) {
		free(ms);
		return NULL;
	}
	ms->bufp = bufp;
	ms->sizep = sizep;

	f = _file_openOps("w", &memstream_ops, ms);
	if (f == NULL) {
		free(ms->buf);
		free(ms);
		return NULL;
	}

	/* Valid from the start, so a close with nothing written yields "". */
	memstream_publish(ms);

	return f;
}


/* ------------------------------------------------------------------------
 * fmemopen(): a stream over a caller-supplied (or private) fixed buffer.
 *
 * `len` is the current size of the contents, `size` the capacity. Reads stop
 * at len, writes at size, and seeks may go anywhere in [0, size].
 * ------------------------------------------------------------------------ */

typedef struct {
	char *buf;
	size_t size;
	size_t len;
	size_t pos;
	int append;   /* mode 'a': every write goes to the end of the contents */
	int readable; /* '+' or mode 'r' */
	int owned;    /* buf was allocated here (buf == NULL was passed) */
} fmemstream_t;


static ssize_t fmem_read(void *cookie, char *data, size_t size)
{
	fmemstream_t *fm = cookie;
	size_t n;

	if (fm->pos >= fm->len) {
		return 0;
	}

	n = fm->len - fm->pos;
	if (n > size) {
		n = size;
	}
	memcpy(data, fm->buf + fm->pos, n);
	fm->pos += n;

	return (ssize_t)n;
}


static ssize_t fmem_write(void *cookie, const char *data, size_t size)
{
	fmemstream_t *fm = cookie;
	size_t n;

	if (fm->append != 0) {
		fm->pos = fm->len;
	}

	n = (fm->pos < fm->size) ? (fm->size - fm->pos) : 0u;
	if (n > size) {
		n = size;
	}
	if (n == 0u) {
		return 0; /* full: stdio reports ENOSPC */
	}

	memcpy(fm->buf + fm->pos, data, n);
	fm->pos += n;

	/* POSIX: when the contents grow, a NUL follows them if it fits; a stream
	 * open for writing only puts it in the last byte of the buffer when they
	 * fill it. (An update stream never overwrites data for it.) */
	if (fm->pos > fm->len) {
		fm->len = fm->pos;
		if (fm->len < fm->size) {
			fm->buf[fm->len] = '\0';
		}
		else if (fm->readable == 0) {
			fm->buf[fm->size - 1u] = '\0';
		}
	}

	return (ssize_t)n;
}


static off_t fmem_seek(void *cookie, off_t offset, int whence)
{
	fmemstream_t *fm = cookie;
	off_t npos = memstream_seekTo(fm->pos, fm->len, fm->size, offset, whence);

	if (npos >= 0) {
		fm->pos = (size_t)npos;
	}

	return npos;
}


static int fmem_close(void *cookie)
{
	fmemstream_t *fm = cookie;

	if (fm->owned != 0) {
		free(fm->buf);
	}
	free(fm);

	return 0;
}


static const file_ops_t fmem_ops = {
	.read = fmem_read,
	.write = fmem_write,
	.seek = fmem_seek,
	.close = fmem_close,
};


FILE *fmemopen(void *restrict buf, size_t size, const char *restrict mode)
{
	fmemstream_t *fm;
	FILE *f;

	/* size 0: POSIX "may fail" with EINVAL; musl does, glibc does not */
	if ((mode == NULL) || (mode[0] == '\0') || (strchr("rwa", mode[0]) == NULL) || (size == 0u)) {
		errno = EINVAL;
		return NULL;
	}

	fm = calloc(1, sizeof(*fm));
	if (fm == NULL) {
		return NULL;
	}

	if (buf == NULL) {
		fm->buf = calloc(1, size);
		if (fm->buf == NULL) {
			free(fm);
			return NULL;
		}
		fm->owned = 1;
	}
	else {
		fm->buf = buf;
	}

	fm->size = size;
	fm->readable = ((mode[0] == 'r') || (strchr(mode, '+') != NULL)) ? 1 : 0;

	switch (mode[0]) {
		case 'r':
			fm->len = size; /* the whole buffer is readable, NULs included */
			break;
		case 'w':
			fm->len = 0; /* truncate */
			if (fm->readable != 0) {
				fm->buf[0] = '\0'; /* "w+" also empties the string (musl, glibc) */
			}
			break;
		default: /* 'a': position at the first NUL, or the end of the buffer */
			fm->len = strnlen(fm->buf, size);
			fm->pos = fm->len;
			fm->append = 1;
			break;
	}

	f = _file_openOps(mode, &fmem_ops, fm);
	if (f == NULL) {
		if (fm->owned != 0) {
			free(fm->buf);
		}
		free(fm);
		return NULL;
	}

	return f;
}
