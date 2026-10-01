/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Floating-point environment for architectures without their own
 * implementation (see <fenv.h>): no exception flags, round to nearest only.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <fenv.h>


#ifndef __ARCH_FENV


int feclearexcept(int excepts)
{
	return (excepts == 0) ? 0 : -1;
}


int fegetexceptflag(fexcept_t *flagp, int excepts)
{
	*flagp = 0;
	return (excepts == 0) ? 0 : -1;
}


int feraiseexcept(int excepts)
{
	return (excepts == 0) ? 0 : -1;
}


int fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	(void)flagp;
	return (excepts == 0) ? 0 : -1;
}


int fetestexcept(int excepts)
{
	(void)excepts;
	return 0;
}


int fegetround(void)
{
	return FE_TONEAREST;
}


int fesetround(int round)
{
	return (round == FE_TONEAREST) ? 0 : -1;
}


int fegetenv(fenv_t *envp)
{
	*envp = 0;
	return 0;
}


int feholdexcept(fenv_t *envp)
{
	*envp = 0;
	return 0;
}


int fesetenv(const fenv_t *envp)
{
	(void)envp;
	return 0;
}


int feupdateenv(const fenv_t *envp)
{
	(void)envp;
	return 0;
}


#endif
