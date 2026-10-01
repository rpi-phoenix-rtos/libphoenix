/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * posix_spawn() and its attribute and file-action objects
 *
 * The child is made with vfork(), not fork(): fork() copies every page of
 * the caller up front, which for a large process is the bulk of the cost of
 * starting a small one. A vfork() child runs in the caller's memory with the
 * caller suspended, but it gets its own copies of the caller's locks, so it
 * must not touch anything another thread of the caller may be using under a
 * lock -- the heap first of all. Everything that allocates (path search,
 * shebang parsing, canonical paths) is therefore done here, before vfork();
 * the child only makes system calls.
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/threads.h>
#include <sys/wait.h>


#define SPAWN_FLAGS (POSIX_SPAWN_RESETIDS | POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGDEF | POSIX_SPAWN_SETSIGMASK | \
		POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER | POSIX_SPAWN_SETSID)

/* The longest "#!interpreter argument" line looked at */
#define SPAWN_SHEBANG_MAX 256


extern int sys_open(const char *filename, int oflag, ...);


enum { spawn_actOpen, spawn_actClose, spawn_actDup2 };


struct __spawn_action {
	int type;
	int fd;
	int newfd;
	int oflag;
	mode_t mode;
	char *path;
};


/* Everything the child needs, prepared by the parent */
typedef struct {
	const char *path;
	char *const *argv;
	char *const *envp;
	const posix_spawn_file_actions_t *fileActions;
	char **openPaths; /* canonical path of each open action, by index */
	const posix_spawnattr_t *attr;
	sigset_t sigmask; /* the mask the child execs with */
	volatile int err; /* set by the child if it does not get to exec */
} spawn_ctx_t;


static int spawn_applyActions(const spawn_ctx_t *ctx)
{
	const posix_spawn_file_actions_t *fa = ctx->fileActions;
	const struct __spawn_action *act;
	int i, fd, fl, err;

	if (fa == NULL) {
		return 0;
	}

	for (i = 0; i < fa->used; i++) {
		act = &fa->actions[i];

		switch (act->type) {
			case spawn_actClose:
				/* Closing a descriptor that is not open is not an error (glibc) */
				(void)close(act->fd);
				break;

			case spawn_actDup2:
				if (act->fd == act->newfd) {
					/* POSIX: the descriptor is kept, without FD_CLOEXEC */
					fl = fcntl(act->fd, F_GETFD);
					if ((fl < 0) || (fcntl(act->fd, F_SETFD, fl & ~FD_CLOEXEC) < 0)) {
						return errno;
					}
				}
				else if (dup2(act->fd, act->newfd) < 0) {
					return errno;
				}
				break;

			case spawn_actOpen:
				do {
					fd = sys_open(ctx->openPaths[i], act->oflag, act->mode);
				} while (fd == -EINTR);

				if (fd < 0) {
					return -fd;
				}
				if (fd != act->fd) {
					err = (dup2(fd, act->fd) < 0) ? errno : 0;
					(void)close(fd);
					if (err != 0) {
						return err;
					}
				}
				break;

			default:
				return EINVAL;
		}
	}

	return 0;
}


