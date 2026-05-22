/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "base/lib/ll.host.h"

static void fill_addrstr(const struct sockaddr *sa, char *buf, size_t bufsz)
{
    if (!sa || !buf || bufsz == 0)
        return;

    buf[0] = 0;

    switch (sa->sa_family) {
    case AF_INET: {
        const struct sockaddr_in *sin = (const struct sockaddr_in *) sa;
        if (!inet_ntop(AF_INET, &sin->sin_addr, buf, bufsz))
            buf[0] = 0;
        break;
    }
    case AF_INET6: {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) sa;
        if (!inet_ntop(AF_INET6, &sin6->sin6_addr, buf, bufsz))
            buf[0] = 0;
        break;
    }
    default:
        buf[0] = 0;
        break;
    }
}

int get_host_by_name(const char *hostname, struct ll_host *hp)
{
    if (!hostname || !*hostname || !hp) {
        errno = EINVAL;
        return -1;
    }

    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = 0;
    hints.ai_flags = AI_ADDRCONFIG;

    int rc = getaddrinfo(hostname, NULL, &hints, &ai);
    if (rc != 0) {
        errno = EHOSTUNREACH;
        return -1;
    }

    const struct addrinfo *res = ai;

    memset(hp, 0, sizeof(*hp));
    hp->family = res->ai_family;
    hp->salen = (socklen_t) res->ai_addrlen;
    memcpy(&hp->sa, res->ai_addr, res->ai_addrlen);

    fill_addrstr((const struct sockaddr *) &hp->sa, hp->addr, sizeof(hp->addr));

    if (res->ai_canonname && *res->ai_canonname)
        strncpy(hp->name, res->ai_canonname, sizeof(hp->name) - 1);
    else {
        strncpy(hp->name, hostname, sizeof(hp->name) - 1);
        hp->name[sizeof(hp->name) - 1] = 0;
    }

    freeaddrinfo(ai);
    return 0;
}

int get_host_by_addrstr(const char *ip, struct ll_host *hp)
{
    if (!ip || !*ip || !hp) {
        errno = EINVAL;
        return -1;
    }

    memset(hp, 0, sizeof(*hp));

    struct sockaddr_in v4;
    struct sockaddr_in6 v6;

    if (inet_pton(AF_INET, ip, &v4.sin_addr) == 1) {
        v4.sin_family = AF_INET;
        hp->family = AF_INET;
        hp->salen = sizeof(v4);
        memcpy(&hp->sa, &v4, sizeof(v4));
    } else if (inet_pton(AF_INET6, ip, &v6.sin6_addr) == 1) {
        v6.sin6_family = AF_INET6;
        hp->family = AF_INET6;
        hp->salen = sizeof(v6);
        memcpy(&hp->sa, &v6, sizeof(v6));
    } else {
        errno = EINVAL;
        return -1;
    }

    strncpy(hp->addr, ip, sizeof(hp->addr) - 1);

    getnameinfo((const struct sockaddr *) &hp->sa, hp->salen, hp->name,
                sizeof(hp->name), NULL, 0, NI_NAMEREQD);

    return 0;
}

int get_host_addrv4(const struct ll_host *hp, struct sockaddr_in *out)
{
    if (!hp || !out) {
        errno = EINVAL;
        return -1;
    }

    if (hp->family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }

    memcpy(out, (const struct sockaddr_in *) &hp->sa, sizeof(*out));
    return 0;
}

const char *addr_to_str(struct sockaddr_in *from)
{
    static __thread char buf[INET_ADDRSTRLEN + 8];
    char ip[INET_ADDRSTRLEN];

    if (!from) {
        strcpy(buf, "?.?:?");
        return buf;
    }

    if (!inet_ntop(AF_INET, &from->sin_addr, ip, sizeof(ip))) {
        strcpy(buf, "?.?:?");
        return buf;
    }

    unsigned port = (unsigned) ntohs(from->sin_port);
    snprintf(buf, sizeof(buf), "%s:%u", ip, port);

    return buf;
}
