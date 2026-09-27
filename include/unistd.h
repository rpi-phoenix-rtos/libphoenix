/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * unistd.h
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Kamil Amanowicz, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _LIBPHOENIX_UNISTD_H_
#define _LIBPHOENIX_UNISTD_H_


#include <sys/cdefs.h>
#include <arch.h>
#include <stddef.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2


#define F_OK 0
#define R_OK (1 << 2)
#define W_OK (1 << 1)
#define X_OK (1 << 0)


#define _SC_OPEN_MAX   0
#define _SC_IOV_MAX    1
#define _SC_ATEXIT_MAX 2
#define _SC_CLK_TCK    3
#define _SC_PAGESIZE   4
#define _SC_PAGE_SIZE  _SC_PAGESIZE /* spec. 1170 compatibility */
#define _SC_SPIN_LOCKS 5

/* RPi4 port: private sysconf keys at 100+ so they don't re-collide with future
 * upstream low-number additions (all binaries are statically linked, so the
 * numeric value is free to choose). Keep unistd/conf.c's sysconf() in sync. */
#define _SC_LINE_MAX         100
#define _SC_NPROCESSORS_CONF 101
#define _SC_NPROCESSORS_ONLN 102
#define _SC_PHYS_PAGES       103 /* pages of RAM the kernel manages */
#define _SC_AVPHYS_PAGES     104 /* ...of which currently free */

/* POSIX conformance version (IEEE Std 1003.1-2008). A POSIX system must define
 * this in <unistd.h>; without it, portable software (e.g. bash) falls back to
 * legacy non-POSIX code paths such as `union wait`. */
#define _POSIX_VERSION             200809L
#define _POSIX2_VERSION            200809L

#define _POSIX_NO_TRUNC             1
#define _POSIX_ASYNC_IO             -1 /* Async IO not implemented. */
#define _POSIX_FALLOC               -1 /* posix_fallocate not implemented. */
#define _POSIX_PRIO_IO              -1 /* Async IO not implemented. */
#define _POSIX_SYNC_IO              -1 /* Sync IO not implemented. */
#define _POSIX_TIMESTAMP_RESOLUTION -1 /* FS timestamps have a resolution up to a second. */
#define _POSIX_VDISABLE             '\0'

#define _PC_FILESIZEBITS         0
#define _PC_LINK_MAX             1
#define _PC_MAX_CANON            2
#define _PC_MAX_INPUT            3
#define _PC_PATH_MAX             4
#define _PC_PIPE_BUF             5
#define _PC_2_SYMLINKS           6
#define _PC_ALLOC_SIZE_MIN       7
#define _PC_REC_INCR_XFER_SIZE   8
#define _PC_REC_MAX_XFER_SIZE    9
#define _PC_REC_MIN_XFER_SIZE    10
#define _PC_REC_XFER_ALIGN       11
#define _PC_SYMLINK_MAX          12
#define _PC_TEXTDOMAIN_MAX       13
#define _PC_CHOWN_RESTRICTED     14
#define _PC_NO_TRUNC             15
#define _PC_VDISABLE             16
#define _PC_ASYNC_IO             17
#define _PC_FALLOC               18
#define _PC_PRIO_IO              19
#define _PC_SYNC_IO              20
#define _PC_TIMESTAMP_RESOLUTION 21
#define _PC_NAME_MAX             22

/* POSIX option macros — "does this implementation support feature X".
 *
 * libphoenix declares _POSIX_VERSION 200809L but used to define NO option macro
 * at all, only the limits above. That is not a cosmetic gap: the portable way to
 * ask for a monotonic clock is
 *
 *     #if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
 *
 * and on Phoenix that was always false, so portable code silently took its
 * fallback path. Two shipped ports were measured doing exactly that and timing
 * themselves off the WALL clock, which steps when ntpclient sets the time:
 * Quake II's frame timer and MicroPython's time.ticks_ms()/ticks_us() (both since
 * fixed per-port). It also defeated libstdc++'s own configure probe, which is
 * what left std::chrono::steady_clock at 1-second resolution.
 *
 * Values are 200809L to match _POSIX_VERSION. A supported option MUST be a
 * positive value, never 0 or -1: a great deal of real code tests these with a
 * bare #ifdef, so -1 ("not supported", the convention used for the unsupported
 * options above) reads as SUPPORTED to it.
 *
 * Blast radius before adding these was measured across all 58 extracted port
 * source trees rather than guessed — see
 * docs/misc/2026-09-10-posix-option-macros-blast-radius.md in the coordination
 * repo. Only claim one here when libphoenix really implements the group; the
 * groups deliberately NOT claimed (no sem_*, no pthread_barrier_*, no
 * posix_spawn, no shm_open, no sigqueue, no mq_*, no clock_getcpuclockid) are
 * left undefined so portable code keeps taking its fallback.
 */
#define _POSIX_SPIN_LOCKS 202405L

/* clock_gettime/clock_settime/clock_getres over a genuinely monotonic
 * CLOCK_MONOTONIC, plus nanosleep. */
