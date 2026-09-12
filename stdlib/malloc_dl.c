/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * stdlib/malloc (Doug Lea)
 *
 * Copyright 2017, 2020 Phoenix Systems
 * Author: Jakub Sejdak, Jan Sikorski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <sys/list.h>
#include <sys/minmax.h>
#include <sys/rb.h>
#include <sys/threads.h>
#include <sys/minmax.h>
#include <sys/mman.h>
#include <sys/debug.h>

#include <arch.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#define CEIL(value, size)          ((((value) + (size) - 1) / (size)) * (size))
#define FLOOR(value, size)         (((value) / (size)) * (size))

#define CHUNK_PUSED                1
#define CHUNK_CUSED                2

#define CHUNK_OVERHEAD             CEIL(__builtin_offsetof(chunk_t, next), 8)
#define CHUNK_MIN_SIZE             CEIL(__builtin_offsetof(chunk_t, node) + sizeof(size_t), 8)
#define CHUNK_SMALLBIN_MAX_SIZE    (256 - CHUNK_OVERHEAD)


typedef struct {
	size_t size;
	size_t freesz;
	uint8_t space[];
} heap_t;


typedef struct _chunk_t {
/*	size_t prevSize; This is a foot field of the previous chunk! */
	size_t size;
	heap_t *heap;

	/* Following fields are used only when the chunk is free */
	struct _chunk_t *next;
	struct _chunk_t *prev;
	rbnode_t node; /* Only used for big chunks */
} chunk_t;


struct {
	uint32_t sbinmap;
	uint32_t lbinmap;
	chunk_t *sbins[32];
	rbtree_t lbins[32];

	size_t allocsz;
	size_t freesz;

	/* The last few heaps handed back to the kernel, newest at `relIdx - 1`.
	 *
	 * Diagnostic, not bookkeeping: a free-bin entry must never point into a heap
	 * we have released, so if a rejected link falls inside one of these the entry
	 * is a STALE POINTER INTO RECYCLED MEMORY -- proven, not inferred. That is
	 * the reading the field data points at: one report's chunk decoded as a fresh
	 * heap header (hbase?=1) while another at the same stride decoded as live
	 * 16-bit GPU index data (0,1,2,3,...), which is what recycled memory looks
	 * like when you read it as a chunk. mmap reuses addresses, so "looks sane"
	 * and "looks like garbage" are the same defect seen at different moments. */
	struct {
		uintptr_t base;
		size_t size;
	} released[8];
	unsigned int relIdx;

	/* Base addresses of heaps currently mmap'd, newest wrapping at 16.
	 *
	 * Settles the one reading `hbase?=` cannot: that test is a value test, so a
	 * LIVE allocated block whose payload happens to begin with a size-like word
	 * and a self-pointer -- a perfectly ordinary embedded list head -- would also
	 * satisfy it. If the rejected address instead matches a base we actually
	 * mmap'd, it IS a heap base and no coincidence is involved. Paired with
	 * `freed?=` (which reads 0 on every event so far, i.e. the heap was never
	 * released) this says whether a LIVE heap's base is sitting in a free bin. */
	/* 256 slots, not 16: SuperTuxKart gives almost every large allocation its own
	 * mmap'd heap, so a 16-entry ring evicts a base long before a bin entry
	 * pointing at it is rejected -- which makes `lheap?=0` mean "evicted" rather
	 * than "never a heap" and renders the field useless. Measured: 3 events all
	 * read lheap?=0 while hbase?=1, with no way to tell the readings apart.
	 * 2 KiB of BSS is a fair price for a verdict that discriminates. */
	uintptr_t live[256];
	unsigned int liveIdx;
	unsigned int liveOverflow;

	/* Address window of every heap this allocator has ever mmap'd. Only ever
	 * widened, so a munmap'd heap leaves its range inside the window -- that is
	 * fine, this is a plausibility filter, not exact membership, and it costs two
	 * comparisons on a path that already holds the mutex.
	 *
	 * It exists because malloc_chunkValid() could not tell a real header from a
	 * fabricated one. On 2026-09-09 a child in the AF_UNIX liveness test called
	 * free() on a garbage pointer (0x25e0); the allocator read chunk->heap =
	 * 0x2000 out of low memory, and that "heap" passed every existing check --
	 * 0x2000 IS page-aligned, its "size" 0x1000 IS a page multiple, and 0x25e0 IS
	 * inside 0x2000..0x3000. So the block looked valid, its in-use bit was clear,
	 * and the allocator reported a DOUBLE FREE and exited EX_SOFTWARE.
	 *
	 * ⚠ CORRECTION (measured 2026-09-09, later): the first version of this comment
	 * argued the pointer must be garbage because "no mmap returns page 2". That is
	 * FALSE on this port -- a real STK process reports heapLo=0x2000, so the very
	 * first heap does live at page 2. The window is still worth having as a cheap
	 * plausibility filter for a WILD pointer, but it does not prove anything about
	 * that earlier report, and the stray-free conclusion drawn from it does not
	 * stand on this evidence. */
	uintptr_t heapLo;
	uintptr_t heapHi;

	handle_t mutex;
} malloc_common;


static inline size_t malloc_chunkSize(chunk_t *chunk)
{
	return chunk->size & ~(CHUNK_CUSED | CHUNK_PUSED);
}


static int malloc_cmp(rbnode_t *n1, rbnode_t *n2)
{
	chunk_t *e1 = lib_treeof(chunk_t, node, n1);
	chunk_t *e2 = lib_treeof(chunk_t, node, n2);

	size_t e1sz = malloc_chunkSize(e1);
	size_t e2sz = malloc_chunkSize(e2);

	if (e1sz == e2sz)
		return 0;

	return (e1sz > e2sz) ? 1 : -1;
}


static int malloc_find(rbnode_t *n1, rbnode_t *n2)
{
	chunk_t *e1 = lib_treeof(chunk_t, node, n1);
	chunk_t *e1_left = lib_treeof(chunk_t, node, n1->left);
	chunk_t *e2 = lib_treeof(chunk_t, node, n2);

	size_t e1sz = malloc_chunkSize(e1);
	size_t e2sz = malloc_chunkSize(e2);

	if (e1sz == e2sz)
		return 0;

	if (e1sz > e2sz) {
		if (e1_left != NULL && malloc_chunkSize(e1_left) >= e2sz)
			return 1;

		return 0;
	}

	return -1;
}


static inline unsigned int malloc_getsidx(size_t size)
{
	return (size + 7) >> 3;
}


static inline unsigned int malloc_getlidx(size_t size)
{
	unsigned int x = (size >> 8), k;

	if (x == 0)
		return 0;

	if (x > 0xffff)
		return 31;

	k = sizeof(x) * __CHAR_BIT__ - 1 - __builtin_clz(x);
	return (k << 1) + (size >> (k + (8 - 1)) & 1);
}


static inline int malloc_chunkIsFirst(chunk_t *chunk)
{
	return (chunk->heap->space == (uint8_t*) chunk);
}


static long int malloc_chunkIsLast(chunk_t *chunk)
{
	return ((uintptr_t) chunk + malloc_chunkSize(chunk) + CHUNK_MIN_SIZE > (uintptr_t) chunk->heap + chunk->heap->size);
}


/* Same test, but against a heap the CALLER already trusts instead of chunk->heap.
 *
 * Why it exists: malloc_chunkValid() checks that a chunk's ADDRESS lies inside the
 * given heap; it never checks that the chunk's own ->heap field agrees. So a chunk
 * can pass validation while carrying a wrong ->heap, and malloc_chunkIsLast() then
 * computes the heap end from that wrong pointer. If the wrong heap is larger or
 * based lower, the end lands past the real one, the "is this the last chunk?" test
 * says no, and the walk steps to chunk+size -- which is exactly the real heap's end,
 * a PAGE-ALIGNED address just outside it.
 *
 * That is the fingerprint in every corrupt-neighbour report on record: 11 of 11
 * siblings page-aligned, while the chunks they came from were only 16-byte aligned
 * (0 of 11). Random bytes over a size field land page-aligned with p ~ 1/256, so
 * 11 for 11 is ~1e-27 -- the walk was reaching a heap edge by construction.
 *
 * In _malloc_chunkJoin() the reference heap is trustworthy: it is the caller's
 * chunk->heap, and every chunk promoted into `it` was validated as lying inside it.
 * Using it here removes the wrong-heap arithmetic from the loop entirely. */
static long int malloc_chunkIsLastIn(chunk_t *chunk, const heap_t *heap)
{
	return ((uintptr_t)chunk + malloc_chunkSize(chunk) + CHUNK_MIN_SIZE
			> (uintptr_t)heap + heap->size);
}