/* Runs in the vfork() child: system calls only (see the top of the file) */
static __attribute__((noreturn, noinline)) void spawn_child(spawn_ctx_t *ctx)
{
	const posix_spawnattr_t *attr = ctx->attr;
	short flags = (attr != NULL) ? attr->flags : 0;
	struct sigaction sa;
	int sig, err = 0;

	/* The caller's handlers are code in the caller's memory: none may run here.
	 * exec would reset them anyway; ignored signals stay ignored unless the
	 * caller asked for them back. All signals are blocked until the exec. */
	for (sig = 1; sig < NSIG; sig++) {
		if (sigaction(sig, NULL, &sa) != 0) {
			continue;
		}
		if ((sa.sa_handler == SIG_DFL) ||
				((sa.sa_handler == SIG_IGN) &&
						(((flags & POSIX_SPAWN_SETSIGDEF) == 0) || (sigismember(&attr->sigdefault, sig) != 1)))) {
			continue;
		}
		sa.sa_handler = SIG_DFL;
		sa.sa_flags = 0;
		(void)sigemptyset(&sa.sa_mask);
		(void)sigaction(sig, &sa, NULL);
	}

	if (((flags & POSIX_SPAWN_SETSID) != 0) && (setsid() < 0)) {
		err = errno;
	}

	if ((err == 0) && ((flags & POSIX_SPAWN_SETPGROUP) != 0) && (setpgid(0, attr->pgroup) < 0)) {
		err = errno;
	}

	if (err == 0) {
		if ((flags & POSIX_SPAWN_SETSCHEDULER) != 0) {
			if (sched_setscheduler(0, attr->schedpolicy, &attr->schedparam) < 0) {
				err = errno;
			}
		}
		else if ((flags & POSIX_SPAWN_SETSCHEDPARAM) != 0) {
			if (sched_setparam(0, &attr->schedparam) < 0) {
				err = errno;
			}
		}
	}

	if (err == 0) {
		err = spawn_applyActions(ctx);
	}

	if (err == 0) {
		(void)sigprocmask(SIG_SETMASK, &ctx->sigmask, NULL);
		err = -exec(ctx->path, ctx->argv, ctx->envp);
		if (err <= 0) {
			err = ENOEXEC;
		}
	}

	ctx->err = err;
	_exit(127);
}


/* Kept apart so that no state of the caller's frame lives across vfork() */
static __attribute__((noinline)) pid_t spawn_fork(spawn_ctx_t *ctx)
{
	pid_t pid = vfork();

	if (pid == 0) {
		spawn_child(ctx);
	}

	return pid;
}


/* The file to run: file itself, or for a PATH search the first regular file
 * named file in a PATH directory. Returns a malloc()ed path. */
static int spawn_findFile(const char *file, int search, char **out)
{
	const char *dir, *end;
	size_t dirlen, filelen = strlen(file);
	struct stat st;
	char *cand;
	int err = ENOENT;

	if (filelen == 0u) {
		return ENOENT;
	}

	if ((search == 0) || (strchr(file, '/') != NULL)) {
		*out = strdup(file);
		return (*out == NULL) ? ENOMEM : 0;
	}

	dir = getenv("PATH");
	if (dir == NULL) {
		dir = "/bin:/usr/bin";
	}

	for (;;) {
		end = strchrnul(dir, ':');
		dirlen = (size_t)(end - dir);

		cand = malloc(dirlen + filelen + 3u);
		if (cand == NULL) {
			return ENOMEM;
		}
		/* An empty PATH element is the current directory */
		if (dirlen == 0u) {
			cand[0] = '.';
			dirlen = 1u;
		}
		else {
			memcpy(cand, dir, dirlen);
		}
		cand[dirlen] = '/';
		memcpy(cand + dirlen + 1u, file, filelen + 1u);

		if (stat(cand, &st) == 0) {
			if (S_ISREG(st.st_mode)) {
				*out = cand;
				return 0;
			}
			err = EACCES;
		}
		free(cand);

		if (*end == '\0') {
			break;
		}
		dir = end + 1;
	}

	return err;
}


static int spawn_canonicalExec(const char *path, char **canonical)
{
	struct stat st;

	*canonical = resolve_path(path, NULL, 1, 0);
	if (*canonical == NULL) {
		return errno;
	}

	if ((stat(*canonical, &st) != 0) || !S_ISREG(st.st_mode)) {
		free(*canonical);
		*canonical = NULL;
		return EACCES;
	}

	return 0;
}


/* If exe is a "#!interpreter [argument]" script, run the interpreter with
 * the script's path in place of argv[0], as execve() does. */
