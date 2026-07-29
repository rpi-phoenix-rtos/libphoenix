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
	/* Signal number, oldmask - keep stack aligned to 16 */
	ldp x0, x19, [sp], #0x10
	/* dbg: stash the interrupted cpu_context_t* (signalCtx, now at [sp]) into the global
	 * _dbg_signal_ctx so a userspace backtrace helper can read the interrupted pc + x29 frame
	 * pointer. x9/x10 are caller-saved temporaries; x0 (signal number) is preserved. */
	ldr x9, [sp]
	adrp x10, _dbg_signal_ctx
	add x10, x10, :lo12:_dbg_signal_ctx
	str x9, [x10]
	ldr x9, [sp, #8]        /* interrupted pc (leaf/fault site); signalCtx->pc is clobbered w/ handler */
	adrp x10, _dbg_signal_pc
	add x10, x10, :lo12:_dbg_signal_pc
	str x9, [x10]
	bl _signal_handler

	mov x0, x19             /* move oldmask to x0 */
	ldp x1, x2, [sp], #0x10 /* cpu_context_t *, pc */
	ldp x3, x4, [sp], #0x10 /* sp, psr */
	bl sigreturn
.size _signal_trampoline, .-_signal_trampoline
