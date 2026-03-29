// Copyright (C) 2007 Platform Computing Inc
// Copyright (C) 2024-2025 LavaLite Contributors
// GPL v2

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <netdb.h>
#include <time.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.syslog.h"

static int log_fd = -1;
static int log_min_level;
static char log_ident[LL_BUFSIZ_32];
static char log_path[PATH_MAX];

static const char *level_str(int);
static int get_level_str(const char *);
static void build_timestamp(char *, size_t);
static void write_record(int fd, const char *, size_t);
static void ls_reopen_log(void);

// tag to indicate if child or parent useful after we fork
// and use the same log file
static char log_tag[LL_BUFSIZ_64];

int ls_openlog(const char *ident, const char *logdir, const char *mask)
{

    if (ident == NULL)
        return -1;

    if (logdir == NULL || *logdir == 0)
        return -1;

    snprintf(log_ident, sizeof(log_ident), "%s", ident);

    char host[MAXHOSTNAMELEN];
    if (gethostname(host, MAXHOSTNAMELEN) < 0)
        strcat(host, "unknown");

    log_min_level = get_level_str(mask);

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.log.%s", logdir, log_ident, host);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0) {
        fd = STDERR_FILENO;
        return -1;
    }
    /*
     * Normalize descriptor:
     * guarantee fd >= 3
     */
    int norm = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    close(fd);

    if (norm < 0)
        return -1;

    log_fd = norm;

    fchmod(log_fd, 0644);
    snprintf(log_path, sizeof(log_path), "%s", path);

    return 0;
}

void ls_syslog(int level, const char *fmt, ...)
{
    va_list ap;
    char msg[LL_BUFSIZ_1K];
    char line[LL_BUFSIZ_4K];
    int n;

    // no logger configured return, sorry no implicit
    // logging to stderr but definite behaviour
    // ls_openlog()/ls_syslog()/ls_closelog()
    if (log_fd < 0)
        return;

    // ignore messages above current mask
    if (level > log_min_level)
        return;

    // format message (%m handled by glibc)
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    // build timestamp
    char ts[LL_BUFSIZ_64];
    build_timestamp(ts, sizeof(ts));

    /* final formatted line:
     * "Dec 19 16:13:16 2025 [LOG_INFO] tag pid message text"
     */

    if (log_tag[0] != '\0') {
        n = snprintf(line, sizeof(line),
                     "%s [%s] %s %d %s\n",
                     ts,
                     level_str(level),
                     log_tag,
                     getpid(),
                     msg);
    } else {
        n = snprintf(line, sizeof(line),
                     "%s [%s] %d %s\n",
                     ts,
                     level_str(level),
                     getpid(),
                     msg);
    }
    if (n < 0)
        return;

    size_t len = (size_t)n;
    if (len >= sizeof(line))
        len = sizeof(line) - 1;

    if (log_fd != STDERR_FILENO) {
        struct stat st;
        if (fstat(log_fd, &st) == 0 && st.st_nlink == 0)
            ls_reopen_log();

        write_record(log_fd, line, len);
    }
    // mirror to stderr if tty
    if (log_fd != STDERR_FILENO && isatty(STDERR_FILENO))
        write(STDERR_FILENO, line, len);
}

static void ls_reopen_log(void)
{
    if (log_path[0] == 0)
        return;

    int newfd = open(log_path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (newfd < 0)
        return;

    if (log_fd >= 0)
        close(log_fd);

    log_fd = newfd;
    fchmod(log_fd, 0644);
}


void ls_closelog(void)
{
    if (log_fd >= 0) {
        close(log_fd);
        log_fd = -1;
    }
}

static int get_level_str(const char *name)
{
    if (name == NULL) {
        return LOG_INFO;
    }

    if (strcmp(name, "LOG_EMERG") == 0) {
        return LOG_EMERG;
    }
    if (strcmp(name, "LOG_ALERT") == 0) {
        return LOG_ALERT;
    }
    if (strcmp(name, "LOG_CRIT") == 0) {
        return LOG_CRIT;
    }
    if (strcmp(name, "LOG_ERR") == 0) {
        return LOG_ERR;
    }
    if (strcmp(name, "LOG_WARNING") == 0) {
        return LOG_WARNING;
    }
    if (strcmp(name, "LOG_NOTICE") == 0) {
        return LOG_NOTICE;
    }
    if (strcmp(name, "LOG_INFO") == 0) {
        return LOG_INFO;
    }
    if (strcmp(name, "LOG_DEBUG") == 0) {
        return LOG_DEBUG;
    }

    return LOG_INFO;
}

static void build_timestamp(char *buf, size_t bufsz)
{
    struct timespec ts;
    struct tm tm_buf;

    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
        buf[0] = 0;
        return;
    }

    if (localtime_r(&ts.tv_sec, &tm_buf) == NULL) {
        buf[0] = 0;
        return;
    }
    //* "Dec 19 16:13:16.123"
    snprintf(buf, bufsz,
             "%.3s %2d %02d:%02d:%02d.%03ld",
             "JanFebMarAprMayJunJulAugSepOctNovDec" + tm_buf.tm_mon * 3,
             tm_buf.tm_mday,
             tm_buf.tm_hour,
             tm_buf.tm_min,
             tm_buf.tm_sec,
             ts.tv_nsec / 1000000);
}

/* Map syslog level value -> string name.
 * This is the inverse of get_level_str().
 */
static const char *level_str(int level)
{
    if (level == LOG_EMERG) {
        return "LOG_EMERG";
    }

    if (level == LOG_ALERT) {
        return "LOG_ALERT";
    }

    if (level == LOG_CRIT) {
        return "LOG_CRIT";
    }

    if (level == LOG_ERR) {
        return "LOG_ERR";
    }

    if (level == LOG_WARNING) {
        return "LOG_WARNING";
    }

    if (level == LOG_NOTICE) {
        return "LOG_NOTICE";
    }

    if (level == LOG_INFO) {
        return "LOG_INFO";
    }

    if (level == LOG_DEBUG) {
        return "LOG_DEBUG";
    }

    // Fallback: be conservative and return INFO if we see an unknown level.
    return "LOG_INFO";
}

void ls_setlogtag(const char *tag)
{
    if (!tag || !*tag) {
        log_tag[0] = 0;
        return;
    }

    snprintf(log_tag, sizeof(log_tag), "%s", tag);
}

static void write_record(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        buf += (size_t)n;
        len -= (size_t)n;
    }
}
