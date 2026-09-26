/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * inttypes.h
 *
 * Copyright 2019 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_INTTYPES_H_
#define _LIBPHOENIX_INTTYPES_H_


#include <arch.h>
#include <stdint.h>

/* wchar_t only, for wcstoimax()/wcstoumax() (C99 7.8.2.4) */
#define __need_wchar_t
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __ARCH_INTTYPES
#include __ARCH_INTTYPES
#endif

#ifdef _USE_STANDARD_TYPES_STDINT

#define PRId8 _PRI_8 "d"
#define PRIu8 _PRI_8 "u"
#define PRIx8 _PRI_8 "x"
#define PRIX8 _PRI_8 "X"
#define PRIi8 _PRI_8 "i"
#define PRIo8 _PRI_8 "o"

#define PRId16 _PRI_16 "d"
#define PRIu16 _PRI_16 "u"
#define PRIx16 _PRI_16 "x"
#define PRIX16 _PRI_16 "X"
#define PRIi16 _PRI_16 "i"
#define PRIo16 _PRI_16 "o"

#define PRId32 _PRI_32 "d"
#define PRIu32 _PRI_32 "u"
#define PRIx32 _PRI_32 "x"
#define PRIX32 _PRI_32 "X"
#define PRIi32 _PRI_32 "i"
#define PRIo32 _PRI_32 "o"

#define PRId64 _PRI_64 "d"
#define PRIu64 _PRI_64 "u"
#define PRIx64 _PRI_64 "x"
#define PRIX64 _PRI_64 "X"
#define PRIi64 _PRI_64 "i"
#define PRIo64 _PRI_64 "o"

#define PRIdLEAST8 PRId8
#define PRIuLEAST8 PRIu8
#define PRIxLEAST8 PRIx8
#define PRIXLEAST8 PRIX8
#define PRIiLEAST8 PRIi8
#define PRIoLEAST8 PRIo8

#define PRIdLEAST16 PRId16
#define PRIuLEAST16 PRIu16
#define PRIxLEAST16 PRIx16
#define PRIXLEAST16 PRIX16
#define PRIiLEAST16 PRIi16
#define PRIoLEAST16 PRIo16

#define PRIdLEAST32 PRId32
#define PRIuLEAST32 PRIu32
#define PRIxLEAST32 PRIx32
#define PRIXLEAST32 PRIX32
#define PRIiLEAST32 PRIi32
#define PRIoLEAST32 PRIo32

#define PRIdLEAST64 PRId64
#define PRIuLEAST64 PRIu64
#define PRIxLEAST64 PRIx64
#define PRIXLEAST64 PRIX64
#define PRIiLEAST64 PRIi64
#define PRIoLEAST64 PRIo64

/* The fast types differ per architecture (int_fast8_t is 8 bits on ia32 and 32
 * bits elsewhere), and _SCN_FAST* already names the length modifier that
 * matches each one. hh/h are valid printf modifiers too, so the same prefix
 * serves both directions and stays -Wformat clean on every arch. */
#define PRIdFAST8 _SCN_FAST8 "d"
#define PRIuFAST8 _SCN_FAST8 "u"
#define PRIxFAST8 _SCN_FAST8 "x"
#define PRIXFAST8 _SCN_FAST8 "X"
#define PRIiFAST8 _SCN_FAST8 "i"
#define PRIoFAST8 _SCN_FAST8 "o"

#define PRIdFAST16 _SCN_FAST16 "d"
#define PRIuFAST16 _SCN_FAST16 "u"
#define PRIxFAST16 _SCN_FAST16 "x"
#define PRIXFAST16 _SCN_FAST16 "X"
#define PRIiFAST16 _SCN_FAST16 "i"
#define PRIoFAST16 _SCN_FAST16 "o"

#define PRIdFAST32 _SCN_FAST32 "d"
#define PRIuFAST32 _SCN_FAST32 "u"
#define PRIxFAST32 _SCN_FAST32 "x"
#define PRIXFAST32 _SCN_FAST32 "X"
#define PRIiFAST32 _SCN_FAST32 "i"
#define PRIoFAST32 _SCN_FAST32 "o"

#define PRIdFAST64 _SCN_FAST64 "d"
#define PRIuFAST64 _SCN_FAST64 "u"
#define PRIxFAST64 _SCN_FAST64 "x"
#define PRIXFAST64 _SCN_FAST64 "X"
#define PRIiFAST64 _SCN_FAST64 "i"
#define PRIoFAST64 _SCN_FAST64 "o"

#define PRIdMAX _PRI_64 "d"
#define PRIiMAX _PRI_64 "i"
#define PRIoMAX _PRI_64 "o"
#define PRIuMAX _PRI_64 "u"
#define PRIxMAX _PRI_64 "x"
#define PRIXMAX _PRI_64 "X"