static inline chunk_t *malloc_chunkPrev(chunk_t *chunk)
{
	/* size_t, not unsigned: the footer is a size_t, so a 32-bit type silently
	 * truncates any value above 4 GiB -- and, worse, mis-truncates a CORRUPTED
	 * footer into a plausible-looking small offset, which is exactly the case the
	 * validation below exists to catch. */
	size_t prevSize = ((chunk->size & CHUNK_PUSED) != 0) ? 0u : *((size_t *)chunk - 1);
	if (prevSize == 0)
		return NULL;

	return (chunk_t *) ((uintptr_t) chunk - prevSize);
}


static inline chunk_t *malloc_chunkNext(chunk_t *chunk)
{
	if (malloc_chunkIsLast(chunk))
		return NULL;

	return (chunk_t *) ((uintptr_t) chunk + malloc_chunkSize(chunk));
}


static inline void malloc_chunkSetFooter(chunk_t *chunk)
{
	size_t size = malloc_chunkSize(chunk);
	*((size_t *)((uintptr_t) chunk + size) - 1) = size;
}


/* Print "<label>0x<hex>\n" without allocating. printf() here would call back
 * into malloc while its lock is held; debug() is a raw write. */
static void malloc_debugHex(const char *label, uintptr_t v)
{
	static const char hexd[] = "0123456789abcdef";
	char buf[80];
	size_t n = 0;
	int i;

	while ((label[n] != '\0') && (n < 48)) {
		buf[n] = label[n];
		n++;
	}
	buf[n++] = '0';
	buf[n++] = 'x';
	for (i = (int)(sizeof(uintptr_t) * 2) - 1; i >= 0; i--) {
		buf[n++] = hexd[(v >> (unsigned)(i * 4)) & 0xfu];
	}
	buf[n++] = '\n';
	buf[n] = '\0';
	debug(buf);
}


/* Is this chunk header self-consistent with the heap it claims to belong to?
 *
 * A heap overflow in the caller lands on the NEXT chunk's header, and free()
 * then derives a write address from the corrupted size (malloc_chunkSetFooter
 * writes at chunk + size - sizeof(size_t)). That turns one overflow into a
 * second, unbounded write, which is why the eventual fault usually appears in
 * an unrelated allocation -- observed as faults in malloc_cmp/lib_rbInsert and
 * as a jump to a garbage address. Checking the header first localises the
 * damage to the block that was actually smashed. */
static int malloc_wasReleased(const chunk_t *chunk);


static int malloc_chunkValid(chunk_t *chunk, const heap_t *heap)
{
	uintptr_t base = (uintptr_t)heap;
	size_t size;

	/* Reject a chunk pointer that cannot be a user address BEFORE anything here
	 * dereferences it. This function's job is to decide whether `chunk` is
	 * trustworthy, and it ends by reading chunk->size -- so a wild pointer used
	 * to fault inside the validator itself, which is the one place that must not
	 * happen. Observed on hardware: a Data Abort at malloc_chunkSize()
	 * (malloc_dl.c:136) on chunk = 0x800000000ccdc000.
	 *
	 * The range test below cannot catch that on its own: it compares against
	 * base + heap->size, so it only rejects the chunk if the heap's own bounds
	 * are sound, and the window test above it is skipped entirely while
	 * heapHi is still 0 (early in a process).
	 *
	 * AArch64 user addresses are canonical -- bits 63..47 zero -- and every
	 * corrupt pointer seen in this class violates that outright (0x80000001...,
	 * 0x80000000...). NULL and a misaligned pointer are equally impossible for a
	 * chunk, so fold those in too. */
	if ((chunk == NULL) || ((((uintptr_t)chunk) >> 47) != 0)
			|| ((((uintptr_t)chunk) & 7u) != 0)) {
		return 0;
	}

	/* ...and reject a pointer into a heap we have already released. The check
	 * above only catches a pointer that cannot be an address at all; a stale
	 * pointer into a munmap'd heap is perfectly canonical, and the range test
	 * below cannot reject it either, because it is measured against that same
	 * stale heap's header -- which may still be readable even after some of the
	 * heap's later pages have gone. Observed on hardware: a Data Abort at
	 * malloc_chunkSize() on chunk = 0x0cdfa000, page-aligned, canonical, and
	 * inside the range the stale header claimed.
	 *
	 * This is a "known bad" test, not a "known good" one: it can only miss, never
	 * false-reject, so it is safe to act on. (The live[] ring is NOT usable for
	 * the opposite test -- it is a 256-entry diagnostic with a documented
	 * overflow counter, so once it wraps a "not live" answer would reject VALID
	 * heaps and stop coalescing altogether.) */
	if (malloc_wasReleased(chunk) != 0) {
		return 0;
	}

	if ((heap == NULL) || ((base & (uintptr_t)(_PAGE_SIZE - 1)) != 0)) {
		return 0; /* heaps come from mmap(), so they are page-aligned */
	}
	/* ...and they lie inside the window of heaps we have actually mmap'd. This is
	 * what rejects a header fabricated out of unrelated memory, which the
	 * alignment and range tests below cannot: see the note on heapLo/heapHi. */
	if (malloc_common.heapHi != 0u) {
		if ((base < malloc_common.heapLo) || (base >= malloc_common.heapHi)) {
			return 0;
		}
	}
	if ((heap->size < sizeof(heap_t)) || ((heap->size & (_PAGE_SIZE - 1)) != 0)) {
		return 0;
	}
	if (((uintptr_t)chunk < base + sizeof(heap_t)) || ((uintptr_t)chunk >= base + heap->size)) {
		return 0;
	}

	size = malloc_chunkSize(chunk);
	if ((size < CHUNK_MIN_SIZE) || ((size & 7u) != 0)) {
		return 0;
	}
	if (((uintptr_t)chunk + size) > (base + heap->size)) {
		return 0;
	}

	return 1;
}


static void malloc_chunkInit(chunk_t *chunk, heap_t *heap, size_t size)
{
	chunk->size = size;
	chunk->heap = heap;
	chunk->next = NULL;
	chunk->prev = NULL;

	malloc_chunkSetFooter(chunk);
}


static void _malloc_chunkAdd(chunk_t *chunk)
{
	unsigned int idx;
	size_t chunksz = malloc_chunkSize(chunk);
	chunk_t *exist;

	if (chunksz <= CHUNK_SMALLBIN_MAX_SIZE) {
		idx = malloc_getsidx(chunksz);
		LIST_ADD(&malloc_common.sbins[idx], chunk);
		malloc_common.sbinmap |= (1 << idx);
		return;
	}

	idx = malloc_getlidx(chunksz);
	exist = lib_treeof(chunk_t, node, lib_rbInsert(&malloc_common.lbins[idx], &chunk->node));
	if (exist != NULL)
		/* Mark chunk as not actually being in the tree */
		chunk->node.parent = &chunk->node;
	LIST_ADD(&exist, chunk);

	malloc_common.lbinmap |= (1 << idx);
}


/* Is this pointer inside a heap this allocator has actually mmap'd, and aligned
 * like a chunk? A pure value test, so it is safe to run on a pointer we are not
 * yet willing to dereference -- which is the whole point: malloc_chunkValid()
 * reads chunk->heap, and a pointer synthesised by a stray tree walk may not be
 * readable at all. */
static int malloc_chunkInWindow(const chunk_t *chunk)
{
	uintptr_t p = (uintptr_t)chunk;

	if ((p & 7u) != 0u) {
		return 0;
	}
	if (malloc_common.heapHi == 0u) {
		return 0;
	}
	/* RANGE ONLY. Requiring sizeof(chunk_t) to fit rejects legitimate chunks near
	 * a heap's end, because sizeof(chunk_t) includes the rbnode that only LARGE
	 * chunks ever use while the free-list fields live in the payload. That is the
	 * same trap malloc_linkPlausible() documents above, and adding
	 * `+ sizeof(chunk_t)` here reintroduced it: measured on hardware, the
	 * small-bin head check then fired ~390 times per SuperTuxKart run in EVERY
	 * run, including runs that had been clean, dropping healthy bins and leaking
	 * their chunks. A hardening check that rejects valid input is worse than no
	 * check -- it converts a rare fault into constant quiet damage. */
	if ((p < malloc_common.heapLo) || (p >= malloc_common.heapHi)) {
		return 0;
	}
	return 1;
}


/* Is this address the base of a heap we actually mmap'd (and have not released)?
 * Pure value test over a small ring, safe on a pointer we will not dereference.
 * A hit removes the last alternative to the heap-base reading: that `hbase?=`
 * matched a live block whose payload merely looks like a heap header. */
