/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * mntent.c — filesystem-table (fstab/mtab) access.
 *
 * Reads and writes whitespace-separated entries of a filesystem table. Fields
 * containing a space, tab, newline or backslash are stored with octal escapes
 * (\040, \011, \012, \134). The mount table MOUNTED is a log kept by the
 * mounters (see <mntent.h>); reading it returns only the filesystems that are
 * mounted now.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/msg.h>


#define MNTENT_SEPARATORS " \t\r\n"
#define MNTENT_LINE_MAX   1024


/* Decodes the octal escapes of one field in place. */
static void mntent_unescape(char *field)
{
	char *src = field, *dst = field;

	while (*src != '\0') {
		if ((src[0] == '\\') &&
				(src[1] >= '0') && (src[1] <= '3') &&
				(src[2] >= '0') && (src[2] <= '7') &&
				(src[3] >= '0') && (src[3] <= '7')) {
			*dst++ = (char)(((src[1] - '0') << 6) | ((src[2] - '0') << 3) | (src[3] - '0'));
			src += 4;
		}
		else {
			*dst++ = *src++;
		}
	}
	*dst = '\0';
}


static int mntent_putField(FILE *stream, const char *field)
{
	for (; *field != '\0'; field++) {
		if ((*field == ' ') || (*field == '\t') || (*field == '\n') || (*field == '\\')) {
			if (fprintf(stream, "\\%03o", (unsigned int)(unsigned char)*field) < 0) {
				return -1;
			}
		}
		else if (fputc(*field, stream) == EOF) {
			return -1;
		}
	}

	return 0;
}


/* Discards the rest of a line that did not fit in the caller's buffer. */
static void mntent_skipLine(FILE *stream)
{
	int c;

	do {
		c = fgetc(stream);
	} while ((c != '\n') && (c != EOF));
}


/*
 * Is dir (canonical, absolute) a mount point now? "/" always is; any other
 * directory is one when its contents are served by another server than those
 * of its parent. (A server answers a lookup of a mount point either with its
 * own node and the mounted root as the device, or with the mounted root for
 * both, so the device of the lookup is the server of the contents either way.)
 */
static int mntent_isMountPoint(const char *dir)
{
	oid_t oid, dev, pdev;
	char *parent, *slash;
	int err;

	if ((dir[0] == '/') && (dir[1] == '\0')) {
		return 1;
	}

	if ((dir[0] != '/') || (lookup(dir, &oid, &dev) < 0)) {
		return 0;
	}

	parent = strdup(dir);
	if (parent == NULL) {
		return 0;
	}
	slash = strrchr(parent, '/');
	slash[(slash == parent) ? 1 : 0] = '\0';

	err = lookup(parent, &oid, &pdev);
	free(parent);

	return ((err >= 0) && (dev.port != pdev.port)) ? 1 : 0;
}


static int mntent_cmpDir(const void *a, const void *b)
{
	const struct mntent *const *ea = a;
	const struct mntent *const *eb = b;

	return strcmp((*ea)->mnt_dir, (*eb)->mnt_dir);
}


/* Copies an entry into a single allocation (the strings follow the struct). */
static struct mntent *mntent_dup(const struct mntent *mnt)
{
	size_t lfsname = strlen(mnt->mnt_fsname) + 1u;
	size_t ldir = strlen(mnt->mnt_dir) + 1u;
	size_t ltype = strlen(mnt->mnt_type) + 1u;
	size_t lopts = strlen(mnt->mnt_opts) + 1u;
	struct mntent *copy = malloc(sizeof(*copy) + lfsname + ldir + ltype + lopts);
	char *p;

	if (copy == NULL) {
		return NULL;
	}

	p = (char *)(copy + 1);
	copy->mnt_fsname = memcpy(p, mnt->mnt_fsname, lfsname);
	p += lfsname;
	copy->mnt_dir = memcpy(p, mnt->mnt_dir, ldir);
	p += ldir;
	copy->mnt_type = memcpy(p, mnt->mnt_type, ltype);
	p += ltype;
	copy->mnt_opts = memcpy(p, mnt->mnt_opts, lopts);
	copy->mnt_freq = mnt->mnt_freq;
	copy->mnt_passno = mnt->mnt_passno;

	return copy;
}


/*
 * Reads the mount log at path and returns a memory stream with the mounts in
 * effect: the last entry for each directory, only for directories that are
 * mount points now, sorted by directory (so a parent precedes what is mounted
 * below it, whatever order the boot-time mounters ran in).
 */
