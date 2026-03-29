/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

int ls_openlog(const char *, const char *, const char *);
int ls_getlogfd(void);
void ls_syslog(int, const char *, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;
void ls_setlogtag(const char *);
void ls_closelog(void);

#define LS_ERRX(fmt, ...) \
    ls_syslog(LOG_ERR, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LS_ERR(fmt, ...) \
    ls_syslog(LOG_ERR, "%s: " fmt " %m", __func__, ##__VA_ARGS__)

#define LS_WARNING(fmt, ...) \
    ls_syslog(LOG_WARNING, "%s: " fmt " %m", __func__, ##__VA_ARGS__)

#define LS_WARNINGX(fmt, ...)                                            \
    ls_syslog(LOG_WARNING, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LS_INFO(fmt, ...) \
    ls_syslog(LOG_INFO, "%s: " fmt, __func__, ##__VA_ARGS__)

#define LS_DEBUG(fmt, ...) \
    ls_syslog(LOG_DEBUG, "%s: " fmt, __func__, ##__VA_ARGS__)
