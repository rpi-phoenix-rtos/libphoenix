/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * Doubly-linked list
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <sys/list.h>
#include <stdint.h>


void lib_listAdd(void **list, void *t, size_t noff, size_t poff)
{
	if (t == NULL)
		return;
	if (*list == NULL) {
		*((uintptr_t *)(t + noff)) = (uintptr_t)t;
		*((uintptr_t *)(t + poff)) = (uintptr_t)t;
		*list = t;
	}
	else {
		*((uintptr_t *)(t + poff)) = *((uintptr_t *)(*list + poff));
		*((uintptr_t *)((void *)*((uintptr_t *)(*list + poff)) + noff)) = (uintptr_t)t;
		*((uintptr_t *)(t + noff)) = *((uintptr_t *)list);
		*((uintptr_t *)(*list + poff)) = (uintptr_t)t;
	}
}


void lib_listRemove(void **list, void *t, size_t noff, size_t poff)
{
	uintptr_t next, prev;

	if (t == NULL)
		return;

	next = *((uintptr_t *)(t + noff));
	prev = *((uintptr_t *)(t + poff));

	/* t is not on a list: either it was never added, or it was already removed
	 * -- this function NULLs both links on its way out. Removing it a second
	 * time used to fall into the else branch below and WRITE through the NULL
	 * prev pointer. Observed 2026-09-04 as `Exception #36: Data Abort (EL0)
	 * ... far=0x10` (0x10 == the `next` offset in FILE) while quake3e loaded a
	 * map: stdio's open-FILE list is the only lib_list user in libphoenix
	 * (stdio/file.c), and the same FILE was unlinked twice.
	 *
	 * A wild write through NULL is a far worse outcome than tolerating the
	 * second removal, and every caller's intent -- "t must not be on the list"
	 * -- already holds. Clear the head too if it still names t, since a head
	 * pointing at an unlinked node is corruption either way. */
	if (next == (uintptr_t)NULL || prev == (uintptr_t)NULL) {
		if (*list == t) {
			*list = NULL;
		}
		return;
	}

	if (next == (uintptr_t)t && prev == (uintptr_t)t) {
		*list = NULL;
	}
	else {
		*((uintptr_t *)((void *)(*((uintptr_t *)(t + poff))) + noff)) = *((uintptr_t *)(t + noff));
		*((uintptr_t *)((void *)(*((uintptr_t *)(t + noff))) + poff)) = *((uintptr_t *)(t + poff));
		if (t == *list)
			*list = (void *)*((uintptr_t *)(t + noff));
	}
	*((uintptr_t *)(t + noff)) = (uintptr_t)NULL;
	*((uintptr_t *)(t + poff)) = (uintptr_t)NULL;
}
