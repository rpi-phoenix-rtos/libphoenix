/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * net/hosts-internal.h: getaddrinfo() answers that need no name server
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_NET_HOSTS_INTERNAL_H_
#define _LIBPHOENIX_NET_HOSTS_INTERNAL_H_

#include <netdb.h>


/* Returned when the name is not one the local host answers (EAI_* are negative) */
#define NETDB_NOT_LOCAL 1


/* Resolves, in this order, a name listed in /etc/hosts (IPv4 lines),
 * "localhost"/"*.localhost" (127.0.0.1) and the machine's own gethostname()
 * (127.0.1.1), and rejects the empty name. Returns 0 with a freeaddrinfo()able
 * *res, an EAI_* error, or NETDB_NOT_LOCAL for anything the network stack has to
 * answer: NULL, numeric addresses, AI_NUMERICHOST, non-IPv4 families, and names
 * that are not local. */
extern int __netdb_localAddrInfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);


#endif