static FILE *mntent_openMounted(const char *path)
{
	struct mntent ent, **all = NULL, **cur = NULL, **grown;
	size_t nall = 0, ncur = 0, i, j, size = 0;
	char *line, *text = NULL;
	FILE *log, *ms, *result = NULL;
	int err = 0;

	/* On the heap: a shell runs this on a small stack. */
	line = malloc(MNTENT_LINE_MAX);
	if (line == NULL) {
		return NULL;
	}

	log = fopen(path, "r");
	if (log == NULL) {
		free(line);
		return NULL;
	}

	while (getmntent_r(log, &ent, line, MNTENT_LINE_MAX) != NULL) {
		grown = realloc(all, (nall + 1u) * sizeof(*all));
		if (grown == NULL) {
			err = ENOMEM;
			break;
		}
		all = grown;
		all[nall] = mntent_dup(&ent);
		if (all[nall] == NULL) {
			err = ENOMEM;
			break;
		}
		nall++;
	}
	(void)fclose(log);

	if ((err == 0) && (nall != 0u)) {
		cur = malloc(nall * sizeof(*cur));
		if (cur == NULL) {
			err = ENOMEM;
		}
	}

	if (err == 0) {
		/* Newest first, so the first entry kept for a directory is its latest. */
		for (i = nall; i-- > 0u;) {
			for (j = 0; j < ncur; j++) {
				if (strcmp(cur[j]->mnt_dir, all[i]->mnt_dir) == 0) {
					break;
				}
			}
			if (j == ncur) {
				cur[ncur++] = all[i];
			}
		}

		if (ncur != 0u) {
			qsort(cur, ncur, sizeof(*cur), mntent_cmpDir);
		}

		ms = open_memstream(&text, &size);
		if (ms == NULL) {
			err = errno;
		}
		else {
			for (i = 0; i < ncur; i++) {
				if ((mntent_isMountPoint(cur[i]->mnt_dir) != 0) && (addmntent(ms, cur[i]) != 0)) {
					err = ENOMEM;
					break;
				}
			}
			if ((fclose(ms) != 0) && (err == 0)) {
				err = ENOMEM;
			}
		}
	}

	if (err == 0) {
		/* + 1: fmemopen() rejects a zero size, and an empty table is valid */
		result = fmemopen(NULL, size + 1u, "w+");
		if (result == NULL) {
			err = errno;
		}
		else if ((fwrite(text, 1, size, result) != size) || (fseek(result, 0, SEEK_SET) != 0)) {
			err = EIO;
			(void)fclose(result);
			result = NULL;
		}
	}

	free(line);
	free(text);
	free(cur);
	for (i = 0; i < nall; i++) {
		free(all[i]);
	}
	free(all);

	if (err != 0) {
		errno = err;
	}

	return result;
}


FILE *setmntent(const char *filename, const char *type)
{
	if ((filename == NULL) || (type == NULL)) {
		errno = EINVAL;
		return NULL;
	}

	/* Reading the mount table gives the mounts in effect, not the raw log. */
	if ((strcmp(filename, MOUNTED) == 0) && (type[0] == 'r') && (strchr(type, '+') == NULL)) {
		return mntent_openMounted(filename);
	}

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
	char *cursor, *end, *freq, *passno;
	size_t len;

	if ((stream == NULL) || (result == NULL) || (buffer == NULL) || (bufsize < 2)) {
		return NULL;
	}

	for (;;) {
		if (fgets(buffer, bufsize, stream) == NULL) {
			return NULL;
		}

		len = strlen(buffer);
		if ((len == (size_t)bufsize - 1u) && (buffer[len - 1u] != '\n') && (feof(stream) == 0)) {
			/* Longer than the buffer: skip it rather than split it in two entries. */
			mntent_skipLine(stream);
			continue;
		}

		/* An empty string for the optional fields that are missing. */
		end = buffer + len;

		result->mnt_fsname = strtok_r(buffer, MNTENT_SEPARATORS, &cursor);
		if ((result->mnt_fsname == NULL) || (result->mnt_fsname[0] == '#')) {
			continue; /* a blank line or a comment */
		}

		result->mnt_dir = strtok_r(NULL, MNTENT_SEPARATORS, &cursor);
		if (result->mnt_dir == NULL) {
			continue; /* malformed: an entry needs a device and a mount point */
		}

		result->mnt_type = strtok_r(NULL, MNTENT_SEPARATORS, &cursor);
		result->mnt_opts = strtok_r(NULL, MNTENT_SEPARATORS, &cursor);
		freq = strtok_r(NULL, MNTENT_SEPARATORS, &cursor);
		passno = strtok_r(NULL, MNTENT_SEPARATORS, &cursor);

		if (result->mnt_type == NULL) {
			result->mnt_type = end;
		}
		if (result->mnt_opts == NULL) {
			result->mnt_opts = end;
		}
		result->mnt_freq = (freq != NULL) ? atoi(freq) : 0;
		result->mnt_passno = (passno != NULL) ? atoi(passno) : 0;

		mntent_unescape(result->mnt_fsname);
		mntent_unescape(result->mnt_dir);
		mntent_unescape(result->mnt_type);
		mntent_unescape(result->mnt_opts);

		return result;
	}
}


