/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * spawn.h - spawn a process (POSIX.1 advanced realtime, ADV)
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_SPAWN_H_
#define _LIBPHOENIX_SPAWN_H_

#include <sched.h>
#include <signal.h>
#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif


/* posix_spawnattr_setflags() */
#define POSIX_SPAWN_RESETIDS      0x01 /* accepted; there is only one user */
#define POSIX_SPAWN_SETPGROUP     0x02
#define POSIX_SPAWN_SETSIGDEF     0x04
#define POSIX_SPAWN_SETSIGMASK    0x08
#define POSIX_SPAWN_SETSCHEDPARAM 0x10
#define POSIX_SPAWN_SETSCHEDULER  0x20
#define POSIX_SPAWN_SETSID        0x80


typedef struct {
	short flags;
	pid_t pgroup;
	sigset_t sigdefault;
	sigset_t sigmask;
	int schedpolicy;
	struct sched_param schedparam;
} posix_spawnattr_t;


struct __spawn_action;

typedef struct {
	int used;
	int size;
	struct __spawn_action *actions;
} posix_spawn_file_actions_t;


/*
 * The child is created with vfork() and runs no allocator or other locked
 * library code before it execs: the executable and every path of the file
 * actions are resolved by the caller beforehand. A failure anywhere up to and
 * including the exec is returned by posix_spawn() itself (the child is
 * reaped), as on glibc.
 *
 * A signal the caller catches is reset to the default in the child; an ignored
 * one stays ignored unless named by POSIX_SPAWN_SETSIGDEF. An addclose() of a
 * descriptor that is not open is not an error.
 */
extern int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fileActions,
		const posix_spawnattr_t *attr, char *const argv[], char *const envp[]);


/* As posix_spawn(), searching PATH (of the caller) for a file with no '/' */
extern int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fileActions,
		const posix_spawnattr_t *attr, char *const argv[], char *const envp[]);


extern int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fileActions);


extern int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fileActions);


extern int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *__restrict fileActions, int fd,
		const char *__restrict path, int oflag, mode_t mode);


extern int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fileActions, int fd);


extern int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fileActions, int fd, int newfd);


extern int posix_spawnattr_init(posix_spawnattr_t *attr);


extern int posix_spawnattr_destroy(posix_spawnattr_t *attr);


extern int posix_spawnattr_getflags(const posix_spawnattr_t *__restrict attr, short *__restrict flags);


extern int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags);


extern int posix_spawnattr_getpgroup(const posix_spawnattr_t *__restrict attr, pid_t *__restrict pgroup);


extern int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup);


extern int posix_spawnattr_getsigdefault(const posix_spawnattr_t *__restrict attr, sigset_t *__restrict sigdefault);


extern int posix_spawnattr_setsigdefault(posix_spawnattr_t *__restrict attr, const sigset_t *__restrict sigdefault);


extern int posix_spawnattr_getsigmask(const posix_spawnattr_t *__restrict attr, sigset_t *__restrict sigmask);


extern int posix_spawnattr_setsigmask(posix_spawnattr_t *__restrict attr, const sigset_t *__restrict sigmask);


extern int posix_spawnattr_getschedparam(const posix_spawnattr_t *__restrict attr, struct sched_param *__restrict param);


extern int posix_spawnattr_setschedparam(posix_spawnattr_t *__restrict attr, const struct sched_param *__restrict param);


extern int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *__restrict attr, int *__restrict policy);


extern int posix_spawnattr_setschedpolicy(posix_spawnattr_t *attr, int policy);


#ifdef __cplusplus
}
#endif


#endif
