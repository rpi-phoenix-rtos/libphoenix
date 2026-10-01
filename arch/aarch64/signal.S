/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Signal trampoline (aarch64)
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

.text

.globl _signal_trampoline
.type _signal_trampoline, %function
_signal_trampoline:
	/* dbg (ours, kept across the 2026-09-17 upstream signal rework): stash the
	 * interrupted cpu_context_t* and pc into globals so a userspace backtrace
	 * helper can read the interrupted pc + x29 frame pointer from inside the
	 * handler. x9/x10 are caller-saved temporaries and nothing here disturbs the
	 * frame the code below pops.
	 *
	 * Offsets follow the kernel's push order in hal_cpuPushSignal()
	 * (hal/aarch64/cpu.c), which is n, handler, oldmask, signalCtx, pc, sp, psr,
	 * siginfo, ucontext at 8 bytes each:
	 *     [sp+0]=n  [sp+8]=handler  [sp+16]=oldmask  [sp+24]=signalCtx
	 *     [sp+32]=pc  [sp+40]=sp  [sp+48]=psr  [sp+56]=siginfo  [sp+64]=ucontext
	 * siginfo is NULL, and ucontext absent, unless the handler was installed
	 * with SA_SIGINFO.
	 * The pc pushed here is the INTERRUPTED one -- signalCtx->pc has already been
	 * overwritten with the trampoline's address. */
	ldr x9, [sp, #24]       /* signalCtx (cpu_context_t *) */
	adrp x10, _dbg_signal_ctx
	add x10, x10, :lo12:_dbg_signal_ctx
	str x9, [x10]
	ldr x9, [sp, #32]       /* interrupted pc (leaf/fault site) */
	adrp x10, _dbg_signal_pc
	add x10, x10, :lo12:_dbg_signal_pc
	str x9, [x10]

	/* handler(signo, siginfo, ucontext) - a plain handler ignores the rest */
	ldp x0, x16, [sp]
	ldr x1, [sp, #56]
	mov x2, xzr
	cbz x1, 1f
	ldr x2, [sp, #64]
1:
	blr x16

	ldr x9, [sp, #56]
	cbnz x9, 2f

	ldp x0, x1, [sp, #16] /* oldmask, cpu_context_t * */
	ldp x2, x3, [sp, #32] /* pc, sp */
	ldr x4, [sp, #48]     /* psr */
	add sp, sp, #0x40

	bl sigreturn
	brk #0 /* sigreturn does not return */

2:
	/* SA_SIGINFO: the kernel restores the registers and the signal mask from
	 * the ucontext, which the handler may have changed */
	ldr x0, [sp, #24]     /* cpu_context_t * */
	ldr x1, [sp, #64]     /* ucontext_t * */
	add sp, sp, #0x50

	bl sigreturnContext
	brk #0 /* sigreturnContext does not return */
.size _signal_trampoline, .-_signal_trampoline
