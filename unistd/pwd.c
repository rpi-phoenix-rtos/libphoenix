/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Password related functions
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Jan Sikorski, Mateusz Niewiadomski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define PASSWD_PATH "/etc/passwd"

struct passwd pwnam;
char pw_name[NAME_MAX];
char intstr[16];
char pw_gecos[128];
char pw_dir[PATH_MAX];
char pw_shell[PATH_MAX];
char pw_passwd[128];


static int readpwentry(FILE *fp, char *pwentry, unsigned int maxlen, unsigned int islast)
{
	int c = 0;
	unsigned int i = 0, isbad = 0;

	do {
		c = fgetc(fp);
		if (c == ':' || c == '\n' || c == EOF)
			break;

		if (i < maxlen - 1)
			pwentry[i++] = c;
		else
			isbad = 1;
	} while (!isbad);
	pwentry[i] = '\0';

	if ((c == EOF || c == '\n') && !islast)
		isbad = 1;

	return isbad;
}


static inline int readuid(int readpwentry, uid_t *uid)
{
	int i, err = readpwentry;
	*uid = strtoul(intstr, NULL, 10);

	if (readpwentry == 0) {
		for (i = 0; intstr[i] != '\0'; ++i) {
			if (!isdigit(intstr[i])) {
				err = 1;
				break;
			}
		}
	}

	return err;
}


static struct passwd *getpwby(const char *name, uid_t *uid)
{
	struct passwd *retpwnam = NULL;
	FILE *fp = fopen(PASSWD_PATH, "r");

	if (fp != NULL) {
		pwnam.pw_name = pw_name;
		pwnam.pw_dir = pw_dir;
		pwnam.pw_gecos = pw_gecos;
		pwnam.pw_shell = pw_shell;
		pwnam.pw_passwd = pw_passwd;

		do {
			if (readpwentry(fp, pwnam.pw_name, NAME_MAX, 0))
				break;
			if (readpwentry(fp, pwnam.pw_passwd, sizeof(pw_passwd), 0))
				break;
			if (readuid(readpwentry(fp, intstr, sizeof(intstr), 0), &pwnam.pw_uid))
				break;
			if (readuid(readpwentry(fp, intstr, sizeof(intstr), 0), &pwnam.pw_gid))
				break;
			if (readpwentry(fp, pwnam.pw_gecos, sizeof(pw_gecos), 0))
				break;
			if (readpwentry(fp, pwnam.pw_dir, PATH_MAX, 0))
				break;
			if (readpwentry(fp, pwnam.pw_shell, PATH_MAX, 1))
				break;

			if (name != NULL && strcmp(pwnam.pw_name, name) == 0) {
				retpwnam = &pwnam;
				break;
			}
			else if (uid != NULL && pwnam.pw_uid == *uid) {
				retpwnam = &pwnam;
				break;
			}
		} while (!feof(fp));

		fclose(fp);
	}
	return retpwnam;
}


struct passwd *getpwnam(const char *name)
{
	return (name == NULL) ? NULL : getpwby(name, NULL);
}


struct passwd *getpwuid(uid_t uid)
{
	return getpwby(NULL, &uid);
}


/* Pack a passwd entry's strings into the caller-provided buffer and point pwd's
 * fields into it (POSIX-reentrant form). Returns 0 on success, ERANGE if buffer
 * is too small. */
static int pw_pack(const struct passwd *src, struct passwd *pwd, char *buffer,
		   size_t bufsize, struct passwd **result)
{
	size_t need, off = 0;
	const char *name = (src->pw_name != NULL) ? src->pw_name : "";
	const char *passwd = (src->pw_passwd != NULL) ? src->pw_passwd : "";
	const char *gecos = (src->pw_gecos != NULL) ? src->pw_gecos : "";
	const char *dir = (src->pw_dir != NULL) ? src->pw_dir : "";
	const char *shell = (src->pw_shell != NULL) ? src->pw_shell : "";

	need = strlen(name) + strlen(passwd) + strlen(gecos) + strlen(dir) + strlen(shell) + 5;
	if (need > bufsize) {
		*result = NULL;
		return ERANGE;
	}

#define PW_COPY(field, str) \
	do { \
		(field) = buffer + off; \
		strcpy(buffer + off, (str)); \
		off += strlen(str) + 1; \
	} while (0)

	PW_COPY(pwd->pw_name, name);
	PW_COPY(pwd->pw_passwd, passwd);
	PW_COPY(pwd->pw_gecos, gecos);
	PW_COPY(pwd->pw_dir, dir);
	PW_COPY(pwd->pw_shell, shell);
#undef PW_COPY

	pwd->pw_uid = src->pw_uid;
	pwd->pw_gid = src->pw_gid;

	*result = pwd;
	return 0;
}


int getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
	       size_t bufsize, struct passwd **result)
{
	struct passwd *src;

	if (name == NULL || pwd == NULL || result == NULL) {
		if (result != NULL) {
			*result = NULL;
		}
		return EINVAL;
	}

	src = getpwby(name, NULL);
	if (src == NULL) {
		*result = NULL;
		return 0; /* not found is not an error per POSIX */
	}

	return pw_pack(src, pwd, buffer, bufsize, result);
}


int getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer,
	       size_t bufsize, struct passwd **result)
{
	struct passwd *src;

	if (pwd == NULL || result == NULL) {
		if (result != NULL) {
			*result = NULL;
		}
		return EINVAL;
	}

	src = getpwby(NULL, &uid);
	if (src == NULL) {
		*result = NULL;
		return 0; /* not found is not an error per POSIX */
	}

	return pw_pack(src, pwd, buffer, bufsize, result);
}
