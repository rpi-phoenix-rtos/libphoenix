/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Floating-point environment (arch/aarch64)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_ARCH_AARCH64_FENV_H_
#define _LIBPHOENIX_ARCH_AARCH64_FENV_H_


/* The control (FPCR) and status (FPSR) registers */
typedef struct {
	unsigned int __fpcr;
	unsigned int __fpsr;
} fenv_t;

typedef unsigned int fexcept_t;


/* FPSR cumulative exception flags */
#define FE_INVALID    0x01
#define FE_DIVBYZERO  0x02
#define FE_OVERFLOW   0x04
#define FE_UNDERFLOW  0x08
#define FE_INEXACT    0x10
#define FE_ALL_EXCEPT 0x1f

/* FPCR.RMode */
#define FE_TONEAREST  0x000000
#define FE_UPWARD     0x400000
#define FE_DOWNWARD   0x800000
#define FE_TOWARDZERO 0xc00000


#endif
