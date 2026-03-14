/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

// Automake generated
#include "config.h"

/* System headers needed by multiple intlib .c files
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <linux/limits.h>
#include <netdb.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <sys/timerfd.h>
#include <pwd.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/utsname.h>
#include <assert.h>

#include "include/ll.h"

#ifndef MAX
#define MAX(a, b) \
    ({ \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a > _b ? _a : _b;  \
    })
#endif

#ifndef MIN
#define MIN(a, b) \
    ({ \
        typeof(a) _a = (a); \
        typeof(b) _b = (b); \
        _a < _b ? _a : _b;  \
    })
#endif

enum {
    LL_BUFSIZ_32 = 32,
    LL_BUFSIZ_64 = 64,
    LL_BUFSIZ_256 = 256,
    LL_BUFSIZ_1K = 1024,
    LL_BUFSIZ_4K = 4096,
};

#define TIMEIT(level, func, name)                                              \
    {                                                                          \
        if (timinglevel > level) {                                             \
            struct timeval before, after;                                      \
            struct timezone tz;                                                \
            gettimeofday(&before, &tz);                                        \
            func;                                                              \
            gettimeofday(&after, &tz);                                         \
            ls_syslog(LOG_INFO, "L%d %s %d ms", level, name,                   \
                      (int) ((after.tv_sec - before.tv_sec) * 1000 +           \
                             (after.tv_usec - before.tv_usec) / 1000));        \
        } else                                                                 \
            func;                                                              \
    }

#define TIMEVAL(level, func, val)                                              \
    {                                                                          \
        if (timinglevel > level) {                                             \
            struct timeval before, after;                                      \
            struct timezone tz;                                                \
            gettimeofday(&before, &tz);                                        \
            func;                                                              \
            gettimeofday(&after, &tz);                                         \
            val = (int) ((after.tv_sec - before.tv_sec) * 1000 +               \
                         (after.tv_usec - before.tv_usec) / 1000);             \
        } else {                                                               \
            func;                                                              \
            val = 0;                                                           \
        }                                                                      \
    }

/* Maximum size of a single environment variable stored in job spec.
* 2 MiB is practically unlimited for real systems (EDA/Lmod etc.) while
* still preventing pathological allocations caused by corrupt jobfiles.
* This matches ARG_MAX ~= 2MB on Rocky9/Ubuntu24.x.
*/
static const size_t LL_ENVVAR_MAX = 2 * 1024 * 1024;

/* Utility macro for kibibytes
 */
#define LL_KiB(n) ((size_t) (n) * 1024)

int get_uid(const char *, uid_t *);
int millisleep(uint32_t);
size_t ll_strlcpy(char *, const char *, size_t);
int ll_atoi(const char *, int *);
int ll_atoll(const char *, int64_t *);
const char *ctime2(time_t *);
int rd_poll(int, int);
struct passwd *getpwnam2(const char *);
struct passwd *getpwuid2(uid_t);
