/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * atexit.c
 *
 * Copyright 2019 2022 Phoenix Systems
 * Author: Kamil Amanowicz, Dawid Szpejna
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/threads.h>


/* The atexit_node structure uses uint32_t for flags that
 indicate whether to call a function with arguments or not, therefore
 why ATEXIT_MAX must be 32 as type "fntype" */
#define ATEXIT_MAX 32


typedef void (*destructor_t)(void);


struct atexit_node {
	destructor_t destructors[ATEXIT_MAX];
	void *args[ATEXIT_MAX];
	void *handles[ATEXIT_MAX];
	uint32_t fntype;
	struct atexit_node *prev;
};


/* The first node is statically allocated to provide at least 32 function slots.
 *
 * It is a NAMED object rather than the address of a file-scope compound literal
 * (`&((struct atexit_node){})`, which this used to be). That construct is legal
 * C -- the literal has static storage duration -- but it leaves the only pointer
 * to the node in an initialised .data word, and on this target that word has
 * been observed reading back as NULL: _atexit_init() then did
 * memset(NULL, 0, sizeof(struct atexit_node)) and the process died on the first
 * `dc zva`, before main(). Caught on hardware as 2070 identical
 * Data Abort (EL0) with far=0x0 in a psh applet (see _atexit_init below).
 * A named object is the same storage with a name the recovery path can use. */
static struct atexit_node atexit_firstNode;

static struct {
	handle_t lock;
	struct atexit_node *head;
	struct atexit_node *newestNode;
	unsigned int idx;
} atexit_common = { .head = &atexit_firstNode };


/* Diagnostic: is the atexit bookkeeping still self-consistent?
 *
 * Returns 0 if it is, or a bitmask naming what is wrong. Exists because
 * atexit_common has been observed corrupted on a Pi4 -- idx far above
 * ATEXIT_MAX, head overwritten with the kernel's thread-stack fill byte -- and
 * the crash only surfaces much later, at exit, in __cxa_finalize. A caller that
 * polls this can say WHEN the damage appeared instead of only that it did.
 * Deliberately read-only and lock-free so it is safe to call from anywhere. */
int _atexit_check(void)
{
	int bad = 0;

	if (atexit_common.idx > ATEXIT_MAX) {
		bad |= 1;
	}
	if (atexit_common.head == NULL) {
		bad |= 2;
	}
	else if (((uintptr_t)atexit_common.head & 0x7UL) != 0UL) {
		/* every node is calloc'd or static, so at least 8-byte aligned */
		bad |= 4;
	}
	if ((atexit_common.newestNode != NULL) && (((uintptr_t)atexit_common.newestNode & 0x7UL) != 0UL)) {
		bad |= 8;
	}

	return bad;
}


/* Initialise atexit_common structure before main */
void _atexit_init(void)
{
	mutexCreate(&atexit_common.lock);

	/* head is initialised statically, so a NULL here means the initialised word
	 * did not survive into this process image -- observed on hardware. Dereferen-
	 * cing it kills the process before main() with no diagnostic at all, which is
	 * a far worse failure than re-pointing it at the node that certainly exists.
	 * Recover rather than fault: the storage is a named static object. */
	if (atexit_common.head == NULL) {
		atexit_common.head = &atexit_firstNode;
	}

	memset(atexit_common.head, 0, sizeof(struct atexit_node));
	atexit_common.idx = 0;
	atexit_common.newestNode = atexit_common.head;
}


/* Generic function to register destructors */
static int _atexit_register(int isarg, void (*fn)(void), void *arg, void *handle)
{
	struct atexit_node *node;

	mutexLock(atexit_common.lock);
	node = atexit_common.head;

	/* Allocate new node if there are no free slots left.
	 *
	 * >= rather than ==: with an equality test, an idx that ever exceeded
	 * ATEXIT_MAX (however it got there) never matched again, so every
	 * subsequent registration wrote node->destructors[idx], node->args[idx] and
	 * node->handles[idx] PAST the end of the node -- straight into whatever
	 * follows it. Observed on a Pi4 with idx == 180 against ATEXIT_MAX == 32,
	 * having overwritten the head pointer and idx of atexit_common itself. */
	if (atexit_common.idx >= ATEXIT_MAX) {
		node = (struct atexit_node *)calloc(1, sizeof(struct atexit_node));
		if (node == NULL) {
			mutexUnlock(atexit_common.lock);
			return -1;
		}
		node->prev = atexit_common.head;

		atexit_common.head = node;
		atexit_common.idx = 0;
		atexit_common.newestNode = node;
	}

	if (isarg != 0) {
		node->fntype |= (1u << atexit_common.idx);
		node->args[atexit_common.idx] = arg;
	}
	node->destructors[atexit_common.idx] = fn;
	node->handles[atexit_common.idx] = handle;

	atexit_common.idx++;

	mutexUnlock(atexit_common.lock);
	return 0;
}


