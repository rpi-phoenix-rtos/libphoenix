/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * scandir(), alphasort() (POSIX.1-2008)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


int alphasort(const struct dirent **a, const struct dirent **b)
{
	return strcoll((*a)->d_name, (*b)->d_name);
}


/* qsort() hands the comparator pointers to the array ELEMENTS, which are
 * struct dirent pointers, i.e. exactly the `const struct dirent **` the
 * scandir() comparator takes -- but calling it through a cast function
 * pointer is undefined, so bounce through this. qsort has no context
 * argument, and scandir() may run in several threads, hence the TLS. */
static __thread int (*scandir_compar)(const struct dirent **, const struct dirent **);


static int scandir_cmp(const void *a, const void *b)
{
	return scandir_compar((const struct dirent **)a, (const struct dirent **)b);
}


int scandir(const char *dir, struct dirent ***namelist,
	int (*sel)(const struct dirent *),
	int (*compar)(const struct dirent **, const struct dirent **))
{
	DIR *d;
	struct dirent *de, *copy, **list = NULL, **nlist;
	size_t count = 0, cap = 0, sz;
	int err;

	d = opendir(dir);
	if (d == NULL) {
		return -1;
	}

	while ((de = readdir(d)) != NULL) {
		if ((sel != NULL) && (sel(de) == 0)) {
			continue;
		}

		if (count == cap) {
			cap = (cap == 0u) ? 16u : 2u * cap;
			if (cap > (size_t)INT_MAX) {
				errno = EOVERFLOW;
				goto fail;
			}
			nlist = realloc(list, cap * sizeof(*list));
			if (nlist == NULL) {
				goto fail;
			}
			list = nlist;
		}

		/* d_name is a flexible array: size each copy to its own name */
		sz = offsetof(struct dirent, d_name) + strlen(de->d_name) + 1u;
		copy = malloc(sz);
		if (copy == NULL) {
			goto fail;
		}
		memcpy(copy, de, sz);
		list[count++] = copy;
	}
	closedir(d);

	if ((compar != NULL) && (count > 1u)) {
		scandir_compar = compar;
		qsort(list, count, sizeof(*list), scandir_cmp);
	}

	*namelist = list;
	return (int)count;

fail:
	err = errno;
	while (count > 0u) {
		free(list[--count]);
	}
	free(list);
	closedir(d);
	errno = err;
	return -1;
}