static int spawn_shebang(const char *exe, char *const argv[], char **interpBuf, char ***newArgv)
{
	char *buf, *p, *interp, *arg = NULL;
	char **nargv;
	int fd, argc, n, i;
	ssize_t len;

	*interpBuf = NULL;
	*newArgv = NULL;

	fd = open(exe, O_RDONLY);
	if (fd < 0) {
		return errno;
	}

	buf = malloc(SPAWN_SHEBANG_MAX + 1);
	if (buf == NULL) {
		(void)close(fd);
		return ENOMEM;
	}

	do {
		len = read(fd, buf, SPAWN_SHEBANG_MAX);
	} while ((len < 0) && (errno == EINTR));
	(void)close(fd);

	if ((len < 2) || (buf[0] != '#') || (buf[1] != '!')) {
		free(buf);
		return 0;
	}
	buf[len] = '\0';

	p = buf + 2;
	p[strcspn(p, "\n")] = '\0';
	p += strspn(p, " \t");
	interp = p;
	p += strcspn(p, " \t");
	if (*p != '\0') {
		*p++ = '\0';
		p += strspn(p, " \t");
		if (*p != '\0') {
			/* The rest of the line is one argument, as on Linux */
			arg = p;
			p += strlen(p);
			while ((p > arg) && ((p[-1] == ' ') || (p[-1] == '\t'))) {
				*--p = '\0';
			}
		}
	}

	if (*interp == '\0') {
		free(buf);
		return ENOEXEC;
	}

	for (argc = 0; (argv != NULL) && (argv[argc] != NULL); argc++) {
	}

	/* interpreter, argument, script, argv[1..], NULL */
	nargv = malloc(((size_t)argc + 3u) * sizeof(char *));
	if (nargv == NULL) {
		free(buf);
		return ENOMEM;
	}

	n = 0;
	nargv[n++] = interp;
	if (arg != NULL) {
		nargv[n++] = arg;
	}
	nargv[n++] = (char *)exe;
	for (i = 1; i < argc; i++) {
		nargv[n++] = argv[i];
	}
	nargv[n] = NULL;

	*interpBuf = buf;
	*newArgv = nargv;

	return 0;
}


static void spawn_freeOpenPaths(const posix_spawn_file_actions_t *fa, char **paths)
{
	int i;

	if (paths != NULL) {
		for (i = 0; i < fa->used; i++) {
			free(paths[i]);
		}
		free(paths);
	}
}


static int spawn_prepareOpens(const posix_spawn_file_actions_t *fa, char ***out)
{
	const struct __spawn_action *act;
	struct stat st;
	char **paths;
	int i;

	*out = NULL;
	if ((fa == NULL) || (fa->used == 0)) {
		return 0;
	}

	paths = calloc((size_t)fa->used, sizeof(char *));
	if (paths == NULL) {
		return ENOMEM;
	}

	for (i = 0; i < fa->used; i++) {
		act = &fa->actions[i];
		if (act->type != spawn_actOpen) {
			continue;
		}

		/* What open() does before its system call */
		if (((act->oflag & (O_WRONLY | O_RDWR)) != 0) && (stat(act->path, &st) == 0) && S_ISDIR(st.st_mode)) {
			spawn_freeOpenPaths(fa, paths);
			return EISDIR;
		}

		paths[i] = resolve_path(act->path, NULL, 1, 1);
		if (paths[i] == NULL) {
			int err = errno;
			spawn_freeOpenPaths(fa, paths);
			return err;
		}
	}

	*out = paths;
	return 0;
}


