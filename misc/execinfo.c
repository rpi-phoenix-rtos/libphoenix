/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * execinfo.c - backtrace(), backtrace_symbols(), backtrace_symbols_fd()
 *
 * Copyright 2026 Phoenix Systems
 * Author: Phoenix-RTOS RPi4 port
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>


/* "0x" + one hex digit per nibble of an address + NUL */
#define BT_ADDR_STRLEN (2 + (2 * sizeof(uintptr_t)) + 1)

/* Upper bound on the distance between two consecutive frame records. A frame
 * pointer beyond it is taken for a corrupt chain, or for x29 used as a general
 * register by code built without frame pointers, and ends the walk before it
 * is dereferenced. */
#define BT_MAX_FRAME_SIZE (4UL * 1024UL * 1024UL)


int backtrace(void **buffer, int size)
{
#if defined(__aarch64__)
	/* AAPCS64 frame record: fp[0] = the caller's frame pointer, fp[1] = the
	 * return address. __builtin_frame_address(0) makes this function build a
	 * record of its own, so the first entry is always our caller's. */
	const uintptr_t *fp = __builtin_frame_address(0);
	uintptr_t next;
	int n = 0;

	if (buffer == NULL) {
		return 0;
	}

	while ((fp != NULL) && (n < size)) {
		if (fp[1] == 0) {
			break;
		}
		buffer[n++] = (void *)fp[1];

		/* The stack grows down: an older frame is at a higher address. */
		next = fp[0];
		if ((next <= (uintptr_t)fp) || ((next - (uintptr_t)fp) > BT_MAX_FRAME_SIZE) ||
				((next & (sizeof(uintptr_t) - 1)) != 0)) {
			break;
		}
		fp = (const uintptr_t *)next;
	}

	return n;
#else
	/* No frame walker for this architecture. */
	(void)buffer;
	(void)size;

	return 0;
#endif
}


/* Formats addr as "0x<hex>" into buf (BT_ADDR_STRLEN bytes); returns the length. */
static size_t bt_formatAddr(char *buf, const void *addr)
{
	static const char digits[] = "0123456789abcdef";
	uintptr_t v = (uintptr_t)addr;
	char tmp[2 * sizeof(uintptr_t)];
	size_t n = 0, len;

	do {
		tmp[n++] = digits[v & 0xfU];
		v >>= 4;
	} while (v != 0);

	buf[0] = '0';
	buf[1] = 'x';
	for (len = 2; n > 0; len++) {
		buf[len] = tmp[--n];
	}
	buf[len] = '\0';

	return len;
}


char **backtrace_symbols(void *const *buffer, int size)
{
	char **strings;
	char *str;
	int i;

	if ((buffer == NULL) || (size < 0) ||
			((size_t)size > ((SIZE_MAX - 1U) / (sizeof(char *) + BT_ADDR_STRLEN)))) {
		return NULL;
	}

	/* One block, as glibc: the pointer array followed by the strings. */
	strings = malloc(((size_t)size * sizeof(char *)) + ((size_t)size * BT_ADDR_STRLEN) + 1U);
	if (strings == NULL) {
		return NULL;
	}

	str = (char *)(strings + size);
	for (i = 0; i < size; i++) {
		strings[i] = str;
		str += bt_formatAddr(str, buffer[i]) + 1U;
	}

	return strings;
}


void backtrace_symbols_fd(void *const *buffer, int size, int fd)
{
	char line[BT_ADDR_STRLEN];
	size_t len, done;
	ssize_t ret;
	int i;

	if (buffer == NULL) {
		return;
	}

	for (i = 0; i < size; i++) {
		len = bt_formatAddr(line, buffer[i]);
		line[len++] = '\n';

		for (done = 0; done < len; done += (size_t)ret) {
			ret = write(fd, line + done, len - done);
			if (ret <= 0) {
				return;
			}
		}
	}
}
