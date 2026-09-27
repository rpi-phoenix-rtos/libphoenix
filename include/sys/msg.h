/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/msg
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_MSG_H_
#define _LIBPHOENIX_MSG_H_

#include <stddef.h>
#include <sys/types.h>
#include <phoenix/msg.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Port management */


extern int portCreate(uint32_t *port);

extern void portDestroy(uint32_t port);

extern int portRegister(uint32_t port, const char *name, oid_t *oid);

extern int portUnregister(const char *name);

extern int lookup(const char *name, oid_t *file, oid_t *dev);


/* Message passing */


extern int msgSend(uint32_t port, msg_t *m);

extern int msgPulse(uint32_t port, msg_t *m);

extern int msgRecv(uint32_t port, msg_t *m, msg_rid_t *rid);

extern int msgRespond(uint32_t port, msg_t *m, msg_rid_t rid);


/* Poll readiness */


/*
 * A server tells the kernel that the readiness of one of its oids may have
 * changed (an event was queued, space was freed, the peer hung up), so poll()
 * and select() callers watching that oid send it a fresh atPollStatus query at
 * once instead of after the kernel's timed re-poll. Call it AFTER the state an
 * atPollStatus answer reads has been updated. Only the owner of oid->port may
 * notify (-EPERM otherwise); with no poller watching it is a no-op. A kernel
 * without the call returns -EINVAL, and pollers then fall back to the timed
 * re-poll, so a server may call it unconditionally.
 */
extern int pollNotify(const oid_t *oid);


#ifdef __cplusplus
}
#endif


#endif
