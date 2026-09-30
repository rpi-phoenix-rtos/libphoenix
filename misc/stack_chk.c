/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Stack protector runtime (GCC -fstack-protector ABI)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/debug.h>
#include <sys/time.h>


/*
 * Code built with -fstack-protector* stores __stack_chk_guard in each protected
 * frame on entry and calls __stack_chk_fail() if the copy changed by the time the
 * function returns. GCC on aarch64 (and the other Phoenix targets) reads the global
 * directly; there is no TLS or system-register canary unless the code is built with
 * -mstack-protector-guard=sysreg, which nothing here does.
 *
 * The guard is set ONCE, by _stack_chk_init(), as the first thing _startc() does.
 * It must never change afterwards: every live protected frame holds a copy of it.
 * fork() copies it with the rest of .data and vfork() shares it, both of which is
 * what the child's inherited frames need; exec() starts a new image that seeds its
 * own.
 *
 * The static value is only what the guard holds if seeding produced zero. It has a
 * zero low byte for the same reason the seeded value does (see below).
 */
uintptr_t __stack_chk_guard = (uintptr_t)0xe3a8d5c60f1b7400ULL;


/* splitmix64 finaliser: spreads the few changing bits of each input across all 64 */
static uint64_t stack_chk_mix(uint64_t h, uint64_t v)
{
	h += v + 0x9e3779b97f4a7c15ULL;
	h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
	h = (h ^ (h >> 27)) * 0x94d049bb133111ebULL;
	return h ^ (h >> 31);
}


/*
 * Called from _startc() before anything else, so before any protected function has
 * been entered. It must not be protected itself: it would store the old guard on
 * entry and compare it against the new one on return. Everything it calls returns
 * before the guard is written.
 *
 * TODO(TD-28): the kernel offers no entropy to a new process: no AT_RANDOM-style
 * word on the initial stack and no random syscall. /dev/urandom is not an option
 * this early: it is an IPC to a driver that the first processes (the root
 * filesystem server, the tty, the RNG driver itself) start before, and a pre-main
 * IPC is exactly what hung process start before. So the seed is a mix of what a
 * syscall or a register can give: the virtual counter (EL0 reads are enabled by
 * the kernel, hal/aarch64/_init.S), the boot-relative time in microseconds, the
 * pid and the startup addresses. Without ASLR the addresses are constant and the
 * pid is sequential; the counter and the time carry the entropy, which is enough
 * to stop a fixed exploit string, not an attacker who can measure the start time.
 * Replace with random bytes from the kernel when it passes them.
 */
__attribute__((no_stack_protector)) void _stack_chk_init(char **argv, char **env)
{
	uint64_t h = 0;
	time_t raw = 0, offs = 0;
	uintptr_t guard;

#ifdef __aarch64__
	uint64_t cnt;

	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(cnt));
	h = stack_chk_mix(h, cnt);
#endif

	(void)gettime(&raw, &offs);
	h = stack_chk_mix(h, (uint64_t)raw);
	h = stack_chk_mix(h, (uint64_t)offs);
	h = stack_chk_mix(h, (uint64_t)getpid());
	h = stack_chk_mix(h, (uint64_t)(uintptr_t)argv);
	h = stack_chk_mix(h, (uint64_t)(uintptr_t)env);
	h = stack_chk_mix(h, (uint64_t)(uintptr_t)&h);

	/* A zero low byte (the first byte in memory on little-endian targets) stops a
	 * string-copy overflow from reproducing the guard and a string read from leaking
	 * the rest of it -- the same choice glibc makes. */
	guard = (uintptr_t)h & ~(uintptr_t)0xff;
	if (guard != 0) {
		__stack_chk_guard = guard;
	}
}


/*
 * Stack is already corrupt here, so: no stdio (its buffers and locks may be the
 * victims), no malloc, one write() of a message built on this frame, then abort().
 * Processes without an open stderr (early servers) get the message on the kernel
 * console instead.
 */
__attribute__((noreturn, no_stack_protector)) void __stack_chk_fail(void)
{
	static const char head[] = "*** stack smashing detected ***: ";
	static const char tail[] = " terminated\n";
	char msg[128];
	const char *name = getprogname();
	size_t len = sizeof(head) - 1, n;

	memcpy(msg, head, len);
	n = strlen(name);
	if (n > sizeof(msg) - len - sizeof(tail)) {
		n = sizeof(msg) - len - sizeof(tail);
	}
	memcpy(msg + len, name, n);
	len += n;
	memcpy(msg + len, tail, sizeof(tail)); /* with the terminating NUL, for debug() */
	len += sizeof(tail) - 1;

	if (write(STDERR_FILENO, msg, len) != (ssize_t)len) {
		debug(msg);
	}

	abort();
}
