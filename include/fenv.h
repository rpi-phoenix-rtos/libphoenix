/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * fenv.h - floating-point environment (C99 7.6)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_FENV_H_
#define _LIBPHOENIX_FENV_H_

#include <arch.h>


#ifdef __ARCH_FENV

/* fenv_t, fexcept_t, FE_* exception and rounding macros */
#include __ARCH_FENV

#else

/* No floating-point environment control on this architecture: no exception
 * flags are reported and only the default rounding direction is available,
 * which C99 7.6 permits. */
typedef unsigned int fenv_t;
typedef unsigned int fexcept_t;

#define FE_ALL_EXCEPT 0
#define FE_TONEAREST  0

#endif


/* The environment a program starts with */
#define FE_DFL_ENV ((const fenv_t *)-1)


#ifdef __cplusplus
extern "C" {
#endif


extern int feclearexcept(int excepts);


extern int fegetexceptflag(fexcept_t *flagp, int excepts);


extern int feraiseexcept(int excepts);


extern int fesetexceptflag(const fexcept_t *flagp, int excepts);


extern int fetestexcept(int excepts);


extern int fegetround(void);


extern int fesetround(int round);


extern int fegetenv(fenv_t *envp);


extern int feholdexcept(fenv_t *envp);


extern int fesetenv(const fenv_t *envp);


extern int feupdateenv(const fenv_t *envp);


#ifdef __cplusplus
}
#endif


#endif
