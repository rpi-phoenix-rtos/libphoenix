/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * assert.h
 *
 * Copyright 2017, 2026 Phoenix Systems
 * Author: Pawel Pisarczyk, Michal Lach, Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * NOTE: <assert.h> is intentionally NOT protected by a once-only include
 * guard. The C standard requires the assert() macro to be (re)defined on
 * every inclusion according to the CURRENT definition of NDEBUG, so that a
 * translation unit can toggle NDEBUG and re-include the header. A permanent
 * guard would define assert only once and breaks wrappers that rely on this
 * (e.g. gnulib's <assert.h> substitute, which does #include_next <assert.h>).
 */

#include <stdio.h>
#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * `static_assert` is a keyword in C++ since C++11 and in C since C23. C11 through
 * C17 spell it `_Static_assert` and require <assert.h> to provide the macro below,
 * which is what makes the name usable from both languages.
 */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif


#undef assert

#ifndef NDEBUG
#define assert(__expr) \
	((__expr) ? (void)0 : ({ fprintf(stderr, "Assertion '%s' failed in file %s:%d, function %s.\n", #__expr, __FILE__, __LINE__, __func__); abort(); }))
#else
#define assert(__expr) ((void)0)
#endif /* NDEBUG */


/* C11 7.2p3: <assert.h> defines static_assert as _Static_assert.
 * Not in C++, where static_assert is a keyword (a macro would break libstdc++),
 * and not in C23, where it is a keyword as well. Unlike assert() it does not
 * depend on NDEBUG, so a definition that is already in place (from an earlier
 * inclusion, or a port's own compatibility header) is left alone. */
#if !defined(__cplusplus) && !defined(static_assert) && defined(__STDC_VERSION__) && \
	(__STDC_VERSION__ >= 201112L) && (__STDC_VERSION__ < 202311L)
#define static_assert _Static_assert
#endif


#ifdef __cplusplus
}
#endif
