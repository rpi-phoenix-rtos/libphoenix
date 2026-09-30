/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * net/hosts.c: getaddrinfo() answers that need no name server
 *
 * Copyright 2026 Phoenix Systems
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hosts-internal.h"


#ifndef HOSTS_FILE
#define HOSTS_FILE "/etc/hosts"
#endif

/* The address the machine's own name resolves to when /etc/hosts does not list it
 * (the Debian convention: a loopback address distinct from "localhost"). */
#define HOSTS_MYHOSTNAME_ADDR "127.0.1.1"

#define HOSTS_LINE_MAX 512
#define HOSTS_NAME_MAX 256

static const char hosts_sep[] = " \t\r\n";


/* Finds `name` among the names of an IPv4 line of the hosts file at `path`
 * ("address name [alias...]", '#' starts a comment). On a match stores the
 * address and the line's first (canonical) name and returns 0, else -1. */
static int hosts_fileLookup(const char *path, const char *name, struct in_addr *addr, char *canon, size_t canonsz)
{
	char line[HOSTS_LINE_MAX], *tok, *first, *save, *p;
	struct in_addr a;
	int found = -1, c;
	FILE *f;

	f = fopen(path, "r");
	if (f == NULL) {
		return -1;
	}

	while ((found < 0) && (fgets(line, sizeof(line), f) != NULL)) {
		if ((strchr(line, '\n') == NULL) && (feof(f) == 0)) {
			/* Over-long line: drop the rest of it rather than read it as a new line */
			do {
				c = fgetc(f);
			} while ((c != EOF) && (c != '\n'));
		}

		p = strchr(line, '#');
		if (p != NULL) {
			*p = '\0';
		}

		tok = strtok_r(line, hosts_sep, &save);
		/* IPv6 lines are skipped: the network stack has no IPv6 */
		if ((tok == NULL) || (inet_pton(AF_INET, tok, &a) != 1)) {
			continue;
		}

		first = NULL;
		while ((tok = strtok_r(NULL, hosts_sep, &save)) != NULL) {
			if (first == NULL) {
				first = tok;
			}
			if (strcasecmp(tok, name) == 0) {
				*addr = a;
				(void)strlcpy(canon, first, canonsz);
				found = 0;
				break;
			}
		}
	}

	(void)fclose(f);

	return found;
}


/* RFC 6761 6.3: "localhost" and every name under ".localhost" are loopback */
static int hosts_isLocalhost(const char *name)
{
	static const char suffix[] = ".localhost";
	size_t len = strlen(name);

	if (strcasecmp(name, "localhost") == 0) {
		return 1;
	}

	return ((len > sizeof(suffix) - 1) && (strcasecmp(name + len - (sizeof(suffix) - 1), suffix) == 0)) ? 1 : 0;
}


static int hosts_isMyHostname(const char *name)
{
	char host[HOSTS_NAME_MAX];

	if (gethostname(host, sizeof(host)) < 0) {
		return 0;
	}
	host[sizeof(host) - 1] = '\0';

	return ((host[0] != '\0') && (strcasecmp(name, host) == 0)) ? 1 : 0;
}


static int hosts_port(const char *service, const struct addrinfo *hints, in_port_t *port)
{
	const char *proto = NULL;
	struct servent *se;
	unsigned long v;
	char *end;

	if (service == NULL) {
		*port = 0;
		return 0;
	}

	if ((service[0] >= '0') && (service[0] <= '9')) {
		v = strtoul(service, &end, 10);
		if (*end == '\0') {
			if (v > 65535UL) {
				return EAI_SERVICE;
			}
			*port = htons((in_port_t)v);
			return 0;
		}
	}

	if ((hints != NULL) && ((hints->ai_flags & AI_NUMERICSERV) != 0)) {
		return EAI_NONAME;
	}

	if (hints != NULL) {
		if (hints->ai_socktype == SOCK_STREAM) {
			proto = "tcp";
		}
		else if (hints->ai_socktype == SOCK_DGRAM) {
			proto = "udp";
		}
	}

	se = getservbyname(service, proto);
	if (se == NULL) {
		return EAI_SERVICE;
	}
	*port = (in_port_t)se->s_port;

	return 0;
}


int __netdb_localAddrInfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
	struct {
		struct addrinfo ai;
		struct sockaddr_in sin;
	} *blk;
	int flags = (hints != NULL) ? hints->ai_flags : 0;
	int family = (hints != NULL) ? hints->ai_family : AF_UNSPEC;
	char canon[HOSTS_NAME_MAX];
	struct in_addr addr;
	size_t canonlen;
	in_port_t port;
	int err;

	if (node == NULL) {
		return NETDB_NOT_LOCAL;
	}

	/* An empty string names no host (glibc and musl reject it as well) */
	if (node[0] == '\0') {
		return EAI_NONAME;
	}

	/* Numeric addresses go straight to the network stack, without file I/O: the
	 * filesystem server that owns "/" resolves its own server's address, and a
	 * path lookup from that thread would be sent to the port it services. */
	if (((flags & AI_NUMERICHOST) != 0) || ((family != AF_UNSPEC) && (family != AF_INET)) ||
			(strchr(node, ':') != NULL) || (inet_aton(node, &addr) != 0)) {
		return NETDB_NOT_LOCAL;
	}

	if (hosts_fileLookup(HOSTS_FILE, node, &addr, canon, sizeof(canon)) < 0) {
		if (hosts_isLocalhost(node) != 0) {
			addr.s_addr = htonl(INADDR_LOOPBACK);
		}
		else if (hosts_isMyHostname(node) != 0) {
			(void)inet_pton(AF_INET, HOSTS_MYHOSTNAME_ADDR, &addr);
		}
		else {
			return NETDB_NOT_LOCAL;
		}
		(void)strlcpy(canon, node, sizeof(canon));
	}

	err = hosts_port(service, hints, &port);
	if (err != 0) {
		return err;
	}

	/* One block, as freeaddrinfo() is a single free() */
	canonlen = ((flags & AI_CANONNAME) != 0) ? strlen(canon) + 1 : 0;
	blk = calloc(1, sizeof(*blk) + canonlen);
	if (blk == NULL) {
		return EAI_MEMORY;
	}

	blk->sin.sin_family = AF_INET;
	blk->sin.sin_port = port;
	blk->sin.sin_addr = addr;

	blk->ai.ai_family = AF_INET;
	blk->ai.ai_socktype = (hints != NULL) ? hints->ai_socktype : 0;
	blk->ai.ai_protocol = (hints != NULL) ? hints->ai_protocol : 0;
	blk->ai.ai_addrlen = sizeof(blk->sin);
	blk->ai.ai_addr = (struct sockaddr *)&blk->sin;
	if (canonlen != 0) {
		blk->ai.ai_canonname = (char *)(blk + 1);
		(void)memcpy(blk->ai.ai_canonname, canon, canonlen);
	}

	*res = &blk->ai;

	return 0;
}