static int spawn_common(pid_t *pidp, const char *file, const posix_spawn_file_actions_t *fileActions,
		const posix_spawnattr_t *attr, char *const argv[], char *const envp[], int search)
{
	static char *const noArgs[] = { NULL };
	spawn_ctx_t ctx;
	char *exe = NULL, *canonical = NULL, *interpBuf = NULL, **shebangArgv = NULL, **openPaths = NULL;
	sigset_t all, old;
	int err, status, savedErrno = errno;
	pid_t pid;

	if (file == NULL) {
		return EINVAL;
	}
	if (argv == NULL) {
		argv = noArgs;
	}

	err = spawn_findFile(file, search, &exe);
	if (err == 0) {
		err = spawn_shebang(exe, argv, &interpBuf, &shebangArgv);
	}
	if (err == 0) {
		err = spawn_canonicalExec((shebangArgv != NULL) ? shebangArgv[0] : exe, &canonical);
	}
	if (err == 0) {
		err = spawn_prepareOpens(fileActions, &openPaths);
	}

	if (err == 0) {
		ctx.path = canonical;
		ctx.argv = (shebangArgv != NULL) ? shebangArgv : argv;
		ctx.envp = envp;
		ctx.fileActions = fileActions;
		ctx.openPaths = openPaths;
		ctx.attr = attr;
		ctx.err = 0;

		/* Nothing may be delivered to the child before it has reset the
		 * handlers; it execs with the requested mask, or the caller's */
		(void)sigfillset(&all);
		(void)sigprocmask(SIG_BLOCK, &all, &old);
		ctx.sigmask = ((attr != NULL) && ((attr->flags & POSIX_SPAWN_SETSIGMASK) != 0)) ? attr->sigmask : old;

		pid = spawn_fork(&ctx);

		(void)sigprocmask(SIG_SETMASK, &old, NULL);

		/* The child has exec'd or exited by now */
		if (pid < 0) {
			err = -pid;
		}
		else if (ctx.err != 0) {
			err = ctx.err;
			while ((waitpid(pid, &status, 0) < 0) && (errno == EINTR)) {
			}
		}
		else if (pidp != NULL) {
			*pidp = pid;
		}
	}

	if (fileActions != NULL) {
		spawn_freeOpenPaths(fileActions, openPaths);
	}
	free(shebangArgv);
	free(interpBuf);
	free(canonical);
	free(exe);

	errno = savedErrno;
	return err;
}


int posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *fileActions,
		const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
	return spawn_common(pid, path, fileActions, attr, argv, envp, 0);
}


int posix_spawnp(pid_t *pid, const char *file, const posix_spawn_file_actions_t *fileActions,
		const posix_spawnattr_t *attr, char *const argv[], char *const envp[])
{
	return spawn_common(pid, file, fileActions, attr, argv, envp, 1);
}


int posix_spawn_file_actions_init(posix_spawn_file_actions_t *fileActions)
{
	fileActions->used = 0;
	fileActions->size = 0;
	fileActions->actions = NULL;
	return 0;
}


int posix_spawn_file_actions_destroy(posix_spawn_file_actions_t *fileActions)
{
	int i;

	for (i = 0; i < fileActions->used; i++) {
		free(fileActions->actions[i].path);
	}
	free(fileActions->actions);
	fileActions->actions = NULL;
	fileActions->used = 0;
	fileActions->size = 0;

	return 0;
}


static struct __spawn_action *spawn_addAction(posix_spawn_file_actions_t *fileActions)
{
	struct __spawn_action *actions;
	int size;

	if (fileActions->used == fileActions->size) {
		size = (fileActions->size == 0) ? 4 : (2 * fileActions->size);
		actions = realloc(fileActions->actions, (size_t)size * sizeof(*actions));
		if (actions == NULL) {
			return NULL;
		}
		fileActions->actions = actions;
		fileActions->size = size;
	}

	actions = &fileActions->actions[fileActions->used];
	memset(actions, 0, sizeof(*actions));

	return actions;
}