static int malloc_isLiveHeapBase(const chunk_t *chunk)
{
	uintptr_t p = (uintptr_t)chunk;
	unsigned int i;

	for (i = 0; i < 256u; i++) {
		if ((malloc_common.live[i] != 0u) && (malloc_common.live[i] == p)) {
			return 1;
		}
	}
	return 0;
}


/* Did this address belong to one of the last heaps we released? A pure value
 * test over a tiny ring, so it is safe on a pointer we will not dereference.
 * A hit PROVES the free-bin entry is a stale pointer into recycled memory. */
static int malloc_wasReleased(const chunk_t *chunk)
{
	uintptr_t p = (uintptr_t)chunk;
	unsigned int i;

	for (i = 0; i < 8u; i++) {
		if (malloc_common.released[i].size == 0u) {
			continue;
		}
		if ((p >= malloc_common.released[i].base) &&
				(p < (malloc_common.released[i].base + malloc_common.released[i].size))) {
			return 1;
		}
	}
	return 0;
}


/* Is this free-bin link plausible WITHOUT dereferencing it?
 *
 * Deliberately a pure value test. The link may be wild, so reading link->heap to
 * run malloc_chunkValid() on it would itself fault -- which is the very thing this
 * exists to avoid. The heap window (see heapLo/heapHi) makes the test possible
 * without a dereference: every chunk lives inside a heap we mmap'd.
 *
 * Measured need: SuperTuxKart faulted at lib_listRemove (sys/list.c:71,
 * `t->next->prev = t->prev`) on hardware -- one of four distinct STK crash sites,
 * two of which are inside this allocator rather than in STK's own code. list.c
 * already rejects NULL links; a garbage non-NULL link goes straight through. This
 * is the same defect class fixed in the kernel's scheduler the same day, and the
 * same remedy: check the pointer values before the unlink dereferences them.
 */
static int malloc_linkPlausible(const chunk_t *chunk, const chunk_t *link)
{
	if ((link == NULL) || (link == chunk)) {
		return 1; /* not on a list, or a single-element list */
	}
	if (((uintptr_t)link & 7u) != 0u) {
		return 0;
	}
	if (malloc_common.heapHi == 0u) {
		return 1; /* no heap seen yet: nothing to compare against */
	}
	/* Range only -- do NOT require sizeof(chunk_t) to fit. A legitimate chunk near
	 * the end of a heap fails that: sizeof(chunk_t) includes the rbnode, which only
	 * LARGE chunks use, and the free-list fields live in the chunk's payload area.
	 * Measured: STK produced next=prev=0x6ffd8 against heapHi=0x70000, a valid
	 * chunk that the stricter test rejected -- and rejecting it ABANDONS THE BIN,
	 * so the "hardening" was leaking live memory. */
	if (((uintptr_t)link < malloc_common.heapLo) || ((uintptr_t)link >= malloc_common.heapHi)) {
		return 0;
	}
	return 1;
}


/* Does this address look like the base of a HEAP rather than a chunk?
 *
 * A rejected free-bin link that is really a heap base decodes, field for field,
 * as a heap_t header followed by its first chunk -- and that decode is what
 * identified the SuperTuxKart corruption on hardware, by hand, from a report that
 * printed the raw words. Doing it here means the next occurrence says so itself
 * instead of waiting for someone to re-derive it.
 *
 * The tells, none of which a real chunk can satisfy at once:
 *   * page-aligned. A heap comes from mmap(); a heap's FIRST chunk sits at
 *     heap + sizeof(heap_t) and so is never page-aligned.
 *   * `size` reads as a whole number of pages -- that is heap->size.
 *   * the chunk that WOULD start at heap + sizeof(heap_t) points its ->heap back
 *     at this very address, and its size fits inside the heap.
 *
 * Deliberately no new bookkeeping: every read is inside the candidate region,
 * which the caller already range-checked against the heap window, so this cannot
 * fault where the existing report does not.
 */
static int malloc_looksLikeHeapBase(const chunk_t *chunk)
{
	const heap_t *heap = (const heap_t *)chunk;
	const chunk_t *first;
	size_t hsize;

	if (((uintptr_t)chunk & (uintptr_t)(_PAGE_SIZE - 1)) != 0u) {
		return 0;
	}

	hsize = heap->size;
	if ((hsize < (sizeof(heap_t) + CHUNK_MIN_SIZE)) || ((hsize & (_PAGE_SIZE - 1)) != 0u)) {
		return 0;
	}

	first = (const chunk_t *)((uintptr_t)chunk + sizeof(heap_t));
	if (first->heap != heap) {
		return 0;
	}

	return ((first->size & ~(size_t)(CHUNK_CUSED | CHUNK_PUSED)) <= (hsize - sizeof(heap_t))) ? 1 : 0;
}


/* Unlink `chunk` from its free bin. Returns 1 if it was actually removed, 0 if the
 * bin had to be ABANDONED (a link failed validation, so walking it would hand out
 * a corrupted chunk).
 *
 * The return matters at exactly one call site: the heap-release path. Releasing a
 * heap whose chunk is still linked leaves a free-bin entry pointing into memory we
 * just munmap'd -- and mmap reuses that region, so the entry later reads as a
 * perfectly sane chunk header belonging to the NEXT heap. That is the dangling
 * entry this allocator has been chasing, and it faulted on hardware inside
 * malloc_chunkValid() on chunk = 0x0cdfa000 (canonical, page-aligned, inside the
 * range the stale header claimed). */
