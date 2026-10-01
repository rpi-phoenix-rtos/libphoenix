/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * uchar.h - UTF-16 and UTF-32 characters (C11 7.28)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_UCHAR_H_
#define _LIBPHOENIX_UCHAR_H_

#include <stddef.h>
#include <wchar.h> /* mbstate_t */


#ifdef __cplusplus
extern "C" {
#else
/* C++11 has these as keywords */
typedef __CHAR16_TYPE__ char16_t;
typedef __CHAR32_TYPE__ char32_t;
#endif


/*
 * The conversions follow the rest of the multibyte layer (see mbrtowc()):
 * the only locale is C/POSIX, in which every byte is one character, so a
 * byte converts to the code point of the same value -- 0x80-0xff become
 * U+0080-U+00FF (ISO 8859-1) -- and back. Nothing else can be written:
 * c16rtomb() and c32rtomb() fail with EILSEQ above U+00FF, surrogates
 * included. In particular this is not UTF-8.
 */
extern size_t mbrtoc16(char16_t *__restrict pc16, const char *__restrict s, size_t n, mbstate_t *__restrict ps);


extern size_t c16rtomb(char *__restrict s, char16_t c16, mbstate_t *__restrict ps);


extern size_t mbrtoc32(char32_t *__restrict pc32, const char *__restrict s, size_t n, mbstate_t *__restrict ps);


extern size_t c32rtomb(char *__restrict s, char32_t c32, mbstate_t *__restrict ps);


#ifdef __cplusplus
}
#endif


#endif
