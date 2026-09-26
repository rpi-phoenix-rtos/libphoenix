/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * unistd (POSIX routines for user operations)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Michal Miroslaw
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/syslimits.h>
#include <sys/statvfs.h>
#include <limits.h>
#include <sys/mman.h>

/* _SC_NPROCESSORS_* needs the CPU count. On aarch64-generic (RPi4) the kernel
 * exposes it via platformctl(pctl_cpucount); other targets fall back to EINVAL
 * (matches the previous unimplemented behaviour). Kept as a guarded inline (like
 * arch/aarch64/reboot.c's __CPU_GENERIC guard) rather than a cross-arch hook. */
#if defined(__aarch64__) && defined(__CPU_GENERIC)
#include <sys/platform.h>
#include <phoenix/arch/aarch64/generic/generic.h>
#endif


__EXPORT_INLINE int getpagesize(void);


/* _SC_PHYS_PAGES / _SC_AVPHYS_PAGES from the kernel's page allocator, via
 * meminfo(). mapsz = -1 asks for the counters only, not the page/entry/map
 * tables. The kernel always sets page.sz (sizeof(page_t)), so a zero there
 * means the call did nothing.
 *
 * ⚠ meminfo_t carries the byte counts in `unsigned int`, so they wrap above
 * 4 GiB of managed RAM (kernel FIXME in vm_mapinfo). A 4 GB Pi 4 manages
 * 3997696 KB, just below that; an 8 GB board would need the kernel fields
 * widened first. */
static long conf_physPages(int avail)
{
	meminfo_t info;
	unsigned long long bytes;

	memset(&info, 0, sizeof(info));
	info.page.mapsz = -1;
	info.entry.mapsz = -1;
	info.entry.kmapsz = -1;
	info.maps.mapsz = -1;

	meminfo(&info);
	if (info.page.sz == 0U) {
		errno = EINVAL;
		return -1;
	}

	bytes = (unsigned long long)info.page.free;
	if (avail == 0) {
		bytes += (unsigned long long)info.page.alloc;
	}

	return (long)(bytes / _PAGE_SIZE);
}


long sysconf(int name)
{
	switch (name) {
		case _SC_OPEN_MAX:
			/* Must match the kernel's per-process fd limit (MAX_FD_COUNT in
			 * phoenix-rtos-kernel posix.c, currently 1024); returning less makes
			 * fd-array-sizing / fd-iterating programs miss the upper fds.
			 * TODO: expose MAX_FD_COUNT via a shared header instead of hardcoding. */
			return 1024;
		case _SC_IOV_MAX:
			return IOV_MAX;
		case _SC_ATEXIT_MAX:
			/* we have no limit since we use lists */
			return INT_MAX;
		case _SC_PAGESIZE:
			/* _SC_PAGE_SIZE is synonym */
			return _PAGE_SIZE;
		case _SC_SPIN_LOCKS:
			return _POSIX_SPIN_LOCKS;
		case _SC_LINE_MAX:
			return _POSIX2_LINE_MAX;
		case _SC_CLK_TCK:
			/* clock_t ticks/second for times(); the conventional fixed value
			 * (glibc returns 100 regardless of kernel HZ). Software such as
			 * CPython's _Py_GetTicksPerSecond fails startup if this is < 1. */
			return 100;
		case _SC_NPROCESSORS_CONF:
		case _SC_NPROCESSORS_ONLN:
#if defined(__aarch64__) && defined(__CPU_GENERIC)
			{
				platformctl_t pctl = { 0 };
				pctl.action = pctl_get;
				pctl.type = pctl_cpucount;
				if (platformctl(&pctl) == 0) {
					return (long)pctl.task.cpucount.count;
				}
			}
#endif
			errno = EINVAL;
			return -1;
		case _SC_PHYS_PAGES:
			return conf_physPages(0);
		case _SC_AVPHYS_PAGES:
			return conf_physPages(1);
		default:
			errno = EINVAL;
			return -1;
	}
}


