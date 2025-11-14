#define _GNU_SOURCE

#include "lsf/lib/ll.host.h"

static void fill_addrstr(const struct sockaddr *sa, socklen_t salen, char *buf,
                         size_t bufsz)
{
    // fill_addrstr(): fast numeric form (no DNS)

    if (!sa || !buf || bufsz == 0)
        return;

    buf[0] = '\0';

    switch (sa->sa_family) {
    case AF_INET: {
        const struct sockaddr_in *sin = (const struct sockaddr_in *) sa;
        if (!inet_ntop(AF_INET, &sin->sin_addr, buf, bufsz))
            buf[0] = '\0';
        break;
    }
    case AF_INET6: {
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) sa;
        if (!inet_ntop(AF_INET6, &sin6->sin6_addr, buf, bufsz))
            buf[0] = '\0';
        break;
    }
    default:
        // unsupported family: leave empty string
        buf[0] = '\0';
        break;
    }
}

static int try_reverse_dns(const struct sockaddr *sa, socklen_t salen,
                           char *buf, size_t bufsz)
{
    if (!sa || !buf || bufsz == 0) {
        errno = EINVAL;
        return -1;
    }

    if (getnameinfo(sa, salen, buf, bufsz, NULL, 0, NI_NAMEREQD) == 0) {
        return 0;
    }

    errno = ENODATA; // good generic signal: reverse lookup failed
    buf[0] = 0;
    return -1;
}