static int _malloc_chunkRemove(chunk_t *chunk)
{
	unsigned int idx;
	size_t chunksz = malloc_chunkSize(chunk);
	chunk_t *next = chunk;

	if ((malloc_linkPlausible(chunk, chunk->next) == 0) ||
			(malloc_linkPlausible(chunk, chunk->prev) == 0)) {
		/* Drop the whole bin rather than unlink through a garbage pointer. Leaving
		 * the chunk in place is not an option either -- the next allocation from
		 * this bin would hand out a corrupted chunk -- so the bin is abandoned,
		 * exactly as the kernel scheduler abandons a ready queue with a non-thread
		 * on it. That leaks the chunks still in the bin; the allocator keeps
		 * working and says which bin it lost. */
		debug("malloc: free-bin link is not a plausible chunk -- abandoning the bin\n");
		malloc_debugHex("malloc:   chunk = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   next  = ", (uintptr_t)chunk->next);
		malloc_debugHex("malloc:   prev  = ", (uintptr_t)chunk->prev);
		/* Discriminators, not decoration. `size` carries the CHUNK_CUSED bit: set
		 * on a chunk sitting in a free bin means the block was handed out again
		 * while still binned, which is a different defect from a write-after-free
		 * and needs a different fix. `heap` should point at the mmap'd heap that
		 * owns this chunk; a wild value there says the header itself is gone
		 * rather than just the payload. And `pay2`/`pay3` are the payload words
		 * PAST the two link fields: the links overlap the payload (CHUNK_OVERHEAD
		 * is 16, so payload starts exactly at `next`), so whether the corruption
		 * continues beyond the first 16 bytes separates a long run of data written
		 * into a freed block from a single small field write. Measured on STK: the
		 * clobbered word is one integer near 0x8000 with its top six bytes zero,
		 * which reads as a field, not a stream -- pay2/pay3 confirm or refute that
		 * without another guess. */
		malloc_debugHex("malloc:   size  = ", (uintptr_t)chunk->size);
		malloc_debugHex("malloc:   heap  = ", (uintptr_t)chunk->heap);
		if (chunksz >= (CHUNK_OVERHEAD + (4u * sizeof(size_t)))) {
			malloc_debugHex("malloc:   pay2  = ", (uintptr_t) * (size_t *)((uintptr_t)chunk + 32u));
			malloc_debugHex("malloc:   pay3  = ", (uintptr_t) * (size_t *)((uintptr_t)chunk + 40u));
		}
		malloc_debugHex("malloc:   heapLo= ", malloc_common.heapLo);
		malloc_debugHex("malloc:   heapHi= ", malloc_common.heapHi);
		/* 1 here means the bin held a pointer to a HEAP BASE, not to a chunk --
		 * a different defect from a corrupted chunk header, and the one measured
		 * on hardware. See malloc_looksLikeHeapBase(). */
		malloc_debugHex("malloc:   hbase?= ", (uintptr_t)malloc_looksLikeHeapBase(chunk));
		/* 1 here PROVES a stale pointer into a heap we already released. */
		malloc_debugHex("malloc:   freed?= ", (uintptr_t)malloc_wasReleased(chunk));
		/* 1 here is PROOF the bin held a heap base, not a look-alike payload. */
		malloc_debugHex("malloc:   lheap?= ", (uintptr_t)malloc_isLiveHeapBase(chunk));
		/* Non-zero means the live ring wrapped over still-live entries, so a
		 * `lheap?=0` above may mean "evicted", not "not a heap". */
		malloc_debugHex("malloc:   lovfl = ", (uintptr_t)malloc_common.liveOverflow);
		if (chunksz <= CHUNK_SMALLBIN_MAX_SIZE) {
			idx = malloc_getsidx(chunksz);
			malloc_common.sbins[idx] = NULL;
			malloc_common.sbinmap &= ~(1 << idx);
		}
		else {
			/* A large bin is an rbtree plus a same-size list threaded through
			 * chunk->next/prev -- the very links just rejected -- so unlinking is
			 * not safe. Drop the tree root instead. This MUST happen: returning
			 * with the chunk still in lbins[idx] leaves _malloc_allocFrom() free
			 * to mark it CHUNK_CUSED and hand it out while lib_rbFindEx() can
			 * still find it, so the same block goes to two callers and the second
			 * free() of it reports a double free with a perfectly valid header.
			 * Both the abandon paths are deliberately symmetric now. */
			idx = malloc_getlidx(chunksz);
			malloc_common.lbins[idx].root = NULL;
			malloc_common.lbinmap &= ~(1 << idx);
		}
		return 0;
	}

	if (chunksz <= CHUNK_SMALLBIN_MAX_SIZE) {
		idx = malloc_getsidx(chunksz);
		LIST_REMOVE(&malloc_common.sbins[idx], chunk);
		if (malloc_common.sbins[idx] == NULL)
			malloc_common.sbinmap &= ~(1 << idx);

		return 1;
	}

	idx = malloc_getlidx(chunksz);
	LIST_REMOVE(&next, chunk);

	if (next == NULL) {
		lib_rbRemove(&malloc_common.lbins[idx], &chunk->node);

		if (malloc_common.lbins[idx].root == NULL)
			malloc_common.lbinmap &= ~(1 << idx);
	}
	else if (chunk->node.parent != &chunk->node) {
		/* Hand the tree node over to the new list head -- but ONLY if the tree
		 * actually links to this node.
		 *
		 * rb_transplant() writes through u->parent unconditionally
		 * (`u->parent->left = v`), and the abandon path above sets
		 * lbins[idx].root = NULL WITHOUT touching the orphaned chunks' node
		 * fields or their same-size lists. So after any abandon, a chunk can
		 * still carry a parent pointer into the dropped tree. Removing it later
		 * takes this branch and stores 8 bytes through that stale parent -- into
		 * memory the allocator may since have handed out. That is an 8-byte
		 * pointer-shaped write into somebody's payload, and it is also how a
		 * large-bin tree link comes to point INSIDE a live allocation: from there
		 * a walk yields lib_treeof(node) = node - 32, so a link landing at
		 * heap_base + 32 surfaces the heap base itself -- which is exactly what
		 * the free-bin reports show (`hbase?=1` on 24 of 25 events, measured on
		 * SuperTuxKart).
		 *
		 * The invariant that makes the write safe is local and cheap: a parent
		 * may only be written through when it points back at us. rb_transplant()
		 * dereferences that parent regardless, so testing it first is strictly
		 * safer than the status quo, never less. */
		rbnode_t *parent = chunk->node.parent;
		int linked;

		if (parent == NULL) {
			linked = (malloc_common.lbins[idx].root == &chunk->node) ? 1 : 0;
		}
		else if (malloc_common.lbins[idx].root == NULL) {
			/* Tree was abandoned; nothing can legitimately be transplanted. */
			linked = 0;
		}
		else {
			linked = ((parent->left == &chunk->node) || (parent->right == &chunk->node)) ? 1 : 0;
		}

		if (linked != 0) {
			next->node = chunk->node;
			rb_transplant(&malloc_common.lbins[idx], &chunk->node, &next->node);
			if (next->node.left != NULL)
				next->node.left->parent = &next->node;
			if (next->node.right != NULL)
				next->node.right->parent = &next->node;
		}
		else {
			/* Orphaned by an earlier abandon. Drop it from the list's point of
			 * view and leave the tree alone rather than writing through a pointer
			 * the tree no longer owns. */
			debug("malloc: stale large-bin node -- not transplanting\n");
			malloc_debugHex("malloc:   chunk  = ", (uintptr_t)chunk);
			malloc_debugHex("malloc:   parent = ", (uintptr_t)parent);
			malloc_debugHex("malloc:   root   = ", (uintptr_t)malloc_common.lbins[idx].root);
			/* The chunk is off the LIST but the tree still owns its node, so this
			 * is NOT a clean removal -- say so, or the heap-release path would
			 * unmap a heap the tree can still reach. */
			return 0;
		}
	}

	return 1;
}


static inline int malloc_chunkCanSplit(chunk_t *chunk, size_t size)
{
	return (size >= CHUNK_OVERHEAD) && (malloc_chunkSize(chunk) >= size + CHUNK_MIN_SIZE);
}


static void _malloc_chunkSplit(chunk_t *chunk, size_t size)
{
	chunk_t *sibling;

	_malloc_chunkRemove(chunk);

	sibling = (chunk_t *) ((uintptr_t) chunk + size);
	malloc_chunkInit(sibling, chunk->heap, malloc_chunkSize(chunk) - size);

	chunk->size = size | CHUNK_PUSED;
	_malloc_chunkAdd(sibling);
}


/* Report a neighbour whose header did not survive validation, and name the block.
 *
 * The join loops walk into the chunks on either side of the one being freed, and
 * those headers are NOT covered by the caller's malloc_chunkValid() check --
 * only the freed chunk itself is. A heap overflow out of the PRECEDING block
 * smashes that block's footer, malloc_chunkPrev() derives a bogus sibling from
 * it, the backward loop promotes it (it = sibling), and the forward loop's
 * malloc_chunkIsLast() is then the first code to dereference its garbage ->heap.
 * That was observed on hardware as a Data Abort at malloc_dl.c:346 with a
 * page-aligned far address and no indication of which allocation was at fault.
 *
 * Validating the neighbour turns that into this message, which names the block.
 *
 * ⚠ 2026-09-12, MEASURED: the overflow story above does NOT fit the reports. Across
 * every occurrence on record (11 events, 7 runs, all SuperTuxKart) the derived
 * `sibling` is PAGE-ALIGNED -- 11 of 11 -- while the `chunk` it was derived from is
 * only 16-byte aligned (0 of 11 page-aligned). `sibling` is `chunk + chunkSize(chunk)`,
 * so an application scribbling arbitrary bytes over a size/footer would land on a page
 * boundary with p ~ 1/256 per event; 11 for 11 is ~1e-27. The walk is reaching a page
 * boundary BY CONSTRUCTION, and the page-aligned things in this allocator are heap
 * bases and heap ends (mmap'd, page-multiple sizes).
 *
 * That points at malloc_chunkIsLast() failing to stop the walk -- i.e. the chunk's
 * ->heap or that heap's ->size disagreeing with the chunk's real heap -- rather than at
 * a userspace buffer overflow. One run showed five events whose sibling was the SAME
 * address (0x0d12a000) reached from five different chunks in three different heaps,
 * which is what a wrong chunk->heap association looks like and is not what random
 * corruption looks like. Aim there before re-litigating the overflow theory. */
static void malloc_reportBadNeighbour(const char *where, chunk_t *it, chunk_t *sibling,
	const heap_t *ref)
{
	debug("malloc: corrupt ");
	debug(where);
	debug(" neighbour chunk header -- stopping coalesce\n");
	malloc_debugHex("malloc:   chunk    = ", (uintptr_t)it);
	malloc_debugHex("malloc:   sibling  = ", (uintptr_t)sibling);
	malloc_debugHex("malloc:   heap     = ", (uintptr_t)it->heap);
	/* THE DISCRIMINATOR (2026-09-12). Every sibling on record is page-aligned, which
	 * says the walk ran to a heap EDGE rather than into scribbled-over bytes. Two ways
	 * that happens, and these fields tell them apart without another hunt:
	 *
	 *   refheap != heap  -> `it` is not in the heap the join started from. Validation
	 *                       is done against the ORIGINAL chunk->heap (captured at entry)
	 *                       while malloc_chunkIsLast() uses it->heap, so a mismatch
	 *                       alone produces this report with NO corruption anywhere.
	 *   refheap == heap  -> same heap, so the walk overran a real boundary: compare
	 *                       chunk+size against heapEnd below.
	 *
	 * heap->size is only read when the pointer passes the same window test
	 * malloc_chunkValid() uses, so a bogus ->heap cannot fault us here. */
	malloc_debugHex("malloc:   refheap  = ", (uintptr_t)ref);
	malloc_debugHex("malloc:   chunksz  = ", (uintptr_t)malloc_chunkSize(it));
	if ((it->heap != NULL) && (((uintptr_t)it->heap & (uintptr_t)(_PAGE_SIZE - 1)) == 0)
			&& ((malloc_common.heapHi == 0u)
				|| (((uintptr_t)it->heap >= malloc_common.heapLo)
					&& ((uintptr_t)it->heap < malloc_common.heapHi)))) {
		malloc_debugHex("malloc:   heapsz   = ", (uintptr_t)it->heap->size);
		malloc_debugHex("malloc:   heapEnd  = ", (uintptr_t)it->heap + it->heap->size);
	}
	else {
		debug("malloc:   heapsz   = <heap pointer outside the mmap'd window; not read>\n");
	}
}


