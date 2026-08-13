/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * mntent.h
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_MNTENT_H_
#define _LIBPHOENIX_MNTENT_H_

#include <stdio.h>


#define MNTTAB  "/etc/fstab"
#define MOUNTED "/etc/mtab"

#define MNTTYPE_IGNORE "ignore"
#define MNTTYPE_NFS    "nfs"
#define MNTTYPE_SWAP   "swap"

#define MNTOPT_DEFAULTS "defaults"
#define MNTOPT_RO       "ro"
#define MNTOPT_RW       "rw"
#define MNTOPT_SUID     "suid"
#define MNTOPT_NOSUID   "nosuid"
#define MNTOPT_NOAUTO   "noauto"


#ifdef __cplusplus
extern "C" {
#endif


struct mntent {
	char *mnt_fsname; /* device or remote filesystem */
	char *mnt_dir;    /* mount point */
	char *mnt_type;   /* filesystem type */
	char *mnt_opts;   /* comma-separated mount options */
	int mnt_freq;     /* dump frequency, in days */
	int mnt_passno;   /* fsck pass number */
};


FILE *setmntent(const char *filename, const char *type);
struct mntent *getmntent(FILE *stream);
struct mntent *getmntent_r(FILE *stream, struct mntent *result, char *buffer, int bufsize);
int addmntent(FILE *stream, const struct mntent *mnt);
int endmntent(FILE *stream);
char *hasmntopt(const struct mntent *mnt, const char *opt);


#ifdef __cplusplus
}
#endif


#endif /* _LIBPHOENIX_MNTENT_H_ */
