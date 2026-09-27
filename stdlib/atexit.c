/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * atexit.c
 *
 * Copyright 2019, 2022, 2026 Phoenix Systems
 * Author: Kamil Amanowicz, Dawid Szpejna, Adam Greloch, Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/threads.h>
#include <sys/debug.h>


#define ATEXIT_MAX 32


typedef void (*destructor_t)(void);


struct atexit_node {
	destructor_t destructors[ATEXIT_MAX];
	void *args[ATEXIT_MAX];
	void *handles[ATEXIT_MAX];
	uint32_t fntype; /* i-th bit set means i-th function accepts an arg */
	struct atexit_node *prev;
};


_Static_assert(ATEXIT_MAX <= sizeof(((struct atexit_node *)0)->fntype) * 8, "ATEXIT_MAX must fit into fntype bitmask");


/*
 * List of functions to be called on termination. Nodes other than the (optional)
 * statically allocated one are allocated on demand.
 */
struct atexit_list {
	/* Top of the node chain - new functions are always registered into this node */
	struct atexit_node *topNode;
	/* Node that must not be freed, or NULL if the list has no statically allocated node */
	struct atexit_node *staticNode;
	/* Number of slots used in topNode */
	unsigned int usedCnt;
	/*
	 * Counts the changes that invalidate a walk in progress: every registration and
	 * every node release. It can safely wrap around.
	 */
	unsigned int modCnt;
};


/* The exit list gets a statically allocated node, to provide at least 32 function slots */
static struct atexit_node atexit_staticNode;


static struct {
	struct atexit_list exitList;
	/* POSIX-DEVIATION: Nodes of the quick exit list are allocated only if at_quick_exit() is used, to optimize memory use. */
	struct atexit_list quickExitList;
	handle_t lock;
} atexit_common = {
	.exitList = {
		.topNode = &atexit_staticNode,
		.staticNode = &atexit_staticNode,
	},
};


/* Diagnostic: is the atexit bookkeeping still self-consistent?
 *
 * Returns 0 if it is, or a bitmask naming what is wrong. Exists because the
 * atexit state has been observed corrupted on a Pi4 -- the slot index far above
 * ATEXIT_MAX, the top-node pointer overwritten with the kernel's thread-stack
 * fill byte -- and the crash only surfaces much later, at exit, in
 * __cxa_finalize. A caller that polls this can say WHEN the damage appeared
 * instead of only that it did. Deliberately read-only and lock-free so it is
 * safe to call from anywhere.
 *
 * Only the exit list is checked: an empty quick exit list (topNode == NULL) is
 * the normal state until at_quick_exit() is first used. */
int _atexit_check(void)
{
	const struct atexit_list *list = &atexit_common.exitList;
	int bad = 0;

	if (list->usedCnt > ATEXIT_MAX) {
		bad |= 1;
	}
	if (list->topNode == NULL) {
		bad |= 2;
	}
	else if (((uintptr_t)list->topNode & 0x7UL) != 0UL) {
		/* every node is calloc'd or static, so at least 8-byte aligned */
		bad |= 4;
	}
	if (list->staticNode != &atexit_staticNode) {
		bad |= 8;
	}

	return bad;
}


/* Initialise exit lists before main */
void _atexit_init(void)
{
	mutexCreate(&atexit_common.lock);

	/* The exit list is initialised statically, so a NULL here means the initialised
	 * .data word did not survive into this process image -- observed on hardware,
	 * where the top-node pointer read back NULL and the process died pre-main (2070
	 * identical Data Aborts with far=0x0 in a psh applet, when this function still
	 * memset() the node through that pointer). Recover rather than fault later:
	 * with a NULL top node and a non-NULL static node, the release loop in
	 * _atexit_finalize() would walk off the end of the chain.
	 *
	 * Reaching here is NOT routine defensiveness. It is a memory-integrity failure
	 * that would hit ANY statically initialised pointer -- this one merely happens
	 * to be used early enough to matter -- so report it rather than hide the only
	 * evidence that it still happens. debug() is a raw syscall with no stdio,
	 * malloc or locking, which is why it is safe here: _file_init() has not run. */
	if ((atexit_common.exitList.topNode == NULL) || (atexit_common.exitList.staticNode == NULL)) {
		debug("libc: atexit list read back NULL -- .data did not survive into this process image; recovered\n");
		atexit_common.exitList.topNode = &atexit_staticNode;
		atexit_common.exitList.staticNode = &atexit_staticNode;
		atexit_common.exitList.usedCnt = 0;
	}
}


/* Generic function to register destructors */
static int _atexit_register(struct atexit_list *list, int isarg, void (*fn)(void), void *arg, void *handle)
{
	struct atexit_node *node;

	mutexLock(atexit_common.lock);
	node = list->topNode;

	/* Allocate new node if there are no free slots left.
	 *
	 * >= rather than ==: with an equality test, a usedCnt that ever exceeded
	 * ATEXIT_MAX (however it got there) never matched again, so every subsequent
	 * registration wrote the node's arrays PAST their end -- straight into
	 * whatever follows. Observed on a Pi4 with the index at 180 against
	 * ATEXIT_MAX == 32, having overwritten the list bookkeeping itself. */
	if ((node == NULL) || (list->usedCnt >= ATEXIT_MAX)) {
		node = (struct atexit_node *)calloc(1, sizeof(struct atexit_node));
		if (node == NULL) {
			mutexUnlock(atexit_common.lock);
			return -1;
		}
		node->prev = list->topNode;

		list->topNode = node;
		list->usedCnt = 0;
	}

	if (isarg != 0) {
		node->fntype |= (1u << list->usedCnt);
		node->args[list->usedCnt] = arg;
	}
	else {
		node->fntype &= ~(1u << list->usedCnt);
		node->args[list->usedCnt] = NULL;
	}

	node->destructors[list->usedCnt] = fn;
	node->handles[list->usedCnt] = handle;

	list->usedCnt++;
	list->modCnt++;

	mutexUnlock(atexit_common.lock);
	return 0;
}


