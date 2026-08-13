/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * mntent.c — filesystem-table (fstab/mtab) access.
 *
 * Reads whitespace-separated entries from a mount table opened with
 * setmntent(). Phoenix does not maintain /etc/mtab by default, so on a
 * typical system setmntent() simply fails to open it and the caller sees an
 * empty table — enough for portable software (e.g. gnulib's mountlist, used
 * by GNU coreutils' df) to build and link and degrade gracefully.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


FILE *setmntent(const char *filename, const char *type)
{
	return fopen(filename, type);
}


int endmntent(FILE *stream)
{
	if (stream != NULL) {
		(void)fclose(stream);
	}
	return 1;
}


struct mntent *getmntent_r(FILE *stream, struct mntent *result, char *buffer, int bufsize)
{
	char *freq, *passno;

	if (stream == NULL || result == NULL || buffer == NULL) {
		return NULL;
	}

	/* Skip blank lines and comments. */
	do {
		if (fgets(buffer, bufsize, stream) == NULL) {
			return NULL;
		}
	} while (buffer[0] == '#' || buffer[0] == '\n' || buffer[0] == '\0');

	result->mnt_freq = 0;
	result->mnt_passno = 0;

	result->mnt_fsname = strtok(buffer, " \t\n");
	result->mnt_dir = strtok(NULL, " \t\n");
	result->mnt_type = strtok(NULL, " \t\n");
	result->mnt_opts = strtok(NULL, " \t\n");
	freq = strtok(NULL, " \t\n");
	passno = strtok(NULL, " \t\n");

	if (freq != NULL) {
		result->mnt_freq = atoi(freq);
	}
	if (passno != NULL) {
		result->mnt_passno = atoi(passno);
	}

	/* A valid entry needs at least a device and a mount point. */
	if (result->mnt_fsname == NULL || result->mnt_dir == NULL) {
		return NULL;
	}

	return result;
}


struct mntent *getmntent(FILE *stream)
{
	static struct mntent mnt;
	static char linebuf[512];

	return getmntent_r(stream, &mnt, linebuf, (int)sizeof(linebuf));
}


int addmntent(FILE *stream, const struct mntent *mnt)
{
	if (stream == NULL || mnt == NULL) {
		return 1;
	}
	if (fprintf(stream, "%s %s %s %s %d %d\n",
			mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts,
			mnt->mnt_freq, mnt->mnt_passno) < 0) {
		return 1;
	}
	return 0;
}


char *hasmntopt(const struct mntent *mnt, const char *opt)
{
	size_t optlen;
	const char *p;

	if (mnt == NULL || mnt->mnt_opts == NULL || opt == NULL) {
		return NULL;
	}

	optlen = strlen(opt);
	p = mnt->mnt_opts;
	while (p != NULL && *p != '\0') {
		if (strncmp(p, opt, optlen) == 0 && (p[optlen] == '\0' || p[optlen] == ',' || p[optlen] == '=')) {
			return (char *)p;
		}
		p = strchr(p, ',');
		if (p != NULL) {
			p++;
		}
	}

	return NULL;
}
