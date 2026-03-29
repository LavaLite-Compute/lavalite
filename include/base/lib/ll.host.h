/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#pragma once

struct ll_host {
    int family;
    socklen_t salen;
    struct sockaddr_storage sa;
    char name[MAXHOSTNAMELEN];
    char addr[MAXHOSTNAMELEN];
};

int get_host_by_name(const char *, struct ll_host *);
int get_host_by_addrstr(const char *, struct ll_host *);
int get_host_addrv4(const struct ll_host *, struct sockaddr_in *);
const char *addr_to_str(struct sockaddr_in *);