struct mntent *getmntent(FILE *stream)
{
	static struct mntent mnt;
	static char linebuf[MNTENT_LINE_MAX];

	return getmntent_r(stream, &mnt, linebuf, (int)sizeof(linebuf));
}


int addmntent(FILE *stream, const struct mntent *mnt)
{
	const char *type, *opts;

	if ((stream == NULL) || (mnt == NULL) ||
			(mnt->mnt_fsname == NULL) || (mnt->mnt_fsname[0] == '\0') ||
			(mnt->mnt_dir == NULL) || (mnt->mnt_dir[0] == '\0')) {
		return 1;
	}

	/* An empty field would merge with the next one when read back. */
	type = ((mnt->mnt_type != NULL) && (mnt->mnt_type[0] != '\0')) ? mnt->mnt_type : MNTTYPE_NONE;
	opts = ((mnt->mnt_opts != NULL) && (mnt->mnt_opts[0] != '\0')) ? mnt->mnt_opts : MNTOPT_DEFAULTS;

	if ((mntent_putField(stream, mnt->mnt_fsname) < 0) || (fputc(' ', stream) == EOF) ||
			(mntent_putField(stream, mnt->mnt_dir) < 0) || (fputc(' ', stream) == EOF) ||
			(mntent_putField(stream, type) < 0) || (fputc(' ', stream) == EOF) ||
			(mntent_putField(stream, opts) < 0) ||
			(fprintf(stream, " %d %d\n", mnt->mnt_freq, mnt->mnt_passno) < 0)) {
		return 1;
	}

	return 0;
}


char *hasmntopt(const struct mntent *mnt, const char *opt)
{
	size_t optlen;
	const char *p;

	if ((mnt == NULL) || (mnt->mnt_opts == NULL) || (opt == NULL)) {
		return NULL;
	}

	optlen = strlen(opt);
	p = mnt->mnt_opts;
	while ((p != NULL) && (*p != '\0')) {
		if ((strncmp(p, opt, optlen) == 0) && ((p[optlen] == '\0') || (p[optlen] == ',') || (p[optlen] == '='))) {
			return (char *)p;
		}
		p = strchr(p, ',');
		if (p != NULL) {
			p++;
		}
	}

	return NULL;
}


int mount_record(const char *source, const char *target, const char *fstype, const char *opts)
{
	struct mntent ent;
	char dir[] = MOUNTED;
	char *slash = strrchr(dir, '/');
	FILE *table;
	int err = 0;

	if ((source == NULL) || (target == NULL) || (fstype == NULL)) {
		errno = EINVAL;
		return -1;
	}

	/* Until devfs is mounted on the table's directory, opening the table would
	 * create it in the filesystem underneath, hidden once devfs is mounted. */
	if ((slash != NULL) && (slash != dir)) {
		*slash = '\0';
		if (mntent_isMountPoint(dir) == 0) {
			errno = EAGAIN;
			return -1;
		}
	}

	ent.mnt_fsname = (char *)source;
	ent.mnt_dir = (char *)target;
	ent.mnt_type = (char *)fstype;
	ent.mnt_opts = (char *)((opts != NULL) ? opts : MNTOPT_DEFAULTS);
	ent.mnt_freq = 0;
	ent.mnt_passno = 0;

	/* One line is one append, which the server applies whole. */
	table = setmntent(MOUNTED, "a");
	if (table == NULL) {
		return -1; /* errno set by fopen() */
	}

	if ((addmntent(table, &ent) != 0) || (fflush(table) != 0)) {
		err = (errno != 0) ? errno : EIO;
	}
	(void)endmntent(table);

	if (err != 0) {
		errno = err;
		return -1;
	}

	return 0;
}
