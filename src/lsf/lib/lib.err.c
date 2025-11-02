/* $Id: lib.err.c,v 1.9 2007/08/15 22:18:50 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/lib.h"

int    lserrno = LSE_NO_ERR;
int    masterLimDown = false;
int    ls_nerr = LSE_NERR;

char   *ls_errmsg[] = {
    /* 0 */       "Error 0",
    /* 1 */       "XDR operation error",
    /* 2 */       "Failed in sending/receiving a message",
    /* 3 */       "Bad arguments",
    /* 4 */       "Cannot locate master LIM now, try later",
    /* 5 */       "LIM is down; try later",
    /* 6 */       "LIM protocol error",
    /* 7 */       "A socket operation has failed",
    /* 8 */       "Failed in an accept system call",
    /* 9 */       "Bad LSF task configuration file format",
    /* 10 */      "Not enough host(s) currently eligible",
    /* 11 */      "No host is eligible",
    /* 12 */      "Communication time out",
    /* 13 */      "Nios has not been started",
    /* 14 */      "Operation permission denied by LIM",
    /* 15 */      "Operation ignored by LIM",
    /* 16 */      "Host name not recognizable by LIM",
    /* 17 */      "Host already locked",
    /* 18 */      "Host was not locked",
    /* 19 */      "Unknown host model",
    /* 20 */      "A signal related system call failed",
    /* 21 */      "Bad resource requirement syntax",
    /* 22 */      "No remote child",
    /* 23 */      "Memory allocation failed",
    /* 24 */      "Unable to open file lsf.conf",
    /* 25 */      "Bad configuration environment, something missing in lsf.conf?",
    /* 26 */      "Lim is not a registered service",
    /* 27 */      "Res is not a registered service",
    /* 28 */      "RES is serving too many connections",
    /* 29 */      "Bad user ID",
    /* 30 */      "Root user rejected",
    /* 31 */      "User permission denied",
    /* 32 */      "Bad operation code",
    /* 33 */      "Protocol error with RES",
    /* 34 */      "RES callback fails; see RES error log for more details",
    /* 35 */      "RES malloc fails",
    /* 36 */      "Fatal error in RES; check RES error log for more details",
    /* 37 */      "RES cannot alloc pty",
    /* 38 */      "RES cannot allocate socketpair as stdin/stdout/stderr for task",
    /* 39 */      "RES fork fails",
    /* 40 */      "Running out of privileged socks",
    /* 41 */      "getwd failed",
    /* 42 */      "Connection is lost",
    /* 43 */      "No such remote child",
    /* 44 */      "Permission denied",
    /* 45 */      "Ptymode inconsistency on ls_rtask",
    /* 46 */      "Bad host name",
    /* 47 */      "NIOS protocol error",
    /* 48 */      "A wait system call failed",
    /* 49 */      "Bad parameters for setstdin",
    /* 50 */      "Insufficient list length for returned rpids",
    /* 51 */      "Invalid cluster name",
    /* 52 */      "Incompatible versions of tty params",
    /* 53 */      "Failed in a execv() system call",
    /* 54 */      "No such directory",
    /* 55 */      "Directory may not be accessible",
    /* 56 */      "Invalid service Id",
    /* 57 */      "Request from a non-LSF host rejected",
    /* 58 */      "Unknown resource name",
    /* 59 */      "Unknown resource value",
    /* 60 */      "Task already exists",
    /* 61 */      "Task does not exist",
    /* 62 */      "Task table is full",
    /* 63 */      "A resource limit system call failed",
    /* 64 */      "Bad index name list",
    /* 65 */      "LIM malloc failed",
    /* 66 */      "NIO not initialized",
    /* 67 */      "Bad syntax in lsf.conf",
    /* 68 */      "File operation failed",
    /* 69 */      "A connect sys call failed",
    /* 70 */      "A select system call failed",
    /* 71 */      "End of file",
    /* 72 */      "Bad lsf accounting record format",
    /* 73 */      "Bad time specification",
    /* 74 */      "Unable to fork child",
    /* 75 */      "Failed to setup pipe",
    /* 76 */      "Unable to access esub/eexec file",
    /* 77 */      "External authentication failed",
    /* 78 */      "Cannot open file",
    /* 79 */      "Out of communication channels",
    /* 80 */      "Bad communication channel",
    /* 81 */      "Internal library error",
    /* 82 */      "Protocol error with server",
    /* 83 */      "A system call failed",
    /* 84 */      "Failed to get rusage",
    /* 85 */      "No shared resources",
    /* 86 */      "Bad resource name",
    /* 87 */      "Failed to contact RES parent",
    /* 88 */      "i18n setlocale failed",
    /* 89 */      "i18n catopen failed",
    /* 90 */      "i18n malloc failed",
    /* 91 */      "Cannot allocate memory",
    /* 92 */      "Close a NULL-FILE pointer",
    /* 93 */      "Slave LIM configuration is not ready yet",
    /* 94 */       "Master LIM is down; try later",
    /* 95 */       "Requested label is not valid",
    /* 96 */       "Requested label is above your allowed range",
    /* 97 */       "Request label rejected by /etc/rhost.conf",
    /* 98 */       "Request label doesn't dominate current label",
};