int posix_spawn_file_actions_addopen(posix_spawn_file_actions_t *__restrict fileActions, int fd,
		const char *__restrict path, int oflag, mode_t mode)
{
	struct __spawn_action *act;
	char *copy;

	if (fd < 0) {
		return EBADF;
	}

	copy = strdup(path);
	if (copy == NULL) {
		return ENOMEM;
	}

	act = spawn_addAction(fileActions);
	if (act == NULL) {
		free(copy);
		return ENOMEM;
	}

	act->type = spawn_actOpen;
	act->fd = fd;
	act->oflag = oflag;
	act->mode = ((oflag & O_CREAT) != 0) ? (mode & ~__getumask()) : mode;
	act->path = copy;
	fileActions->used++;

	return 0;
}


int posix_spawn_file_actions_addclose(posix_spawn_file_actions_t *fileActions, int fd)
{
	struct __spawn_action *act;

	if (fd < 0) {
		return EBADF;
	}

	act = spawn_addAction(fileActions);
	if (act == NULL) {
		return ENOMEM;
	}

	act->type = spawn_actClose;
	act->fd = fd;
	fileActions->used++;

	return 0;
}


int posix_spawn_file_actions_adddup2(posix_spawn_file_actions_t *fileActions, int fd, int newfd)
{
	struct __spawn_action *act;

	if ((fd < 0) || (newfd < 0)) {
		return EBADF;
	}

	act = spawn_addAction(fileActions);
	if (act == NULL) {
		return ENOMEM;
	}

	act->type = spawn_actDup2;
	act->fd = fd;
	act->newfd = newfd;
	fileActions->used++;

	return 0;
}


int posix_spawnattr_init(posix_spawnattr_t *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->schedpolicy = SCHED_RR;
	return 0;
}


int posix_spawnattr_destroy(posix_spawnattr_t *attr)
{
	(void)attr;
	return 0;
}


int posix_spawnattr_getflags(const posix_spawnattr_t *__restrict attr, short *__restrict flags)
{
	*flags = attr->flags;
	return 0;
}


int posix_spawnattr_setflags(posix_spawnattr_t *attr, short flags)
{
	if ((flags & ~SPAWN_FLAGS) != 0) {
		return EINVAL;
	}

	attr->flags = flags;
	return 0;
}


int posix_spawnattr_getpgroup(const posix_spawnattr_t *__restrict attr, pid_t *__restrict pgroup)
{
	*pgroup = attr->pgroup;
	return 0;
}


int posix_spawnattr_setpgroup(posix_spawnattr_t *attr, pid_t pgroup)
{
	attr->pgroup = pgroup;
	return 0;
}


int posix_spawnattr_getsigdefault(const posix_spawnattr_t *__restrict attr, sigset_t *__restrict sigdefault)
{
	*sigdefault = attr->sigdefault;
	return 0;
}


int posix_spawnattr_setsigdefault(posix_spawnattr_t *__restrict attr, const sigset_t *__restrict sigdefault)
{
	attr->sigdefault = *sigdefault;
	return 0;
}


int posix_spawnattr_getsigmask(const posix_spawnattr_t *__restrict attr, sigset_t *__restrict sigmask)
{
	*sigmask = attr->sigmask;
	return 0;
}


int posix_spawnattr_setsigmask(posix_spawnattr_t *__restrict attr, const sigset_t *__restrict sigmask)
{
	attr->sigmask = *sigmask;
	return 0;
}


int posix_spawnattr_getschedparam(const posix_spawnattr_t *__restrict attr, struct sched_param *__restrict param)
{
	*param = attr->schedparam;
	return 0;
}


int posix_spawnattr_setschedparam(posix_spawnattr_t *__restrict attr, const struct sched_param *__restrict param)
{
	attr->schedparam = *param;
	return 0;
}


int posix_spawnattr_getschedpolicy(const posix_spawnattr_t *__restrict attr, int *__restrict policy)
{
	*policy = attr->schedpolicy;
	return 0;
}


int posix_spawnattr_setschedpolicy(posix_spawnattr_t *attr, int policy)
{
	if ((policy != SCHED_FIFO) && (policy != SCHED_RR) && (policy != SCHED_OTHER)) {
		return EINVAL;
	}

	attr->schedpolicy = policy;
	return 0;
}