long pathconfStatic(int name)
{
	long res = -1;

	switch (name) {
		case _PC_FILESIZEBITS:
#ifdef FILESIZEBITS
			res = FILESIZEBITS;
#endif
			break;
		case _PC_LINK_MAX:
#ifdef LINK_MAX
			res = LINK_MAX;
#endif
			break;
		case _PC_MAX_CANON:
#ifdef MAX_CANON
			res = MAX_CANON;
#endif
			break;
		case _PC_MAX_INPUT:
#ifdef MAX_INPUT
			res = MAX_INPUT;
#endif
			break;
		case _PC_PATH_MAX:
#ifdef PATH_MAX
			res = PATH_MAX;
#endif
			break;
		case _PC_PIPE_BUF:
#ifdef PIPE_BUF
			res = PIPE_BUF;
#endif
			break;
		case _PC_2_SYMLINKS:
#ifdef POSIX2_SYMLINKS
			res = POSIX2_SYMLINKS;
#endif
			break;
		case _PC_ALLOC_SIZE_MIN:
#ifdef POSIX_ALLOC_SIZE_MIN
			res = POSIX_ALLOC_SIZE_MIN;
#endif
			break;
		case _PC_REC_INCR_XFER_SIZE:
#ifdef POSIX_REC_INCR_XFER_SIZE
			res = POSIX_REC_INCR_XFER_SIZE;
#endif
			break;
		case _PC_REC_MAX_XFER_SIZE:
#ifdef POSIX_REC_MAX_XFER_SIZE
			res = POSIX_REC_MAX_XFER_SIZE;
#endif
			break;
		case _PC_REC_MIN_XFER_SIZE:
#ifdef POSIX_REC_MIN_XFER_SIZE
			res = POSIX_REC_MIN_XFER_SIZE;
#endif
			break;
		case _PC_REC_XFER_ALIGN:
#ifdef POSIX_REC_XFER_ALIGN
			res = POSIX_REC_XFER_ALIGN;
#endif
			break;
		case _PC_SYMLINK_MAX:
#ifdef SYMLINK_MAX
			res = SYMLINK_MAX;
#endif
			break;
		case _PC_TEXTDOMAIN_MAX:
#ifdef TEXTDOMAIN_MAX
			res = TEXTDOMAIN_MAX;
#endif
			break;
		case _PC_CHOWN_RESTRICTED:
#ifdef _POSIX_CHOWN_RESTRICTED
			res = _POSIX_CHOWN_RESTRICTED;
#endif
			break;
		case _PC_NO_TRUNC:
#ifdef _POSIX_NO_TRUNC
			res = _POSIX_NO_TRUNC;
#endif
			break;
		case _PC_VDISABLE:
#ifdef _POSIX_VDISABLE
			res = _POSIX_VDISABLE;
#endif
			break;
		case _PC_ASYNC_IO:
#ifdef _POSIX_ASYNC_IO
			res = _POSIX_ASYNC_IO;
#endif
			break;
		case _PC_FALLOC:
#ifdef _POSIX_FALLOC
			res = _POSIX_FALLOC;
#endif
			break;
		case _PC_PRIO_IO:
#ifdef _POSIX_PRIO_IO
			res = _POSIX_PRIO_IO;
#endif
			break;
		case _PC_SYNC_IO:
#ifdef _POSIX_SYNC_IO
			res = _POSIX_SYNC_IO;
#endif
			break;
		case _PC_TIMESTAMP_RESOLUTION:
#ifdef _POSIX_TIMESTAMP_RESOLUTION
			res = _POSIX_TIMESTAMP_RESOLUTION;
#endif
			break;
		default:
			errno = EINVAL;
			break;
	}
	return res;
}


long pathconf(const char *path, int name)
{
	long res = -1;
	struct statvfs stat;

	switch (name) {
		case _PC_NAME_MAX:
			if (statvfs(path, &stat) == 0) {
				res = (long)stat.f_namemax;
			}
			break;
		default:
			res = pathconfStatic(name);
			break;
	}

	return res;
}


long fpathconf(int fildes, int name)
{
	long res = -1;
	struct statvfs stat;

	switch (name) {
		case _PC_NAME_MAX:
			if (fstatvfs(fildes, &stat) == 0) {
				res = (long)stat.f_namemax;
			}
			else if (errno == ENOSYS) {
				errno = EINVAL;
			}
			break;
		default:
			res = pathconfStatic(name);
			break;
	}

	return res;
}
