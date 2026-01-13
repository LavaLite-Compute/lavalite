// lib.syslog.c - simple deterministic logging for LavaLite daemons
//  Copyright (C) 2024-2025 LavaLite Contributors
//
// Each daemon logs to: <logdir>/<ident>.log.<hostname>
// Optionally also logs to stderr and/or syslog(), but never does
// any dynamic fallback or reopen magic.

#include "lsf/lib/ll.sys.h"
#include "lsf/lib/ll.bufsiz.h"

static int log_fd = -1;
static int log_to_stderr;
static int log_to_syslog;
static int log_min_level;
static char log_ident[LL_BUFSIZ_32];
static char log_path[PATH_MAX];

static const char *level_str(int);
static int get_level_str(const char *);
static void build_timestamp(char *, size_t);
static void write_record(int fd, const char *, size_t);
static void ls_reopen_log(void);

__thread int lserrno = LSE_NO_ERR;
// Just to link...
int logclass = 0;
// timing level is important fro the TIMEIT macros
int timinglevel = 0;

const char *ls_errmsg[] = {
    [LSE_NO_ERR]            = "No error",
    [LSE_BAD_XDR]           = "XDR operation error",
    [LSE_MSG_SYS]           = "Failed in sending/receiving a message",
    [LSE_BAD_ARGS]          = "Bad arguments",
    [LSE_MASTR_UNKNW]       = "Cannot locate master LIM now, try later",
    [LSE_LIM_DOWN]          = "LIM is down; try later",
    [LSE_PROTOC_LIM]        = "LIM protocol error",
    [LSE_SOCK_SYS]          = "A socket operation has failed",
    [LSE_ACCEPT_SYS]        = "Failed in an accept system call",
    [LSE_NO_HOST]           = "Not enough host(s) currently eligible",
    [LSE_NO_ELHOST]         = "No host is eligible",
    [LSE_TIME_OUT]          = "Communication time out",
    [LSE_NIOS_DOWN]         = "Nios has not been started",
    [LSE_LIM_DENIED]        = "Operation permission denied by LIM",
    [LSE_LIM_IGNORE]        = "Operation ignored by LIM",
    [LSE_LIM_BADHOST]       = "Host name not recognizable by LIM",
    [LSE_LIM_ALOCKED]       = "Host already locked",
    [LSE_LIM_NLOCKED]       = "Host was not locked",
    [LSE_LIM_BADMOD]        = "Unknown host model",
    [LSE_SIG_SYS]           = "A signal related system call failed",
    [LSE_BAD_EXP]           = "Bad resource requirement syntax",
    [LSE_NORCHILD]          = "No remote child",
    [LSE_MALLOC]            = "Memory allocation failed",
    [LSE_LSFCONF]           = "Unable to open file lsf.conf",
    [LSE_BAD_ENV]           = "Bad configuration environment, something missing in lsf.conf?",
    [LSE_LIM_NREG]          = "LIM is not a registered service",
    [LSE_RES_NREG]          = "RES is not a registered service",
    [LSE_RES_NOMORECONN]    = "RES is serving too many connections",
    [LSE_BADUSER]           = "Bad user ID",
    [LSE_BAD_OPCODE]        = "Bad operation code",
    [LSE_PROTOC_RES]        = "Protocol error with RES",
    [LSE_NOMORE_SOCK]       = "Running out of privileged socks",
    [LSE_LOSTCON]           = "Connection is lost",
    [LSE_BAD_HOST]          = "Bad host name",
    [LSE_WAIT_SYS]          = "A wait system call failed",
    [LSE_SETPARAM]          = "Bad parameters for setstdin",
    [LSE_BAD_CLUSTER]       = "Invalid cluster name",
    [LSE_EXECV_SYS]         = "Failed in a execv() system call",
    [LSE_BAD_SERVID]        = "Invalid service Id",
    [LSE_NLSF_HOST]         = "Request from a non-LSF host rejected",
    [LSE_UNKWN_RESNAME]     = "Unknown resource name",
    [LSE_UNKWN_RESVALUE]    = "Unknown resource value",
    [LSE_TASKEXIST]         = "Task already exists",
    [LSE_LIMIT_SYS]         = "A resource limit system call failed",
    [LSE_BAD_NAMELIST]      = "Bad index name list",
    [LSE_LIM_NOMEM]         = "LIM malloc failed",
    [LSE_CONF_SYNTAX]       = "Bad syntax in lsf.conf",
    [LSE_FILE_SYS]          = "File operation failed",
    [LSE_CONN_SYS]          = "A connect sys call failed",
    [LSE_SELECT_SYS]        = "A select system call failed",
    [LSE_EOF]               = "End of file",
    [LSE_ACCT_FORMAT]       = "Bad lsf accounting record format",
    [LSE_BAD_TIME]          = "Bad time specification",
    [LSE_FORK]              = "Unable to fork child",
    [LSE_PIPE]              = "Failed to setup pipe",
    [LSE_ESUB]              = "Unable to access esub/eexec file",
    [LSE_EAUTH]             = "External authentication failed",
    [LSE_NO_FILE]           = "Cannot open file",
    [LSE_NO_CHAN]           = "Out of communication channels",
    [LSE_BAD_CHAN]          = "Bad communication channel",
    [LSE_INTERNAL]          = "Internal library error",
    [LSE_PROTOCOL]          = "Protocol error with server",
    [LSE_RES_RUSAGE]        = "Failed to get rusage",
    [LSE_NO_RESOURCE]       = "No shared resources",
    [LSE_BAD_RESOURCE]      = "Bad resource name",
    [LSE_RES_PARENT]        = "Failed to contact RES parent",
    [LSE_NO_MEM]            = "Cannot allocate memory",
    [LSE_FILE_CLOSE]        = "Close a NULL-FILE pointer",
    [LSE_LIMCONF_NOTREADY]  = "Slave LIM configuration is not ready yet",
    [LSE_MASTER_LIM_DOWN]   = "Master LIM is down; try later",
    [LSE_POLL_SYS]          = "A poll system call failed",
};

