/* clang-format off */
#ifndef NUMERIC_SIZE_CONFIG
#define NUMERIC_SIZE_CONFIG

#if __SIZEOF_DOUBLE__ == 8
#ifndef LIBMCS_DOUBLE_IS_64BITS
#define LIBMCS_DOUBLE_IS_64BITS
#endif
#else
#ifndef LIBMCS_DOUBLE_IS_32BITS
#define LIBMCS_DOUBLE_IS_32BITS
#endif
#endif

/*
 * RPi4 fork: any long double that is not x87's 80-bit one -- also aarch64's 128-bit
 * one -- takes the 64-bit branch, as before upstream's 2026 libm rework. That branch
 * is what declares the long double functions in <math.h> and <complex.h>, and the
 * toolchain's libstdc++ was configured with them (_GLIBCXX_USE_C99_MATH_FUNCS): its
 * <cmath> does `using ::acoshl;` and friends, so without the declarations no C++
 * file that includes <cmath> compiles. (Only the phoenix libm is built here; the
 * libmcs build would alias its long double functions to the double ones.)
 */
#if __SIZEOF_LONG_DOUBLE__ == 10
#ifndef LIBMCS_LONG_DOUBLE_IS_80BITS
#define LIBMCS_LONG_DOUBLE_IS_80BITS
#endif
#else
#ifndef LIBMCS_LONG_DOUBLE_IS_64BITS
#define LIBMCS_LONG_DOUBLE_IS_64BITS
#endif
#endif

#if __SIZEOF_LONG__ == 8
#ifndef LIBMCS_LONG_IS_64BITS
#define LIBMCS_LONG_IS_64BITS
#endif
#else
#ifndef LIBMCS_LONG_IS_32BITS
#define LIBMCS_LONG_IS_32BITS
#endif
#endif

#endif
/* clang-format on */