void
ls_errlog(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    verrlog_(-1, fp, fmt, ap);
    va_end(ap);
}

const char *
err_str_(int errnum, const char *fmt, char *buf)
{
    const char *b;
    char *f;

    b = strstr(fmt, "%M");
    if (b)
    {
        strncpy(buf, fmt, b - fmt);
        strcpy(buf + (b - fmt), ls_sysmsg());
        strcat(buf + (b - fmt), b + 2);
        return buf;
    }
    else if (((b = strstr(fmt, "%m")) != NULL) ||
            ((b = strstr(fmt, "%k")) != NULL))
    {
        strncpy(buf, fmt, b - fmt);
        f = buf + (b - fmt);
        if (strerror(errnum) == NULL)
            (void)sprintf(f, "error %d", errnum);
        else
            strcpy(f, strerror(errnum));

        f += strlen(f);
        strcat(f, b + 2);
        return buf;
    }
    else
        return fmt;
}

void
verrlog_(int level, FILE *fp, const char *fmt, va_list ap)
{
    static char lastmsg[16384];
    static int  count;
    static time_t lastime, lastcall;
    time_t now;
    static char buf[LL_BUFSIZ_1K];
    static char tmpbuf[LL_BUFSIZ_1K];
    static char verBuf[LL_BUFSIZ_2K];
    int save_errno = errno;

    memset(buf, 0, sizeof(buf));
    memset(tmpbuf, 0, sizeof(tmpbuf));
    memset(verBuf, 0, sizeof(verBuf));

    vsprintf(buf, err_str_(save_errno, fmt, tmpbuf), ap);

    now = time(0);
    if (lastmsg[0] && (strcmp(buf, lastmsg) == 0) && (now - lastime < 600)) {
        count++;
        lastcall = now;
        return;
    } else {
        if (count) {
            fprintf(fp, "%s %d ", ctime2(&lastcall) + 4, getpid());
            fprintf(fp,("Last message repeated %d times\n"), count);
        }
        fprintf(fp, "%s %d ", ctime2(&now) + 4, getpid());
    }

    if (level >= 0) {
        snprintf(verBuf, sizeof(verBuf), "%d %s %s",
                 level, LAVALITE_VERSION_STR, buf);
    } else {
        snprintf(verBuf, sizeof(verBuf), "%s %s",
                 LAVALITE_VERSION_STR, buf);
    }

    fputs(verBuf, fp);
    putc('\n', fp);
    fflush(fp);
    strcpy(lastmsg, buf);
    count = 0;
    lastime = now;
}

char *
ls_sysmsg(void)
{
    static char buf[256];
    int save_errno = errno;

    if (lserrno >= ls_nerr || lserrno < 0) {
        sprintf(buf, "Error %d", lserrno);
        return buf;
    }

    if (LSE_SYSCALL(lserrno)) {
        if (strerror(save_errno) != NULL && save_errno > 0)
            sprintf(buf, "%s: %s", ls_errmsg[lserrno], strerror(save_errno));
        else
            sprintf(buf, ("%s: unknown system error %d"), ls_errmsg[lserrno], save_errno);
        return buf;
    }

    if ((lserrno == LSE_LIM_DOWN) && (masterLimDown == true)) {
        return(ls_errmsg[LSE_MASTER_LIM_DOWN]);
    }

    return(ls_errmsg[lserrno]);

}

void
ls_perror(char *usrMsg)
{
    if (usrMsg) {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }
    fputs(ls_sysmsg(), stderr);
    putc('\n', stderr);
}

__thread ll_err_t ll_errno = LL_OK;

// Messages aligned with ll_err_t order above
static const char *const ll_errmsg[] = {
    [LL_OK]               = "ok",
    [LL_EINVAL]           = "invalid argument",
    [LL_ENOENTQ]          = "queue not found",
    [LL_ENOENTH]          = "host not found",
    [LL_EPERM_POLICY]     = "rejected by policy",
    [LL_EJOB_NOTFOUND]    = "job not found",
    [LL_EJOB_ARRAY]       = "invalid job array",
    [LL_ERES_BADREQ]      = "invalid resource request",
    [LL_ERES_UNAVAILABLE] = "resources unavailable",
    [LL_ESYSCALL]         = "a system call failed (check ll_syserr())"
};

void
ll_seterr(ll_err_t code)
{
    ll_errno = code;
}
ll_err_t
ll_geterr(void)
{
    return ll_errno;
}

// compile-time sanity check: array size must match enum count
static_assert(sizeof(ll_errmsg)/sizeof(ll_errmsg[0]) == LL_ERR_COUNT,
              "ll_errmsg table out of sync with ll_err_t");

const char *ll_errstr(ll_err_t code)
{
    if (code >= 0 && code < LL_ERR_COUNT)
        return ll_errmsg[code];

    return "unknown error";
}