_Static_assert(sizeof(ls_errmsg) / sizeof(ls_errmsg[0]) == LSE_NERR,
               "ls_errmsg array size must match LSE_NERR");

// tag to indicate if child or parent useful after we fork
// and use the same log file
static char log_tag[LL_BUFSIZ_64];

int ls_openlog(const char *ident,
               const char *logdir,
               int to_stderr,
               int to_syslog,
               const char *mask)
{
    //1. resolve ident
    const char *name;
    if (ident && *ident)
        name = ident;
    else
        name = "lavalite";

    snprintf(log_ident, sizeof(log_ident), "%s", name);

    /* 2. resolve hostname now, so stderr/syslog have it too */
    const char *host = ls_getmyhostname();
    if (!host || !*host)
        host = "unknown";

    // 3. configure base behaviour
    log_min_level  = get_level_str(mask);
    log_to_stderr  = to_stderr ? 1 : 0;
    log_to_syslog  = to_syslog ? 1 : 0;
    log_fd         = -1;
    log_path[0] = 0;

    /* 4. optional syslog mirror, independent of file logging */
    if (log_to_syslog) {
        openlog(log_ident, LOG_NDELAY | LOG_PID, LOG_DAEMON);
        setlogmask(LOG_UPTO(log_min_level));
    }

    /* 5. LSF_LOGDIR missing → fallback only (stderr/syslog) */
    if (logdir == NULL || *logdir == '\0') {

        if (!log_to_stderr)
            log_to_stderr = 1;

        /* stderr still includes host in prefix via ls_syslog macros */
        dprintf(STDERR_FILENO,
                "%s: LSF_LOGDIR not set, logging to stderr only (host=%s)\n",
                log_ident, host);

        /* no file sink, but logging works */
        return 0;
    }

    char path[PATH_MAX];
    /* 6. normal file logging path */
    int cc = snprintf(path, sizeof(path), "%s/%s.log.%s",
                      logdir, log_ident, host);
    if (cc < 0 || cc >= (int)sizeof(path))
        return -1;

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd < 0)
        return -1;   /* caller can decide if this is fatal */

    log_fd = fd;
    snprintf(log_path, sizeof(log_path), "%s", path);

    return 0;
}

void ls_set_time_level(const char *time_value)
{
    if (!time_value || *time_value == '\0') {
        timinglevel = 0;
        return;
    }

    timinglevel = atoi(time_value);
    if (timinglevel < 0)
        timinglevel = 0;
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

    // when negative we still want to write to stderr or syslog
    // if they are configured
    if (log_fd >= 0) {
        struct stat st;
        if (fstat(log_fd, &st) == 0 && st.st_nlink == 0)
            ls_reopen_log();

        flock(log_fd, LOCK_EX);
        write_record(log_fd, line, len);
        flock(log_fd, LOCK_UN);
    }
    // mirror to stderr if requested
    if (log_to_stderr)
        write(STDERR_FILENO, line, len);

    // syslog mirror without our timestamp
    if (log_to_syslog)
        syslog(level, "%s", msg);
}

static void ls_reopen_log(void)
{
    if (log_path[0] == '\0')
        return;

    int newfd = open(log_path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
    if (newfd < 0)
        return;

    if (log_fd >= 0)
        close(log_fd);

    log_fd = newfd;
}


void ls_closelog(void)
{
    if (log_fd >= 0) {
        close(log_fd);
        log_fd = -1;
    }

    if (log_to_syslog) {
        closelog();
        log_to_syslog = 0;
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
        buf[0] = '\0';
        return;
    }

    if (localtime_r(&ts.tv_sec, &tm_buf) == NULL) {
        buf[0] = '\0';
        return;
    }
    /* "Dec 19 16:13:16.123" */
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


// Cambrian legacy....

/*
 * Thread-local library error string.
 * Used by ls_perror() and potentially by higher-level APIs.
 */
const char *ls_sysmsg(void)
{
    static __thread char buf[256];

    if (lserrno < 0 || lserrno >= LSE_NERR) {
        snprintf(buf, sizeof(buf), "Error %d", lserrno);
        return buf;
    }

    return ls_errmsg[lserrno];
}

void ls_perror(const char *usrMsg)
{
    if (usrMsg != NULL && *usrMsg != '\0') {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }

    fputs(ls_sysmsg(), stderr);
    fputc('\n', stderr);
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