/* Generic function to call destructors registered for given object, or all in case of NULL */
static void _atexit_finalize(struct atexit_list *list, void *handle)
{
	struct atexit_node *node, *prev;
	destructor_t destructor;
	unsigned int i, modCnt;
	bool restart;

	mutexLock(atexit_common.lock);

	if (list->topNode == NULL) {
		mutexUnlock(atexit_common.lock);
		return;
	}

	/*
	 * Walk the chain from the top node down. The walk keeps its own cursor, so that
	 * list->topNode always stays the top of the chain. A destructor registering new
	 * functions must not detach the nodes the walk has already passed.
	 *
	 * The lock is dropped around every destructor call, so the list may change while
	 * one runs. The walk saves modCnt before dropping the lock and restarts
	 * from the top when it differs afterwards, which covers two cases:
	 *  - functions registered by the destructor, which have to be called before the
	 *    remaining, older ones,
	 *  - nodes released by a concurrent or recursive finalize, which would otherwise
	 *    leave the local cursor pointing into freed memory.
	 *
	 * Already called destructors are NULL, so a restart does not call anything twice.
	 */
	do {
		restart = false;
		node = list->topNode;
		i = list->usedCnt;
		modCnt = list->modCnt;

		while (node != NULL) {
			while (i > 0) {
				i--;
				destructor = node->destructors[i];

				/* skip already called destructors and destructors not from the current handle */
				if ((destructor == NULL) || ((handle != NULL) && (handle != node->handles[i]))) {
					continue;
				}

				/* mark destructor as called */
				node->destructors[i] = NULL;

				if (((node->fntype >> i) & 1) == 1) {
					void *arg = node->args[i];
					mutexUnlock(atexit_common.lock);
					((void (*)(void *))destructor)(arg);
				}
				else {
					mutexUnlock(atexit_common.lock);
					destructor();
				}
				mutexLock(atexit_common.lock);

				if (list->modCnt != modCnt) {
					restart = true;
					break;
				}
			}

			if (restart) {
				break;
			}

			node = node->prev;

			/* nodes other than the top one are always full */
			i = ATEXIT_MAX;
		}
	} while (restart);

	/*
	 * Release the nodes that have been emptied completely, starting from the top
	 * and stopping at the statically allocated node, or at the end of the chain
	 * when the list has none (for quick exit list staticNode == NULL).
	 */
	node = list->topNode;
	while (node != list->staticNode) {
		for (i = 0; i < ATEXIT_MAX; i++) {
			if (node->destructors[i] != NULL) {
				break;
			}
		}

		if (i != ATEXIT_MAX) {
			break;
		}

		prev = node->prev;
		free(node);
		node = prev;

		/*
		 * A concurrent or recursive walk may hold a cursor into the released
		 * node, make it restart from the top instead of following it.
		 */
		list->modCnt++;
	}

	list->topNode = node;

	/*
	 * Restore usedCnt to one past the highest live slot. Slots below it that a
	 * handle specific finalize emptied stay unused on purpose. A new registration
	 * has to land above every surviving one, or it would be called out of reverse
	 * registration order.
	 */
	i = ATEXIT_MAX;
	if (node == NULL) {
		i = 0;
	}
	else {
		while ((i > 0) && (node->destructors[i - 1] == NULL)) {
			i--;
		}
	}
	list->usedCnt = i;

	mutexUnlock(atexit_common.lock);
}


/* Call destructors registered for given object, or all in case of NULL */
/* Conforming: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor */
void __cxa_finalize(void *handle)
{
	/* Refuse to walk a list that is visibly broken. Every field checked is
	 * dereferenced or used as an index by the walk, so walking a corrupt list
	 * means jumping through a wild pointer -- which is what happened on a Pi4,
	 * where the top-node pointer had been overwritten and the slot index read
	 * 180 against ATEXIT_MAX of 32. A process that cannot run its handlers
	 * should still exit, and say why, rather than take a fault deep inside the
	 * C runtime. */
	int bad = _atexit_check();
	if (bad != 0) {
		fprintf(stderr, "atexit: handler list corrupt (mask=%d, usedCnt=%u, topNode=%p), skipping handlers\n",
				bad, atexit_common.exitList.usedCnt, (void *)atexit_common.exitList.topNode);
		return;
	}

	_atexit_finalize(&atexit_common.exitList, handle);
}


/* Call functions registered with at_quick_exit() */
void _quick_exit_finalize(void)
{
	_atexit_finalize(&atexit_common.quickExitList, NULL);
}


int atexit(void (*func)(void))
{
	return _atexit_register(&atexit_common.exitList, 0, func, NULL, NULL);
}


int __cxa_atexit(void (*func)(void *), void *arg, void *handle)
{
	return _atexit_register(&atexit_common.exitList, 1, (void (*)(void))func, arg, handle);
}


int __cxa_at_quick_exit(void (*func)(void *), void *arg, void *handle)
{
	return _atexit_register(&atexit_common.quickExitList, 1, (void (*)(void))func, arg, handle);
}


int at_quick_exit(void (*func)(void))
{
	return _atexit_register(&atexit_common.quickExitList, 0, func, NULL, NULL);
}
