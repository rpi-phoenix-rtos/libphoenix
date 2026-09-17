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
	 * (hal/aarch64/cpu.c), which is n, handler, oldmask, signalCtx, pc, sp, psr
	 * at 8 bytes each:
	 *     [sp+0]=n  [sp+8]=handler  [sp+16]=oldmask  [sp+24]=signalCtx
	 *     [sp+32]=pc  [sp+40]=sp  [sp+48]=psr
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

	/* Signal number, handler address - stack aligned to 16 */
	ldp x0, x1, [sp], #0x10
	blr x1

	ldp x0, x1, [sp], #0x10 /* oldmask, cpu_context_t * */
	ldp x2, x3, [sp], #0x10 /* pc, sp */
	ldr x4, [sp], #0x10 /* psr, skip padding at the end */

	bl sigreturn
.size _signal_trampoline, .-_signal_trampoline
