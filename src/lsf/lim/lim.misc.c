/* $Id: lim.misc.c,v 1.8 2007/08/15 22:18:54 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lim/lim.h"

void lim_Exit(const char *fname)
{
    ls_syslog(LOG_ERR, "%s: Above fatal errors found.", fname);
    exit(EXIT_FATAL_ERROR);
}

struct hostNode *find_node_by_name(const char *host_name)
{
    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        if (equal_host(host_name, h->hostName))
            return h;
    }

    return NULL;
}

struct hostNode *find_node_by_sockaddr_in(const struct sockaddr_in *from)
{
    if (from->sin_addr.s_addr == ntohl(INADDR_LOOPBACK))
        return myHostPtr;

    for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
        struct sockaddr_in addr;
        get_host_addrv4(h->v4_epoint, &addr);
        if (is_addrv4_equal(from, &addr))
            return h;
    }

    return NULL;
}

struct hostNode *find_node_by_cluster(struct hostNode *hPtr,
                                      const char *host_name)
{
    for (struct hostNode *h = hPtr; h; h = h->nextPtr) {
        if (equal_host(host_name, h->hostName))
            return h;
    }
    return NULL;
}

int definedSharedResource(struct hostNode *host, struct lsInfo *allInfo)
{
    int i, j;
    char *resName;
    for (i = 0; i < host->numInstances; i++) {
        resName = host->instances[i]->resName;
        for (j = 0; j < allInfo->nRes; j++) {
            if (strcmp(resName, allInfo->resTable[j].name) == 0) {
                if (allInfo->resTable[j].flags & RESF_SHARED)
                    return true;
            }
        }
    }
    return false;
}

#if 0
// This is an extended version which supports also IPv6
struct hostNode *
get_node_by_sockaddr(const struct sockaddr_storage *sa)
{
    if (!sa)
        return NULL;

    if (sa->sa_family == AF_INET) {
        const struct in_addr *src = &((const struct sockaddr_in *)sa)->sin_addr;

        // fast-path: 127.0.0.1 → self
        if (src->s_addr == htonl(INADDR_LOOPBACK))
            return myHostPtr;

        for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
            if (h->v4_epoint.family != AF_INET)
                continue;
            const struct sockaddr_in *dst =
                (const struct sockaddr_in *)&h->v4_epoint.sa;
            if (dst->sin_addr.s_addr == src->s_addr)
                return h;
        }
        return NULL;
    }

    if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sa6 = (const struct sockaddr_in6 *)sa;
        const struct in6_addr *src6 = &sa6->sin6_addr;

        // fast-path: ::1 → self
        if (IN6_IS_ADDR_LOOPBACK(src6))
            return myHostPtr;

        for (struct hostNode *h = myClusterPtr->hostList; h; h = h->nextPtr) {
            if (h->n6_epoint.family != AF_INET6)
                continue;
            const struct sockaddr_in6 *dst6 =
                (const struct sockaddr_in6 *)&h->n6_epoint.sa;

            /* If you use link-local v6, also compare scope_id. */
            if (memcmp(&dst6->sin6_addr, src6, sizeof(*src6)) == 0
                /* && sa6->sin6_scope_id == dst6->sin6_scope_id */)
                return h;
        }
        return NULL;
    }

    // unsupported family
    return NULL;
}
#endif