static void _malloc_chunkJoin(chunk_t *chunk)
{
	chunk_t *it = chunk;
	chunk_t *sibling;
	/* The caller validated `chunk` against this heap, so it is the trusted
	 * reference for validating the neighbours we are about to walk into. */
	const heap_t *heap = chunk->heap;

	/* Join with the previous chunks. */
	while (!malloc_chunkIsFirst(it) && (it->size & CHUNK_PUSED) == 0) {
		sibling = malloc_chunkPrev(it);
		if ((sibling == NULL) || (malloc_chunkValid(sibling, heap) == 0)) {
			malloc_reportBadNeighbour("prev", it, sibling, heap);
			break;
		}
		_malloc_chunkRemove(sibling);
		_malloc_chunkRemove(it);

		sibling->size += malloc_chunkSize(it);
		_malloc_chunkAdd(sibling);
		it = sibling;
	}

	/* `it` may have been promoted by the loop above. malloc_chunkValid() checks a chunk's
	 * ADDRESS against the heap, never that its own ->heap field agrees, so a promoted `it`
	 * can carry a stale ->heap -- which is precisely what makes the walk below overrun to a
	 * page-aligned address. Name it here rather than inferring it from the neighbour report.
	 * (Checking `chunk->heap` would be pointless: `heap` was captured from it at entry.) */
	if (it->heap != heap) {
		malloc_debugHex("malloc: promoted chunk's ->heap disagrees with the join reference: chunk = ",
			(uintptr_t)it);
		malloc_debugHex("malloc:   its ->heap = ", (uintptr_t)it->heap);
		malloc_debugHex("malloc:   reference  = ", (uintptr_t)heap);
	}

	/* Join with the following chunks. Boundary computed against the TRUSTED reference
	 * heap, not it->heap -- see malloc_chunkIsLastIn(). */
	while (malloc_chunkIsLastIn(it, heap) == 0) {
		sibling = malloc_chunkNext(it);
		if ((sibling == NULL) || (malloc_chunkValid(sibling, heap) == 0)) {
			malloc_reportBadNeighbour("next", it, sibling, heap);
			break;
		}
		if ((sibling->size & CHUNK_CUSED) != 0) {
			break;
		}
		_malloc_chunkRemove(it);
		_malloc_chunkRemove(sibling);

		it->size += malloc_chunkSize(sibling);
		_malloc_chunkAdd(it);
	}

	malloc_chunkSetFooter(it);
}


static void malloc_heapInit(heap_t *heap, size_t size)
{
	heap->size = size;
	heap->freesz = heap->size - sizeof(heap_t);
}