#define PRIdPTR _PRI_PTR "d"
#define PRIuPTR _PRI_PTR "u"
#define PRIxPTR _PRI_PTR "x"
#define PRIXPTR _PRI_PTR "X"
#define PRIiPTR _PRI_PTR "i"
#define PRIoPTR _PRI_PTR "o"

#define SCNd8 _SCN_8 "d"
#define SCNu8 _SCN_8 "u"
#define SCNx8 _SCN_8 "x"
#define SCNi8 _SCN_8 "i"
#define SCNo8 _SCN_8 "o"

#define SCNdLEAST8 _SCN_LEAST8 "d"
#define SCNuLEAST8 _SCN_LEAST8 "u"
#define SCNxLEAST8 _SCN_LEAST8 "x"
#define SCNiLEAST8 _SCN_LEAST8 "i"
#define SCNoLEAST8 _SCN_LEAST8 "o"

#define SCNdFAST8 _SCN_FAST8 "d"
#define SCNuFAST8 _SCN_FAST8 "u"
#define SCNxFAST8 _SCN_FAST8 "x"
#define SCNiFAST8 _SCN_FAST8 "i"
#define SCNoFAST8 _SCN_FAST8 "o"

#define SCNd16 _SCN_16 "d"
#define SCNu16 _SCN_16 "u"
#define SCNx16 _SCN_16 "x"
#define SCNi16 _SCN_16 "i"
#define SCNo16 _SCN_16 "o"

#define SCNdLEAST16 _SCN_LEAST16 "d"
#define SCNuLEAST16 _SCN_LEAST16 "u"
#define SCNxLEAST16 _SCN_LEAST16 "x"
#define SCNiLEAST16 _SCN_LEAST16 "i"
#define SCNoLEAST16 _SCN_LEAST16 "o"

#define SCNdFAST16 _SCN_FAST16 "d"
#define SCNuFAST16 _SCN_FAST16 "u"
#define SCNxFAST16 _SCN_FAST16 "x"
#define SCNiFAST16 _SCN_FAST16 "i"
#define SCNoFAST16 _SCN_FAST16 "o"

#define SCNd32 _SCN_32 "d"
#define SCNu32 _SCN_32 "u"
#define SCNx32 _SCN_32 "x"
#define SCNi32 _SCN_32 "i"
#define SCNo32 _SCN_32 "o"

#define SCNdLEAST32 _SCN_LEAST32 "d"
#define SCNuLEAST32 _SCN_LEAST32 "u"
#define SCNxLEAST32 _SCN_LEAST32 "x"
#define SCNiLEAST32 _SCN_LEAST32 "i"
#define SCNoLEAST32 _SCN_LEAST32 "o"

#define SCNdFAST32 _SCN_FAST32 "d"
#define SCNuFAST32 _SCN_FAST32 "u"
#define SCNxFAST32 _SCN_FAST32 "x"
#define SCNiFAST32 _SCN_FAST32 "i"
#define SCNoFAST32 _SCN_FAST32 "o"

#define SCNd64 _SCN_64 "d"
#define SCNu64 _SCN_64 "u"
#define SCNx64 _SCN_64 "x"
#define SCNi64 _SCN_64 "i"
#define SCNo64 _SCN_64 "o"

#define SCNdLEAST64 _SCN_LEAST64 "d"
#define SCNuLEAST64 _SCN_LEAST64 "u"
#define SCNxLEAST64 _SCN_LEAST64 "x"
#define SCNiLEAST64 _SCN_LEAST64 "i"
#define SCNoLEAST64 _SCN_LEAST64 "o"

#define SCNdFAST64 _SCN_FAST64 "d"
#define SCNuFAST64 _SCN_FAST64 "u"
#define SCNxFAST64 _SCN_FAST64 "x"
#define SCNiFAST64 _SCN_FAST64 "i"
#define SCNoFAST64 _SCN_FAST64 "o"

/* intmax_t is int64_t on every supported architecture */
#define SCNdMAX _SCN_64 "d"
#define SCNuMAX _SCN_64 "u"
#define SCNxMAX _SCN_64 "x"
#define SCNiMAX _SCN_64 "i"
#define SCNoMAX _SCN_64 "o"

/* scanf and printf use the same length modifier for int and long */
#define SCNdPTR _PRI_PTR "d"
#define SCNuPTR _PRI_PTR "u"
#define SCNxPTR _PRI_PTR "x"
#define SCNiPTR _PRI_PTR "i"
#define SCNoPTR _PRI_PTR "o"

#endif

typedef struct {
	intmax_t quot;
	intmax_t rem;
} imaxdiv_t;


extern intmax_t imaxabs(intmax_t j);


extern imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);


extern intmax_t strtoimax(const char *nptr, char **endptr, int base);


extern uintmax_t strtoumax(const char *nptr, char **endptr, int base);


extern intmax_t wcstoimax(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


extern uintmax_t wcstoumax(const wchar_t *__restrict nptr, wchar_t **__restrict endptr, int base);


#ifdef __cplusplus
}
#endif


#endif