#define _POSIX_MONOTONIC_CLOCK             200809L
/* clock_nanosleep. */
#define _POSIX_CLOCK_SELECTION             200809L
/* pthread_create/join/mutex/cond/key: the core threading group. */
#define _POSIX_THREADS                     200809L
/* pthread_attr_setstacksize / _setstack. */
#define _POSIX_THREAD_ATTR_STACKSIZE       200809L
#define _POSIX_THREAD_ATTR_STACKADDR       200809L
/* pthread_setschedparam, pthread_attr_setschedpolicy/param. */
#define _POSIX_THREAD_PRIORITY_SCHEDULING  200809L
/* sched_setscheduler/getscheduler/setparam/get_priority_max. */
#define _POSIX_PRIORITY_SCHEDULING         200809L
/* pthread_rwlock_* complete. */
#define _POSIX_READER_WRITER_LOCKS         200809L
/* mprotect. */
#define _POSIX_MEMORY_PROTECTION           200809L
/* fsync. */
#define _POSIX_FSYNC                       200809L

/* ⚠ _POSIX_TIMERS is claimed with a known shortfall, deliberately. The clock
 * side is complete — clock_gettime, clock_settime and clock_getres all exist —
 * but the timer_create/timer_settime/timer_gettime/timer_delete/timer_getoverrun
 * family does NOT. It is claimed anyway because the alternative is worse: this
 * is the macro that gates every portable "can I have a monotonic clock" test
 * (openssl's rand_unix.c and MicroPython both spell it exactly as above), so
 * leaving it undefined keeps that code silently on the wall clock. A caller that
 * reaches for timer_create() instead gets an undefined-symbol error at link
 * time, which is loud, immediate and easy to diagnose — the opposite of the
 * silent mistiming this fixes. Implementing the timer_* family is the way to
 * make this claim unconditional. */
#define _POSIX_TIMERS                      200809L

long sysconf(int name);


long pathconf(const char *path, int name);


long fpathconf(int fildes, int name);


/* NOTE: Legacy from SUSv2, new applications should use sysconf(_SC_PAGESIZE) */
__INLINE int getpagesize(void)
{
	return (int)sysconf(_SC_PAGESIZE);
}


int vfork(void);


pid_t fork(void);


int execl(const char *path, const char *arg, ...);


int execle(const char *path, const char *arg, ...);


int execlp(const char *path, const char *arg, ...);


int execve(const char *path, char *const argv[], char *const envp[]);


int execvp(const char *file, char *const argv[]);


int execvpe(const char *file, char *const argv[], char *const envp[]);


int execv(const char *path, char *const argv[]);


pid_t getpid(void);


pid_t getppid(void);


uid_t getuid(void);


uid_t geteuid(void);


gid_t getgid(void);


gid_t getegid(void);


int setuid(uid_t uid);


int seteuid(uid_t uid);


int setgid(gid_t gid);


int setegid(gid_t gid);


int usleep(useconds_t usecs);


unsigned int sleep(unsigned int seconds);


int fsync(int fd);


void sync(void);


/* Also declared in <sys/random.h>; glibc/BSD exposes getentropy() via <unistd.h>
 * too, and code that relies on that (e.g. libICE's arc4random_buf) fails with an
 * implicit-declaration error without this. Same prototype as <sys/random.h>. */
int getentropy(void *buf, size_t buflen);


int chdir(const char *path);


int fchdir(int fd);


int fchown(int fd, uid_t owner, gid_t group);


int rmdir(const char *path);


ssize_t read(int fildes, void *buf, size_t nbyte);


ssize_t write(int fildes, const void *buf, size_t nbyte);


ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);


ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);


int dup(int fildes);


int isatty(int fildes);


int dup2(int fildes, int fildes2);


void _exit(int status) __attribute__((__noreturn__));


/* Creates a symbolic link named linkpath which contains the string target. If linkpath exists, it will not be overwritten. */
int symlink(const char *target, const char *linkpath);


/* Creates a new link (also known as a hard link) to an existing file. If newpath exists, it will not be overwritten. */
int link(const char *oldpath, const char *newpath);


int unlink(const char *pathname);


ssize_t readlink(const char *path, char *buf, size_t bufsiz);


int access(const char *pathname, int mode);


/* The *at() family (dirfd-relative). AT_* flags + openat live in <fcntl.h>. */
int unlinkat(int dirfd, const char *path, int flag);


int faccessat(int dirfd, const char *path, int mode, int flag);


int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flag);


ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz);


int symlinkat(const char *target, int newdirfd, const char *linkpath);


int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flag);


char *getcwd(char *buf, size_t size);


int getopt(int argc, char *const argv[], const char *optstring);


int close(int fildes);


off_t lseek(int fildes, off_t offset, int whence);


int pipe(int fildes[2]);


int truncate(const char *path, off_t length);


int ftruncate(int fildes, off_t length);


unsigned int alarm(unsigned int seconds);


int issetugid(void);


char *getlogin(void);


int getlogin_r(char *name, size_t namesize);


char *crypt(const char *key, const char *salt);


int setpgid(pid_t pid, pid_t pgid);


pid_t getpgid(pid_t pid);


int setpgrp(void);


pid_t getpgrp(void);


pid_t setsid(void);


pid_t getsid(pid_t pid);


int gethostname(char *name, size_t namelen);


int sethostname(const char *name, size_t namelen);


long gethostid(void);


int sethostid(long hostid);


char *getpass(const char *prompt);


int getgroups(int size, gid_t list[]);


char *ttyname(int fildes);


int ttyname_r(int fildes, char *name, size_t namesize);


int pause(void);


int chroot(const char *path);


int nice(int incr);


extern char *optarg;


extern int optind, opterr, optopt;


extern char **__environ, **environ;


#ifdef __cplusplus
}
#endif


#endif