static heap_t *_malloc_heapAlloc(size_t size)
{
	chunk_t *chunk;
	size_t heapSize = CEIL(sizeof(heap_t) + size, _PAGE_SIZE);
	heap_t *heap;

	if (heapSize < size) {
		return NULL;
	}

	heap = mmap(NULL, heapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (heap == MAP_FAILED) {
		return NULL;
	}

	if (malloc_common.live[malloc_common.liveIdx & 255u] != 0u) {
		/* Overwriting a still-live entry: the ring is too small for this
		 * workload and `lheap?=0` can no longer be trusted. Say so rather than
		 * report a verdict that cannot discriminate. */
		++malloc_common.liveOverflow;
	}
	malloc_common.live[malloc_common.liveIdx & 255u] = (uintptr_t)heap;
	++malloc_common.liveIdx;

	if ((malloc_common.heapLo == 0u) || ((uintptr_t)heap < malloc_common.heapLo)) {
		malloc_common.heapLo = (uintptr_t)heap;
	}
	if (((uintptr_t)heap + heapSize) > malloc_common.heapHi) {
		malloc_common.heapHi = (uintptr_t)heap + heapSize;
	}

	chunk = (chunk_t*) heap->space;

	malloc_heapInit(heap, heapSize);
	malloc_chunkInit(chunk, heap, FLOOR(heap->size - sizeof(heap_t), 8));
	chunk->size |= CHUNK_PUSED;
	_malloc_chunkAdd(chunk);
	return heap;
}


static inline void *_malloc_allocFrom(chunk_t *chunk, size_t size)
{
	chunk_t *chunkNext;

	/* ⚠ THIS CHECK MUST STAY AHEAD OF THE SPLIT. It tests CHUNK_CUSED, and
	 * _malloc_chunkSplit() ERASES that bit -- `chunk->size = size | CHUNK_PUSED`
	 * is a plain assignment, not an |=. Sitting after the split (where this used
	 * to live) the condition is provably false on every split hand-out, and a
	 * split is the COMMON case: malloc_chunkCanSplit() needs only
	 * chunkSize >= size + CHUNK_MIN_SIZE, which a large-bin chunk almost always
	 * satisfies. So the detector could not fire in exactly the case it was added
	 * for, while its comment claimed it covered "EVERY allocation path".
	 *
	 * That matters beyond the code: "duplicate hand-out refuted over ~300
	 * instrumented AF_UNIX liveness children" was banked as an eliminated
	 * mechanism in docs/KNOWN-ISSUES.md on the strength of this check. It was
	 * refuted by an instrument that could not report. The mechanism is open again.
	 *
	 * Reading the bin chunk's own header before the split is also the more
	 * informative moment: `size` is the whole free-bin chunk, and `hfree` is the
	 * pre-decrement figure, i.e. the bin state that produced the duplicate. */
	if ((chunk->size & CHUNK_CUSED) != 0) {
		/* This chunk is being handed out while already marked in use, i.e. it is
		 * live with another caller and two pointers to it now exist. The second
		 * free() of it would report a "double free" with a perfectly valid header
		 * and no way to tell that the real fault was here, one or more allocations
		 * earlier -- which is exactly how the AF_UNIX liveness child's exit 70 has
		 * resisted diagnosis. Report at the moment of the duplicate hand-out
		 * instead, where the bin state is still the state that caused it. */
		debug("malloc: chunk handed out twice -- already CHUNK_CUSED\n");
		malloc_debugHex("malloc:   chunk = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   size  = ", (uintptr_t)(chunk->size));
		malloc_debugHex("malloc:   want  = ", (uintptr_t)size);
		malloc_debugHex("malloc:   heap  = ", (uintptr_t)chunk->heap);
		malloc_debugHex("malloc:   hsize = ", (uintptr_t)chunk->heap->size);
		malloc_debugHex("malloc:   hfree = ", (uintptr_t)chunk->heap->freesz);
		malloc_debugHex("malloc:   heapLo= ", malloc_common.heapLo);
		malloc_debugHex("malloc:   heapHi= ", malloc_common.heapHi);
		_exit(EX_SOFTWARE);
	}

	if (malloc_chunkCanSplit(chunk, size))
		_malloc_chunkSplit(chunk, size);
	else
		_malloc_chunkRemove(chunk);

	chunk->heap->freesz -= malloc_chunkSize(chunk);

	chunk->size |= CHUNK_CUSED;

	if ((chunkNext = malloc_chunkNext(chunk)) != NULL)
		chunkNext->size |= CHUNK_PUSED;

	return (void *) ((uintptr_t) chunk + CHUNK_OVERHEAD);
}


/* Walk a large bin's rbtree and report any node that is not a free chunk.
 *
 * Reading the code has not produced the answer to "how does a tree node survive
 * its chunk being allocated?" -- three proposed mechanisms were each refuted by
 * their own instrument -- so observe it instead. This runs only when a lookup has
 * already come back bad and the bin is about to be dropped, so it costs nothing
 * on the normal path and cannot make a healthy tree worse.
 *
 * Every pointer is value-tested with malloc_chunkInWindow() BEFORE it is
 * dereferenced, and the visit count is bounded: the tree we are auditing is by
 * assumption damaged, so it may contain cycles as easily as wild pointers.
 *
 * CHUNK_CUSED set on a node's chunk is the finding to look for -- it means a
 * live, handed-out block is still linked into the free tree, which is the
 * structural defect rather than a symptom of it.
 */
#define MALLOC_AUDIT_MAX 24u

static void malloc_auditLargeBin(rbnode_t *node, unsigned int depth, unsigned int *visited)
{
	chunk_t *chunk;

	if ((node == NULL) || (*visited >= MALLOC_AUDIT_MAX) || (depth > 16u)) {
		return;
	}
	++(*visited);

	chunk = lib_treeof(chunk_t, node, node);
	if (malloc_chunkInWindow(chunk) == 0) {
		malloc_debugHex("malloc:   [audit] wild node   = ", (uintptr_t)node);
		return;
	}
	if ((chunk->size & CHUNK_CUSED) != 0u) {
		malloc_debugHex("malloc:   [audit] USED chunk in free tree = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   [audit]   size  = ", (uintptr_t)chunk->size);
		malloc_debugHex("malloc:   [audit]   hbase?= ", (uintptr_t)malloc_looksLikeHeapBase(chunk));
	}

	malloc_auditLargeBin(node->left, depth + 1u, visited);
	malloc_auditLargeBin(node->right, depth + 1u, visited);
}


static void *_malloc_allocLarge(size_t size)
{
	/* Lookup table to speed-up operation reverse to malloc_getlidx(). */
	static const size_t lookup[32] = {
		   0x17f,    0x1ff,    0x2ff,    0x3ff,    0x5ff,    0x7ff,    0xbff,      0xfff,
		  0x17ff,   0x1fff,   0x2fff,   0x3fff,   0x5fff,   0x7fff,   0xbfff,     0xffff,
		 0x17fff,  0x1ffff,  0x2ffff,  0x3ffff,  0x5ffff,  0x7ffff,  0xbffff,    0xfffff,
		0x17ffff, 0x1fffff, 0x2fffff, 0x3fffff, 0x5fffff, 0x7fffff, 0xbfffff,        0x0
	};

	unsigned int idx = malloc_getlidx(size);
	unsigned int binmap = malloc_common.lbinmap & ~((1 << idx) - 1);
	heap_t *heap;
	chunk_t *chunk = NULL;
	chunk_t t;
	t.size = size;

	while (idx < 32 && binmap) {
		chunk = lib_treeof(chunk_t, node, lib_rbFindEx(malloc_common.lbins[idx].root, &t.node, malloc_find));
		if (chunk != NULL)
			break;

		binmap = binmap & ~(1 << idx++);
	}

	/* The tree's answer is a POINTER DERIVED BY SUBTRACTION -- lib_treeof() takes
	 * 32 off the node address -- so a tree link that no longer points at a node
	 * yields a plausible-looking chunk pointer out of whatever bytes it landed on.
	 * The free-bin links a few lines below get malloc_linkPlausible(); this result
	 * was handed to _malloc_allocFrom() unchecked, which is asymmetric for no
	 * reason.
	 *
	 * Measured on hardware (SuperTuxKart, libphoenix 3e78cbf): a run produced 10
	 * reports whose `chunk` was a HEAP BASE (`hbase?=1`), and walking the callers
	 * of _malloc_chunkRemove() leaves this lookup as the only one that can produce
	 * one -- heap_base + 32 is the first chunk's payload, so a link landing there
	 * comes back as heap_base. Validating here turns that into a contained, named
	 * event and falls back to a fresh heap, WITHOUT needing to know yet how a node
	 * survives its chunk's allocation. */
	if ((chunk != NULL) && (malloc_chunkInWindow(chunk) == 0)) {
		/* Not even inside a heap we mmap'd: do not dereference it at all. */
		debug("malloc: large-bin lookup returned a wild pointer -- dropping the bin\n");
		malloc_debugHex("malloc:   chunk = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   heapLo= ", malloc_common.heapLo);
		malloc_debugHex("malloc:   heapHi= ", malloc_common.heapHi);
		malloc_common.lbins[idx].root = NULL;
		malloc_common.lbinmap &= ~(1 << idx);
		chunk = NULL;
	}
	else if ((chunk != NULL) && (malloc_chunkValid(chunk, chunk->heap) == 0)) {
		debug("malloc: large-bin lookup returned a non-chunk -- dropping the bin\n");
		malloc_debugHex("malloc:   chunk = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   want  = ", (uintptr_t)size);
		malloc_debugHex("malloc:   hbase?= ", (uintptr_t)malloc_looksLikeHeapBase(chunk));
		{
			unsigned int visited = 0u;
			malloc_auditLargeBin(malloc_common.lbins[idx].root, 0u, &visited);
			malloc_debugHex("malloc:   [audit] nodes = ", (uintptr_t)visited);
		}
		malloc_common.lbins[idx].root = NULL;
		malloc_common.lbinmap &= ~(1 << idx);
		chunk = NULL;
	}

	if (chunk == NULL) {
		idx = malloc_getlidx(size);
		if ((heap = _malloc_heapAlloc(max(lookup[idx], size))) == NULL)
			return NULL;

		chunk = (chunk_t *) heap->space;
	}

	return _malloc_allocFrom(chunk, size);
}


static void *_malloc_allocSmall(size_t size)
{
	unsigned int idx = malloc_getsidx(size);
	unsigned int binmap = malloc_common.sbinmap & ~((1 << idx) - 1);
	size_t targetSize = idx << 3;
	size_t idxSize;
	chunk_t *chunk;
	heap_t *heap;

	if (binmap)
		idx = __builtin_ctz(binmap);
	else if (malloc_common.lbinmap)
		return _malloc_allocLarge(size);

	idxSize = idx << 3;
	chunk = malloc_common.sbins[idx];

	/* Validate the small-bin head for the same reason _malloc_allocLarge()
	 * validates the tree's answer, and it is NOT redundant with it: measured on
	 * hardware, a run produced 34 free-bin reports whose `chunk` was a heap base
	 * while the large-bin lookup check fired ZERO times, so the bad pointer
	 * reaches _malloc_chunkRemove() through this path, not the rbtree.
	 *
	 * What misled me into checking only the large path first: the reported chunk
	 * size was 0xd000, which classifies as large -- but that size is read FROM the
	 * bad pointer, so it says nothing about which bin handed it out. A heap base
	 * taken from sbins[] reads its own heap->size as a chunk size and is then
	 * reported down the large-bin branch. Size is an output here, never evidence
	 * of provenance. */
	if ((chunk != NULL) && ((malloc_chunkInWindow(chunk) == 0) ||
			(malloc_chunkValid(chunk, chunk->heap) == 0))) {
		debug("malloc: small-bin head is not a chunk -- dropping the bin\n");
		malloc_debugHex("malloc:   chunk = ", (uintptr_t)chunk);
		malloc_debugHex("malloc:   idx   = ", (uintptr_t)idx);
		malloc_debugHex("malloc:   hbase?= ", (uintptr_t)malloc_looksLikeHeapBase(chunk));
		malloc_common.sbins[idx] = NULL;
		malloc_common.sbinmap &= ~(1 << idx);
		chunk = NULL;
	}

	if (chunk == NULL) {
		if ((heap = _malloc_heapAlloc(idxSize)) == NULL)
			return NULL;

		chunk = (chunk_t *) heap->space;

		if (malloc_chunkCanSplit(chunk, idxSize)) {
			_malloc_chunkSplit(chunk, idxSize);
			malloc_chunkSetFooter(chunk);
			_malloc_chunkAdd(chunk);
		}
	}

	return _malloc_allocFrom(chunk, targetSize);
}


size_t malloc_usable_size(void *ptr)
{
	chunk_t *chunk;
	size_t size = 0;

	if (ptr != NULL) {
		mutexLock(malloc_common.mutex);
		chunk = (chunk_t *)((uintptr_t)ptr - CHUNK_OVERHEAD);
		size = malloc_chunkSize(chunk) - CHUNK_OVERHEAD;
		mutexUnlock(malloc_common.mutex);
	}

	return size;
}


void *malloc(size_t size)
{
	void *ptr = NULL;

	/* C allows malloc(0) to return either NULL or a unique freeable pointer.
	 * glibc/BSD/dlmalloc return a valid pointer; a lot of portable software
	 * (e.g. jq's jv_mem_calloc) does `p = malloc(0); if (!p) out_of_memory();`
	 * and mis-reports OOM if we hand back NULL. Allocate a minimum chunk so
	 * size 0 yields a distinct, freeable, non-NULL pointer. */
	if (size == 0) {
		size = 1;
	}

	if ((size + CHUNK_OVERHEAD) < size) {
		errno = ENOMEM;
		return NULL;
	}

	size = CEIL(max(size + CHUNK_OVERHEAD, CHUNK_MIN_SIZE), 8);

	mutexLock(malloc_common.mutex);
	if (size <= CHUNK_SMALLBIN_MAX_SIZE) {
		ptr = _malloc_allocSmall(size);
	}
	else {
		ptr = _malloc_allocLarge(size);
	}
	mutexUnlock(malloc_common.mutex);

	if (ptr == NULL) {
		errno = ENOMEM;
	}

	return ptr;
}


void *calloc(size_t nitems, size_t size)
{
	if ((nitems != 0) && (size > SIZE_MAX / nitems)) {
		errno = ENOMEM;
		return NULL;
	}

	size_t allocSize = nitems * size;

	void *ptr = malloc(allocSize);
	if (ptr == NULL) {
		return NULL;
	}

	memset(ptr, 0, allocSize);
	return ptr;
}


void free(void *ptr)
{
	chunk_t *chunk, *chunkNext;
	heap_t *heap;
	/* Who called free(). This is THE datum both reports below were missing: they
	 * describe the block perfectly and say nothing about which code freed it, so
	 * a "double free" has to be chased by reading every free() site that could
	 * touch a block of that size. Reading it here (rather than in a helper) is
	 * what makes it free()'s caller instead of free() itself.
	 *
	 * Costs nothing on the normal path -- it is one register read, and only the
	 * report branches use it. Portable across our arches; level 0 needs no frame
	 * pointer.
	 *
	 * To resolve it: the installed binaries are stripped, so run addr2line
	 * against the unstripped copy --
	 *   aarch64-phoenix-addr2line -fe .buildroot/_build/<target>/prog/<prog> <addr>
	 * ⚠ if free() got inlined into a caller inside libphoenix itself (realloc,
	 * say) this names that caller's caller; treat it as a strong hint, not gospel. */
	const void *caller = __builtin_return_address(0);

	if (ptr == NULL)
		return;

	mutexLock(malloc_common.mutex);

	chunk = (chunk_t *) ((uintptr_t) ptr - CHUNK_OVERHEAD);
	heap = chunk->heap;

	/* Refuse a free whose header cannot be trusted. Leaking the block is
	 * strictly better than letting malloc_chunkSetFooter() below write through a
	 * corrupted size, and the report names the smashed block rather than the
	 * unrelated allocation that would fault later. */
	if (malloc_chunkValid(chunk, heap) == 0) {
		debug("malloc: free() of a corrupt chunk header -- leaking the block\n");
		malloc_debugHex("malloc:   caller= ", (uintptr_t)caller);
		malloc_debugHex("malloc:   ptr   = ", (uintptr_t)ptr);
		malloc_debugHex("malloc:   size  = ", (uintptr_t)(chunk->size));
		malloc_debugHex("malloc:   heap  = ", (uintptr_t)heap);
		/* Print the window too, so the reader can tell the two cases apart without
		 * reading this file: a `heap` outside [heapLo, heapHi) means the pointer was
		 * never allocated here at all (a stray/garbage pointer), whereas one inside
		 * it means a real block's header was smashed. */
		malloc_debugHex("malloc:   heapLo= ", malloc_common.heapLo);
		malloc_debugHex("malloc:   heapHi= ", malloc_common.heapHi);
		mutexUnlock(malloc_common.mutex);
		return;
	}

	if (!(chunk->size & CHUNK_CUSED)) {
		/* Name the block. This message used to be the bare string below, which
		 * is how it cost a session to interpret: the AF_UNIX liveness test's
		 * forked child hit this roughly 1 in 50 iterations, and all the parent
		 * could see was WEXITSTATUS == 70 (EX_SOFTWARE), with nothing to say
		 * which allocation or heap was involved -- or even that the allocator
		 * was the reporter.
		 *
		 * Note what reaching HERE already tells us, because the branch above
		 * catches the other case: malloc_chunkValid() passed, so the header is
		 * self-consistent with its heap and only the in-use bit is clear. That
		 * separates a genuine double free from a smashed header, which is the
		 * first question to ask. */
		debug("malloc: double free() -- block already on the free list\n");
		malloc_debugHex("malloc:   caller= ", (uintptr_t)caller);
		malloc_debugHex("malloc:   ptr   = ", (uintptr_t)ptr);
		malloc_debugHex("malloc:   size  = ", (uintptr_t)(chunk->size));
		malloc_debugHex("malloc:   heap  = ", (uintptr_t)heap);
		malloc_debugHex("malloc:   hsize = ", (uintptr_t)heap->size);
		malloc_debugHex("malloc:   hfree = ", (uintptr_t)heap->freesz);
		/* The two footers, which is what separates "the allocator wrote this
		 * header" from "something else did".
		 *
		 * malloc_chunkSetFooter() is the ONLY writer of the trailing size word,
		 * and free() runs it on every free, so on a header the allocator itself
		 * last touched `foot` equals the size with the flag bits masked off. If
		 * `foot` disagrees, the size word was written after the last setFooter --
		 * i.e. by a heap overrun landing on this header, or by a neighbour's
		 * over-reported size planting its footer here. Both reads are in bounds:
		 * malloc_chunkValid() above already established chunk + size <= heap end,
		 * and `prevfoot` sits inside the heap header for the first chunk.
		 *
		 * `prevfoot` is the input the backward join at _malloc_chunkJoin() would
		 * have used, so it also says whether the chunk grid below this block is
		 * intact -- which is how a stale pointer into a REUSED heap (where every
		 * printed field above is legitimately sane) gives itself away.
		 *
		 * Zero cost on the normal path: this branch already ends in _exit(). */
		malloc_debugHex("malloc:   foot  = ",
			(uintptr_t) * ((size_t *)((uintptr_t)chunk + malloc_chunkSize(chunk)) - 1));
		malloc_debugHex("malloc:   pfoot = ", (uintptr_t) * ((size_t *)chunk - 1));
		_exit(EX_SOFTWARE);
	}

	chunk->size &= ~CHUNK_CUSED;
	malloc_chunkSetFooter(chunk);

	if ((chunkNext = malloc_chunkNext(chunk)) != NULL)
		chunkNext->size &= ~CHUNK_PUSED;

	heap->freesz += malloc_chunkSize(chunk);
	_malloc_chunkAdd(chunk);
	_malloc_chunkJoin(chunk);

	if (heap->freesz == heap->size - sizeof(heap_t)) {
		chunk = (chunk_t *) heap->space;

		/* `freesz` says every byte is free; it does NOT say the heap is a single
		 * chunk. Normally it is -- coalescing merges each newly freed block into
		 * its free neighbours, so the last free() leaves one chunk spanning the
		 * heap -- but two paths break that and both are reachable:
		 *
		 *   * _malloc_chunkJoin() BREAKS out of either loop on a neighbour that
		 *     fails validation (see malloc_reportBadNeighbour), leaving the blocks
		 *     on the far side of the bad header free but separate;
		 *   * _malloc_chunkRemove() can take its abandon path while a join is
		 *     mid-merge, so a chunk stays free without being unlinked.
		 *
		 * Unmapping then leaves every free chunk in this heap OTHER than the first
		 * one still threaded into a free bin, pointing into memory we just handed
		 * back. mmap() reuses the region for the next heap of the same size, and
		 * the dangling entry now reads as a perfectly sane chunk header whose
		 * fields are the NEW heap's: `size` = the heap size (a page multiple),
		 * `heap` = its freesz, and the first chunk's size where `next` belongs.
		 * That is precisely the report seen on hardware from SuperTuxKart --
		 * 13 abandoned bins in one run, page-aligned addresses marching upward at
		 * the heap-size stride -- and it is self-feeding: each abandon can leave
		 * another chunk unremoved, producing the next dangling entry.
		 *
		 * So only release the heap when it really is one chunk covering it. The
		 * alternative is to walk a chunk grid we already have reason to distrust;
		 * leaking one heap is strictly better than a dangling free-bin entry,
		 * which is unbounded corruption. `heap->size` is a page multiple and
		 * sizeof(heap_t) is 8-aligned, so the FLOOR in _malloc_heapAlloc()'s
		 * malloc_chunkInit() is the identity here and the sizes compare exactly. */
		if (malloc_chunkSize(chunk) != (heap->size - sizeof(heap_t))) {
			debug("malloc: heap fully free but not one chunk -- not releasing it\n");
			malloc_debugHex("malloc:   heap  = ", (uintptr_t)heap);
			malloc_debugHex("malloc:   hsize = ", (uintptr_t)heap->size);
			malloc_debugHex("malloc:   csize = ", (uintptr_t)malloc_chunkSize(chunk));
		}
		else {
			/* Only release if the chunk really left its bin. _malloc_chunkRemove()
			 * ABANDONS the bin when a link fails validation, and unmapping anyway
			 * leaves that bin pointing into memory we just gave back -- which mmap
			 * then reuses, so the entry later reads as a sane header belonging to
			 * the NEXT heap. That is the dangling entry this allocator has been
			 * chasing; it faulted inside malloc_chunkValid() on a canonical,
			 * page-aligned chunk that sat inside the range the stale header
			 * claimed. Leaking one heap is strictly better, which is the same
			 * trade the size check above already makes. */
			if (_malloc_chunkRemove(chunk) == 0) {
				debug("malloc: heap release ABANDONED -- chunk still binned; leaking the heap\n");
				malloc_debugHex("malloc:   heap = ", (uintptr_t)heap);
				return;
			}
			malloc_common.released[malloc_common.relIdx & 7u].base = (uintptr_t)heap;
			malloc_common.released[malloc_common.relIdx & 7u].size = heap->size;
			++malloc_common.relIdx;
			/* Drop it from the live ring, or `lheap?=` would keep claiming a
			 * released heap is still live and invert the reading. */
			{
				unsigned int i;
				for (i = 0; i < 256u; i++) {
					if (malloc_common.live[i] == (uintptr_t)heap) {
						malloc_common.live[i] = 0u;
					}
				}
			}
			munmap(heap, heap->size);
		}
	}

	mutexUnlock(malloc_common.mutex);
}


void *realloc(void *ptr, size_t size)
{
	chunk_t *chunk, *sibling, *next;
	heap_t *heap;
	size_t chunksz;

	void *p;

	if (ptr == NULL)
		return malloc(size);

	if (size == 0) {
		free(ptr);
		return NULL;
	}

	if ((size + CHUNK_OVERHEAD) < size) {
		errno = ENOMEM;
		return NULL;
	}

	size = CEIL(max(size + CHUNK_OVERHEAD, CHUNK_MIN_SIZE), 8);

	mutexLock(malloc_common.mutex);

	chunk = (chunk_t *) ((uintptr_t) ptr - CHUNK_OVERHEAD);
	heap = chunk->heap;

	/* free() validates its chunk header before touching the heap (see the banner
	 * on malloc_chunkValid); realloc() did not, so a smashed header reached the
	 * split and the join paths unchecked. Same guard, same outcome: report and
	 * leak the block rather than derive a write address from a corrupt size. */
	if (malloc_chunkValid(chunk, heap) == 0) {
		debug("malloc: realloc() of a corrupt chunk header -- leaking the block\n");
		malloc_debugHex("malloc:   ptr    = ", (uintptr_t)ptr);
		malloc_debugHex("malloc:   heap   = ", (uintptr_t)heap);
		mutexUnlock(malloc_common.mutex);
		return NULL;
	}

	chunksz = malloc_chunkSize(chunk);


	if (size < chunksz && malloc_chunkCanSplit(chunk, size)) {
		sibling = (chunk_t *) ((uintptr_t) chunk + size);
		malloc_chunkInit(sibling, heap, chunksz - size);
		sibling->size |= CHUNK_PUSED;
		_malloc_chunkAdd(sibling);
		heap->freesz += chunksz - size;

		chunk->size = size | CHUNK_CUSED | (chunk->size & CHUNK_PUSED);

		_malloc_chunkJoin(sibling);

		if ((next = malloc_chunkNext(sibling)) != NULL)
			next->size &= ~CHUNK_PUSED;
	}
	else if (size > chunksz) {
		if ((next = malloc_chunkNext(chunk)) != NULL && !(next->size & CHUNK_CUSED) &&
				(malloc_chunkSize(next) >= (size - chunksz))) {
			_malloc_allocFrom(next, size - chunksz);
			chunk->size += malloc_chunkSize(next);
		}
		else {
			mutexUnlock(malloc_common.mutex);

			p = malloc(size);
			if (p != NULL) {
				memcpy(p, ptr, chunksz - CHUNK_OVERHEAD);
				free(ptr);
			}

			return p;
		}
	}

	mutexUnlock(malloc_common.mutex);

	return ptr;
}


void *reallocf(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);

	/* BSD reallocf(): free the original block if the resize fails. Guard on
	 * size != 0 because realloc(ptr, 0) already frees ptr and returns NULL,
	 * so freeing again here would be a double-free. */
	if (p == NULL && size != 0)
		free(ptr);

	return p;
}


