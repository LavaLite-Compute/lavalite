/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.sys.h"

int millisleep_(uint32_t ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;

    int ret;
    do {
        ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts);
    } while (ret == EINTR);

    return ret;
}

// ARM does not have strlcpy() and we dont want to use
// libbsd as another dependency
size_t ll_strlcpy(char *dst, const char *src, size_t size)
{
    size_t srclen;

    if (size == 0)
        return strlen(src);

    srclen = strlen(src);

    if (srclen < size) {
        memcpy(dst, src, srclen);
        dst[srclen] = 0;
        return srclen;
    }

    // truncated
    memcpy(dst, src, size - 1);
    dst[size - 1] = 0;
    return srclen;
}

int get_uid(const char *user, uid_t *uid)
{
    struct passwd *pwd;

    if (user == NULL)
        return -1;

    pwd = getpwnam2(user);
    if (pwd == NULL)
        return -1;

    *uid = pwd->pw_uid;

    return 0;
}

// yaai yet another atoi
int ll_atoi(const char *s, int *out)
{
    char *end;
    long v;

    if (!s || !out)
        return 0;

    // Skip leading whitespace explicitly so empty/space-only strings fail cleanly
    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return 0;

    errno = 0;
    v = strtol(s, &end, 10);

    if (end == s)
        return 0;

    // Allow trailing whitespace only; reject any other junk
    while (*end && isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return 0;

    if (errno == ERANGE || v < INT_MIN || v > INT_MAX)
        return 0;

    *out = (int)v;
    return 1;
}

int ll_atoll(const char *s, int64_t *out)
{
    char *end;
    long long v;

    if (!s || !out)
        return 0;

    while (*s && isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return 0;

    errno = 0;
    v = strtoll(s, &end, 10);

    if (end == s)
        return 0;

    while (*end && isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return 0;

    if (errno == ERANGE || v < (long long)INT64_MIN || v > (long long)INT64_MAX)
        return 0;

    *out = (int64_t)v;
    return 1;
}

const char *ctime2(time_t *tp)
{
    time_t t;
    static __thread char ctime2_buf[LL_BUFSIZ_64];

    if (!tp) {
        t = time(NULL);
        tp = &t;
    }
    struct tm tm;
    if (!localtime_r(tp, &tm)) {
        return "";
    }

    // "%a %b %e %T %Y" -> e.g., "Wed Jun  3 11:22:33 2020"
    if (strftime(ctime2_buf, sizeof(ctime2_buf), "%a %b %e %T %Y", &tm) == 0)
        return "";

    return ctime2_buf;
}

int rd_poll(int rd, int ms)
{
    struct pollfd pfd = {.fd = rd, .events = POLLIN};

    for (;;) {
        int cc = poll(&pfd, 1, ms);

        if (cc >= 0)
            return cc;

        if (errno == EINTR)
            continue;

        return -1;
    }
}
struct passwd *getpwuid2(uid_t uid)
{
    // 1K seams a lot but sysconf(_SC_GETPW_R_SIZE_MAX) returns it
    static __thread char buf[LL_BUFSIZ_1K];
    static __thread struct passwd pwd;
    struct passwd *res = NULL;

    int cc = getpwuid_r(uid, &pwd, buf, sizeof(buf), &res);
    if (cc != 0)
        return NULL;
    if (res)
        return res;

    return NULL;
}

struct passwd *getpwnam2(const char *name)
{
    // 1K seams a lot but sysconf(_SC_GETPW_R_SIZE_MAX) returns it
    static __thread char buf[LL_BUFSIZ_1K];
    static __thread struct passwd pwd;
    struct passwd *result = NULL;

    if (getpwnam_r(name, &pwd, buf, sizeof(buf), &result) == 0 &&
        result != NULL) {
        return result;
    }
    return NULL;
}