/* Call destructors registered for given object, or all in case of NULL */
/* Conforming: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor */
void __cxa_finalize(void *handle)
{
	/* No handlers registered. */
	if (atexit_common.idx == 0) {
		return;
	}

	/* Refuse to walk a list that is visibly broken. Every field here is
	 * dereferenced or used as an index below, so walking a corrupt list means
	 * jumping through a wild pointer -- which is what happened on a Pi4, where
	 * head had been overwritten and idx read 180 against ATEXIT_MAX of 32. A
	 * process that cannot run its handlers should still exit, and say why,
	 * rather than take a fault deep inside the C runtime. */
	{
		int bad = _atexit_check();
		if (bad != 0) {
			fprintf(stderr, "atexit: handler list corrupt (mask=%d, idx=%u, head=%p), skipping handlers\n",
				bad, atexit_common.idx, (void *)atexit_common.head);
			return;
		}
	}

	mutexLock(atexit_common.lock);
	/* Iteration has to be over the atexit_common.head and newest node must be restored at the end
	 * as the atexit functions may register new atexit functions. */
	while (atexit_common.head != NULL) {
		/* Clamp before use. idx is decremented below, so a value above
		 * ATEXIT_MAX would read past the arrays, and a value of 0 would wrap to
		 * UINT_MAX -- and 0 is reachable here: the destructors called below run
		 * with the lock dropped and may register new handlers, which allocates
		 * a fresh node and resets idx to 0 (see the note above). */
		if (atexit_common.idx > ATEXIT_MAX) {
			atexit_common.idx = ATEXIT_MAX;
		}

		if (atexit_common.idx == 0) {
			/* Nothing left in this node: step to the previous one. */
			atexit_common.head = atexit_common.head->prev;
			atexit_common.idx = ATEXIT_MAX;
			continue;
		}

		atexit_common.idx--;
		destructor_t destructor = atexit_common.head->destructors[atexit_common.idx];
		/* Do not call already called destructors and destructors not from current handle. */
		if ((destructor != NULL) && ((handle == atexit_common.head->handles[atexit_common.idx]) || (handle == NULL))) {
			/* Mark destructor as called. */
			atexit_common.head->destructors[atexit_common.idx] = NULL;

			if (((atexit_common.head->fntype >> atexit_common.idx) & 1) == 1) {
				void *arg = atexit_common.head->args[atexit_common.idx];
				mutexUnlock(atexit_common.lock);
				((void (*)(void *))destructor)(arg);
			}
			else {
				mutexUnlock(atexit_common.lock);
				destructor();
			}
			mutexLock(atexit_common.lock);
		}
	}

	atexit_common.head = atexit_common.newestNode;

	/* All nodes are fully emptied from destructors, free memory. */
	if (handle == NULL) {
		/* Ensure the first node that is statically allocated is not freed */
		while (atexit_common.head->prev != NULL) {
			struct atexit_node *last = atexit_common.head;
			atexit_common.head = atexit_common.head->prev;
			free(last);
		}
		atexit_common.newestNode = atexit_common.head;
		atexit_common.idx = 0;
	}
	else {
		/* Restore idx. */
		unsigned int i = ATEXIT_MAX;
		while ((i > 0) && (atexit_common.head->destructors[i - 1] == NULL)) {
			i--;
		}
		atexit_common.idx = i;
	}

	mutexUnlock(atexit_common.lock);
}


int atexit(void (*func)(void))
{
	return _atexit_register(0, func, NULL, NULL);
}


/* Register a function to run at process termination with given arguments */
int __cxa_atexit(void (*func)(void *), void *arg, void *handle)
{
	return _atexit_register(1, (void (*)(void))func, arg, handle);
}
