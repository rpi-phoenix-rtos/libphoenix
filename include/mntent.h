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


/*
 * MOUNTED: the table of mounted filesystems.
 *
 * Phoenix keeps no mount table in the kernel: a filesystem server attaches
 * itself to a directory of another filesystem, or registers itself as "/".
 * The table is therefore a file the mounters append to -- mount() and the
 * servers that mount themselves (see mount_record() in <sys/mount.h>). It lives
 * on devfs, which is in RAM, so every boot starts with an empty table.
 *
 * The file is a log, not a snapshot: a later entry for the same directory
 * replaces an earlier one, and an unmount writes nothing. setmntent(MOUNTED, "r")
 * therefore returns the filesystems mounted now -- one entry per directory, the
 * latest, for directories that are still mount points, sorted by directory.
 */
#define MNTTAB  "/etc/fstab"
#define MOUNTED "/dev/mtab"

#define MNTTYPE_IGNORE "ignore"
#define MNTTYPE_NONE   "none"
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
