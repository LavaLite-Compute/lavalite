/* ------------------------------------------------------------------------
 * LavaLite — High-Performance Job Scheduling Infrastructure
 *
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 * ------------------------------------------------------------------------ */

#pragma once

#include "lsf/lib/lib.common.h"

/* global max hostname length
 */
#ifndef LL_HOSTNAME_MAX
#define LL_HOSTNAME_MAX 255
#endif

struct ll_host {
    int family; /* AF_INET / AF_INET6 */
    socklen_t salen;
    struct sockaddr_storage sa; /* canonical socket addr */
    char name[LL_HOSTNAME_MAX]; /* canonical hostname or "" */
    char addr[LL_HOSTNAME_MAX]; /* numeric IP string */
};

/* Lookup by hostname; fills addr + name (reverse-confirmed if available).
 * Returns 0 on success, -1 on failure (errno set).
 */
int get_host_by_name(const char *, struct ll_host *);

/* Lookup by numeric address (e.g., "10.0.0.5" or "2001:db8::1");
 * fills addr + name (reverse DNS if available, non-fatal if missing).
 * Returns 0 on success, -1 on failure (errno set).
 */
int get_host_by_addrstr(const char *ip, struct ll_host *);
int get_host_by_sockaddr(const struct sockaddr *, socklen_t, struct ll_host *);
int equal_host(const char *, const char *);

// sockaddr_in family of routines
char *sockAdd2Str_(struct sockaddr_in *);
int get_host_addrv4(const struct ll_host *, struct sockaddr_in *);
int get_host_sinaddrv4(const struct ll_host *, struct sockaddr_in *);
int is_addrv4_zero(const struct sockaddr_in *);
int is_addrv4_equal(const struct sockaddr_in *, const struct sockaddr_in *);
int is_addrv6_zero(const struct sockaddr_in6 *);
int is_addrv6_equal(const struct sockaddr_in6 *, const struct sockaddr_in6 *);

// Handy wrappers
int get_host_by_sockaddr_in(const struct sockaddr_in *, struct ll_host *);
int get_host_by_sockaddr_in6(const struct sockaddr_in6 *, struct ll_host *);
