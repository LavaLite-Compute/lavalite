/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include <stdio.h>
#include <syslog.h>

int ll_openlog(const char *, const char *, const char *);
int ls_getlogfd(void);
void ll_syslog(int, const char *, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void ls_setlogtag(const char *);
void ll_closelog(void);

#define LL_ERRX(fmt, ...)                                                      \
    ll_syslog(LOG_ERR, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LL_ERR(fmt, ...)                                                       \
    ll_syslog(LOG_ERR, "%s: " fmt ": %m", __func__, ##__VA_ARGS__)

#define LL_WARNING(fmt, ...)                                                   \
    ll_syslog(LOG_WARNING, "%s: " fmt ": %m", __func__, ##__VA_ARGS__)

#define LL_WARNINGX(fmt, ...)                                                  \
    ll_syslog(LOG_WARNING, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LL_INFO(fmt, ...)                                                      \
    ll_syslog(LOG_INFO, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LL_DEBUG(fmt, ...)                                                     \
    ll_syslog(LOG_DEBUG, "%s: " fmt, __func__, ##__VA_ARGS__)