void _malloc_init(void)
{
	int i;

	malloc_common.allocsz = 0;
	malloc_common.freesz = 0;
	malloc_common.sbinmap = 0;
	malloc_common.lbinmap = 0;

	for (i = 0; i < 32; ++i) {
		malloc_common.sbins[i] = NULL;
		lib_rbInit(&malloc_common.lbins[i], malloc_cmp, NULL);
	}

	mutexCreate(&malloc_common.mutex);
}


#define ASSERT(cond, ...) do {					\
		if (!(cond)) {					\
			printf(__VA_ARGS__);			\
			for (;;) ;				\
		}						\
	} while (0)


static void malloc_test_heap(chunk_t *chunk)
{
	chunk_t *next = malloc_chunkNext(chunk);
	const char *unmerged = "malloc_dl: unmerged free chunks\n";
	const char *pused = "malloc_dl: invalid PUSED flag\n";

	ASSERT(chunk->size & CHUNK_PUSED, pused);
	ASSERT(next == NULL || (next->size & CHUNK_CUSED), unmerged);
	ASSERT(next == NULL || !(next->size & CHUNK_PUSED), pused);

	next = malloc_chunkPrev(chunk);
	ASSERT(next == NULL || (next->size & CHUNK_CUSED), unmerged);
	ASSERT(chunk->size & CHUNK_PUSED, pused);
}


