/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Floating-point environment (arch/aarch64)
 *
 * The rounding direction is FPCR.RMode, the exception flags are the
 * cumulative bits of FPSR; both registers are part of each thread's saved
 * context. Exception traps are not enabled by any of these functions (and
 * Cortex-A cores generally do not implement them), so raising an exception
 * only sets its flag.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <fenv.h>


#define FE_ROUND_MASK (FE_UPWARD | FE_DOWNWARD | FE_TOWARDZERO)

/* FPCR trap enables: IOE, DZE, OFE, UFE, IXE (bits 8-12) and IDE (bit 15) */
#define FPCR_TRAP_MASK 0x9f00u


static inline unsigned int fenv_getFpcr(void)
{
	unsigned long v;

	__asm__ volatile("mrs %0, fpcr" : "=r"(v));
	return (unsigned int)v;
}


static inline void fenv_setFpcr(unsigned int v)
{
	__asm__ volatile("msr fpcr, %0" : : "r"((unsigned long)v));
}


static inline unsigned int fenv_getFpsr(void)
{
	unsigned long v;

	__asm__ volatile("mrs %0, fpsr" : "=r"(v));
	return (unsigned int)v;
}


static inline void fenv_setFpsr(unsigned int v)
{
	__asm__ volatile("msr fpsr, %0" : : "r"((unsigned long)v));
}


int feclearexcept(int excepts)
{
	fenv_setFpsr(fenv_getFpsr() & ~((unsigned int)excepts & FE_ALL_EXCEPT));
	return 0;
}


int fegetexceptflag(fexcept_t *flagp, int excepts)
{
	*flagp = fenv_getFpsr() & (unsigned int)excepts & FE_ALL_EXCEPT;
	return 0;
}


int feraiseexcept(int excepts)
{
	fenv_setFpsr(fenv_getFpsr() | ((unsigned int)excepts & FE_ALL_EXCEPT));
	return 0;
}


int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	unsigned int mask = (unsigned int)excepts & FE_ALL_EXCEPT;

	fenv_setFpsr((fenv_getFpsr() & ~mask) | (*flagp & mask));
	return 0;
}


int fetestexcept(int excepts)
{
	return (int)(fenv_getFpsr() & (unsigned int)excepts & FE_ALL_EXCEPT);
}


int fegetround(void)
{
	return (int)(fenv_getFpcr() & FE_ROUND_MASK);
}


int fesetround(int round)
{
	if (((unsigned int)round & ~(unsigned int)FE_ROUND_MASK) != 0u) {
		return -1;
	}

	fenv_setFpcr((fenv_getFpcr() & ~(unsigned int)FE_ROUND_MASK) | (unsigned int)round);
	return 0;
}


int fegetenv(fenv_t *envp)
{
	envp->__fpcr = fenv_getFpcr();
	envp->__fpsr = fenv_getFpsr();
	return 0;
}


int feholdexcept(fenv_t *envp)
{
	(void)fegetenv(envp);
	fenv_setFpsr(envp->__fpsr & ~(unsigned int)FE_ALL_EXCEPT);
	fenv_setFpcr(envp->__fpcr & ~FPCR_TRAP_MASK);
	return 0;
}


int fesetenv(const fenv_t *envp)
{
	if (envp == FE_DFL_ENV) {
		/* What the kernel gives a new thread: round to nearest, no traps,
		 * no flags */
		fenv_setFpcr(0u);
		fenv_setFpsr(0u);
	}
	else {
		fenv_setFpcr(envp->__fpcr);
		fenv_setFpsr(envp->__fpsr);
	}
	return 0;
}


int feupdateenv(const fenv_t *envp)
{
	unsigned int raised = fenv_getFpsr() & FE_ALL_EXCEPT;

	(void)fesetenv(envp);
	return feraiseexcept((int)raised);
}