int get_host_by_name(const char *hostname, struct ll_host *hp)
{
    if (!hostname || !*hostname || !hp) {
        errno = EINVAL;
        return -1;
    }

    /*
     * We currently resolve only IPv4 addresses,
     * but we keep AF_UNSPEC in comments as a reminder of the future dual-stack
     * path.
     *
     * When IPv6 is added, we can simply switch AF_UNSPEC and
     * store both ai_family == AF_INET and AF_INET6 results.
     */
    struct addrinfo hints;
    struct addrinfo *ai = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // v4
    hints.ai_socktype = 0;     // any
    hints.ai_flags = AI_ADDRCONFIG | AI_CANONNAME;

    int rc = getaddrinfo(hostname, NULL, &hints, &ai);
    if (rc != 0) {
        errno = EHOSTUNREACH;
        return -1;
    }

    // Take the first preferred result
    const struct addrinfo *res = ai;

    memset(hp, 0, sizeof(*hp));
    hp->family = res->ai_family;
    hp->salen = (socklen_t) res->ai_addrlen;
    memcpy(&hp->sa, res->ai_addr, res->ai_addrlen);

    // Numeric address (fast, no DNS)
    fill_addrstr((const struct sockaddr *) &hp->sa, hp->salen, hp->addr,
                 sizeof(hp->addr));

    /* Canonical name if resolver provided it; otherwise use the input
     * POSIX:
     * If the AI_CANONNAME flag is specified, and the name service provides
     * a canonical name for the host, the ai_canonname member of the first
     * addrinfo structure shall point to it. Otherwise, the ai_canonname member
     * shall be a NULL pointer.
     */
    if (res->ai_canonname && *res->ai_canonname) {
        strncpy(hp->name, res->ai_canonname, sizeof(hp->name) - 1);
    } else {
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

    memset(hp, 0, sizeof(struct ll_host));

    // Detect family by parse
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

    // reverse DNS
    strncpy(hp->addr, ip, sizeof(hp->addr) - 1);
    try_reverse_dns((const struct sockaddr *) &hp->sa, hp->salen, hp->name,
                    sizeof(hp->name));
    return 0;
}

/* generic: works for AF_INET and AF_INET6 */
int get_host_by_sockaddr(const struct sockaddr *from, socklen_t fromlen,
                         struct ll_host *hp)
{
    if (!from || !hp) {
        errno = EINVAL;
        return -1;
    }

    // empty the host name
    memset(hp, 0, sizeof(*hp));

    switch (from->sa_family) {
    case AF_INET: {
        if (fromlen < sizeof(struct sockaddr_in)) {
            errno = EINVAL;
            return -1;
        }
        const struct sockaddr_in *sin = (const struct sockaddr_in *) from;
        memcpy(&hp->sa, sin, sizeof(*sin));
        hp->family = AF_INET;
        hp->salen = sizeof(struct sockaddr_in);

        if (!inet_ntop(AF_INET, &sin->sin_addr, hp->addr, sizeof(hp->addr))) {
            hp->addr[0] = '\0';
            return -1;
        }
        break;
    }
    case AF_INET6: {
        if (fromlen < sizeof(struct sockaddr_in6)) {
            errno = EINVAL;
            return -1;
        }
        const struct sockaddr_in6 *sin6 = (const struct sockaddr_in6 *) from;
        memcpy(&hp->sa, sin6, sizeof(*sin6));
        hp->family = AF_INET6;
        hp->salen = sizeof(struct sockaddr_in6);

        if (!inet_ntop(AF_INET6, &sin6->sin6_addr, hp->addr,
                       sizeof(hp->addr))) {
            hp->addr[0] = '\0';
            return -1;
        }
        break;
    }
    default:
        errno = EAFNOSUPPORT;
        return -1;
    }

    // reverse DNS, if fail the errno is set in the call
    // and the host name is left empty
    return try_reverse_dns((const struct sockaddr *) &hp->sa, hp->salen,
                           hp->name, sizeof(hp->name));
}

char *sockAdd2Str_(struct sockaddr_in *from)
{
    /* "255.255.255.255" (15) + ":" (1) + "65535" (5) + NUL = 22 max.
     * Give a little headroom. Thread-local to avoid races.
     */
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

    const struct sockaddr_in *src = (const struct sockaddr_in *) &hp->sa;
    memcpy(out, src, sizeof(*out));

    return 0;
}

int get_host_sinaddrv4(const struct ll_host *hp, struct sockaddr_in *dst)
{
    if (!hp || !dst)
        return -1;

    // Preserve dst->sin_port, sin_family, zero, etc.
    struct sockaddr_in *sin = (struct sockaddr_in *) &hp->sa;
    dst->sin_addr = sin->sin_addr;

    return 0;
}

int is_addrv4_zero(const struct sockaddr_in *addr)
{
    if (!addr)
        return 0;

    return addr->sin_addr.s_addr == 0;
}

int is_addrv4_equal(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    if (!a || !b)
        return -1;

    return a->sin_addr.s_addr == b->sin_addr.s_addr;
}

int is_addrv6_zero(const struct sockaddr_in6 *addr)
{
    if (!addr)
        return 0;
    return IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr);
}

int is_addrv6_equal(const struct sockaddr_in6 *a, const struct sockaddr_in6 *b)
{
    if (!a || !b)
        return -1;
    return memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(struct in6_addr)) == 0;
}

// Handy wrappers
int get_host_by_sockaddr_in(const struct sockaddr_in *from, struct ll_host *hp)
{
    return get_host_by_sockaddr((const struct sockaddr *) from, sizeof(*from),
                                hp);
}

int get_host_by_sockaddr_in6(const struct sockaddr_in6 *from,
                             struct ll_host *hp)
{
    return get_host_by_sockaddr((const struct sockaddr *) from, sizeof(*from),
                                hp);
}

// Compatibility API
char *ls_getmyhostname(void)
{
    static __thread char host_name[LL_HOSTNAME_MAX];

    // micro micro optimization to stau in user space
    if (host_name[0] == 0)
        gethostname(host_name, sizeof(host_name));

    return host_name;
}

int equal_host(const char *host, const char *host0)
{
    int cc = strcmp(host, host0);
    if (cc == 0)
        return true;
    return false;
}

int is_valid_host(const char *name)
{
    struct ll_host hp;
    int cc = get_host_by_name(name, &hp);
    if (cc < 0)
        return 0;
    return 1;
}