static void malloc_test_lbin(int lidx, chunk_t *chunk)
{
	size_t sz;
	chunk_t *c;

	if (chunk == NULL)
		return;

	malloc_test_heap(chunk);

	sz = malloc_chunkSize(chunk);
	ASSERT(!(chunk->size & CHUNK_CUSED), "malloc_dl: free chunk marked as used\n");
	ASSERT(malloc_getlidx(sz) == lidx, "malloc_dl: wrong chunk size (%zu) at lbin %d", sz, lidx);

	c = chunk;
	do  {
		ASSERT(!(c->size & CHUNK_CUSED), "malloc_dl: free chunk marked as used\n");
		ASSERT(malloc_chunkSize(c) == sz, "malloc_dl: wrong chunk size at lidx %d\n", lidx);
	} while (c->next != chunk && (c = c->next));

	malloc_test_lbin(lidx, lib_treeof(chunk_t, node, chunk->node.left));
	malloc_test_lbin(lidx, lib_treeof(chunk_t, node, chunk->node.right));
}


void malloc_test(void)
{
	int i;
	chunk_t *chunk;
	mutexLock(malloc_common.mutex);

	for (i = 0; i < 32; ++i) {
		if (malloc_common.sbinmap & (1 << i)) {
			ASSERT(malloc_common.sbins[i] != NULL, "malloc_dl: sbinmap bit %d set but bin is empty\n", i);
			chunk = malloc_common.sbins[i];

			do  {
				malloc_test_heap(chunk);
				ASSERT(!(chunk->size & CHUNK_CUSED), "malloc_dl: free chunk marked as used\n");
				ASSERT(malloc_chunkSize(chunk) == (i << 3), "malloc_dl: wrong chunk size at sidx %d\n", i);
			} while (chunk->next != malloc_common.sbins[i] && (chunk = chunk->next));
		}
		else
			ASSERT(malloc_common.sbins[i] == NULL, "malloc_dl: empty lbin %d should be NULL\n", i);
	}

	for (i = 0; i < 32; ++i) {
		if (malloc_common.lbinmap & (1 << i)) {
			ASSERT(malloc_common.lbins[i].root != NULL, "malloc_dl: lbinmap bit %d set but bin is empty\n", i);
			chunk = lib_treeof(chunk_t, node, malloc_common.lbins[i].root);

			malloc_test_lbin(i, chunk);
		}
		else
			ASSERT(malloc_common.lbins[i].root == NULL, "malloc_dl: empty lbin %d should be NULL\n", i);
	}

	mutexUnlock(malloc_common.mutex);
}
