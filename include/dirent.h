/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * dirent.h
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_DIRENT_H_
#define _LIBPHOENIX_DIRENT_H_


#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


/* NOTE: these values can't be changed as they are kept on FLASH (eg. in jffs2) */
#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14


struct dirent {
	ino_t  d_ino;
	uint32_t d_type;
	uint16_t d_reclen;
	uint16_t d_namlen;
	char   d_name[];
};


/* These functions are not thread-safe */
extern struct dirent *readdir(DIR *dirp);


extern DIR *opendir(const char *dirname);


extern DIR *fdopendir(int fd);


extern void seekdir(DIR *dirp, long loc);


extern long telldir(DIR *dirp);


extern void rewinddir(DIR *dirp);


extern int closedir(DIR *dirp);


/* Lists `dir`: the entries `sel` accepts (all, if NULL), sorted with `compar`
 * (unsorted, if NULL). *namelist and each entry are malloc()ed; the caller
 * frees them. Returns the count, or -1 with errno set. */
extern int scandir(const char *dir, struct dirent ***namelist,
	int (*sel)(const struct dirent *),
	int (*compar)(const struct dirent **, const struct dirent **));


/* scandir() comparator: by d_name, in the collation order (strcoll). */
extern int alphasort(const struct dirent **a, const struct dirent **b);


#ifdef __cplusplus
}
#endif


#endif
