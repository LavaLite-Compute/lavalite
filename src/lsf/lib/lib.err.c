/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.h"

__thread int lserrno = LSE_NO_ERR;
int masterLimDown = false;

const char *ls_errmsg[] = {
    [LSE_NO_ERR] = "No error",
    [LSE_BAD_XDR] = "XDR operation error",
    [LSE_MSG_SYS] = "Failed in sending/receiving a message",
    [LSE_BAD_ARGS] = "Bad arguments",
    [LSE_MASTR_UNKNW] = "Cannot locate master LIM now, try later",
    [LSE_LIM_DOWN] = "LIM is down; try later",
    [LSE_PROTOC_LIM] = "LIM protocol error",
    [LSE_SOCK_SYS] = "A socket operation has failed",
    [LSE_ACCEPT_SYS] = "Failed in an accept system call",
    [LSE_NO_HOST] = "Not enough host(s) currently eligible",
    [LSE_NO_ELHOST] = "No host is eligible",
    [LSE_TIME_OUT] = "Communication time out",
    [LSE_NIOS_DOWN] = "Nios has not been started",
    [LSE_LIM_DENIED] = "Operation permission denied by LIM",
    [LSE_LIM_IGNORE] = "Operation ignored by LIM",
    [LSE_LIM_BADHOST] = "Host name not recognizable by LIM",
    [LSE_LIM_ALOCKED] = "Host already locked",
    [LSE_LIM_NLOCKED] = "Host was not locked",
    [LSE_LIM_BADMOD] = "Unknown host model",
    [LSE_SIG_SYS] = "A signal related system call failed",
    [LSE_BAD_EXP] = "Bad resource requirement syntax",
    [LSE_NORCHILD] = "No remote child",
    [LSE_MALLOC] = "Memory allocation failed",
    [LSE_LSFCONF] = "Unable to open file lsf.conf",
    [LSE_BAD_ENV] = "Bad configuration environment, something missing in lsf.conf?",
    [LSE_LIM_NREG] = "LIM is not a registered service",
    [LSE_RES_NREG] = "RES is not a registered service",
    [LSE_RES_NOMORECONN] = "RES is serving too many connections",
    [LSE_BADUSER] = "Bad user ID",
    [LSE_BAD_OPCODE] = "Bad operation code",
    [LSE_PROTOC_RES] = "Protocol error with RES",
    [LSE_NOMORE_SOCK] = "Running out of privileged socks",
    [LSE_LOSTCON] = "Connection is lost",
    [LSE_BAD_HOST] = "Bad host name",
    [LSE_WAIT_SYS] = "A wait system call failed",
    [LSE_SETPARAM] = "Bad parameters for setstdin",
    [LSE_BAD_CLUSTER] = "Invalid cluster name",
    [LSE_EXECV_SYS] = "Failed in a execv() system call",
    [LSE_BAD_SERVID] = "Invalid service Id",
    [LSE_NLSF_HOST] = "Request from a non-LSF host rejected",
    [LSE_UNKWN_RESNAME] = "Unknown resource name",
    [LSE_UNKWN_RESVALUE] = "Unknown resource value",
    [LSE_TASKEXIST] = "Task already exists",
    [LSE_LIMIT_SYS] = "A resource limit system call failed",
    [LSE_BAD_NAMELIST] = "Bad index name list",
    [LSE_LIM_NOMEM] = "LIM malloc failed",
    [LSE_CONF_SYNTAX] = "Bad syntax in lsf.conf",
    [LSE_FILE_SYS] = "File operation failed",
    [LSE_CONN_SYS] = "A connect sys call failed",
    [LSE_SELECT_SYS] = "A select system call failed",
    [LSE_EOF] = "End of file",
    [LSE_ACCT_FORMAT] = "Bad lsf accounting record format",
    [LSE_BAD_TIME] = "Bad time specification",
    [LSE_FORK] = "Unable to fork child",
    [LSE_PIPE] = "Failed to setup pipe",
    [LSE_ESUB] = "Unable to access esub/eexec file",
    [LSE_EAUTH] = "External authentication failed",
    [LSE_NO_FILE] = "Cannot open file",
    [LSE_NO_CHAN] = "Out of communication channels",
    [LSE_BAD_CHAN] = "Bad communication channel",
    [LSE_INTERNAL] = "Internal library error",
    [LSE_PROTOCOL] = "Protocol error with server",
    [LSE_RES_RUSAGE] = "Failed to get rusage",
    [LSE_NO_RESOURCE] = "No shared resources",
    [LSE_BAD_RESOURCE] = "Bad resource name",
    [LSE_RES_PARENT] = "Failed to contact RES parent",
    [LSE_NO_MEM] = "Cannot allocate memory",
    [LSE_FILE_CLOSE] = "Close a NULL-FILE pointer",
    [LSE_LIMCONF_NOTREADY] = "Slave LIM configuration is not ready yet",
    [LSE_MASTER_LIM_DOWN] = "Master LIM is down; try later",
    [LSE_POLL_SYS] = "A poll system call failed",
};

_Static_assert(sizeof(ls_errmsg) / sizeof(ls_errmsg[0]) == LSE_NERR,
               "ls_errmsg array size must match LSE_NERR");

void ls_errlog(FILE *fp, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    verrlog_(-1, fp, fmt, ap);
    va_end(ap);
}

const char *err_str_(int errnum, const char *fmt, char *buf)
{
    const char *b;
    char *f;

    b = strstr(fmt, "%M");
    if (b) {
        strncpy(buf, fmt, b - fmt);
        strcpy(buf + (b - fmt), ls_sysmsg());
        strcat(buf + (b - fmt), b + 2);
        return buf;
    } else if (((b = strstr(fmt, "%m")) != NULL) ||
               ((b = strstr(fmt, "%k")) != NULL)) {
        strncpy(buf, fmt, b - fmt);
        f = buf + (b - fmt);
        if (strerror(errnum) == NULL)
            (void) sprintf(f, "error %d", errnum);
        else
            strcpy(f, strerror(errnum));

        f += strlen(f);
        strcat(f, b + 2);
        return buf;
    } else
        return fmt;
}

void verrlog_(int level, FILE *fp, const char *fmt, va_list ap)
{
    static char lastmsg[16384];
    static int count;
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
            fprintf(fp, ("Last message repeated %d times\n"), count);
        }
        fprintf(fp, "%s %d ", ctime2(&now) + 4, getpid());
    }

    if (level >= 0) {
        snprintf(verBuf, sizeof(verBuf), "%d %s %s", level,
                 LAVALITE_VERSION_STR, buf);
    } else {
        snprintf(verBuf, sizeof(verBuf), "%s %s", LAVALITE_VERSION_STR, buf);
    }

    fputs(verBuf, fp);
    putc('\n', fp);
    fflush(fp);
    strcpy(lastmsg, buf);
    count = 0;
    lastime = now;
}

// Bug remove it
const char *ls_sysmsg(void)
{
    static __thread char buf[256];

    if (lserrno >= LSE_NERR || lserrno < 0) {
        sprintf(buf, "Error %d", lserrno);
        return buf;
    }

    return ls_errmsg[lserrno];
}

void ls_perror(const char *usrMsg)
{
    if (usrMsg) {
        fputs(usrMsg, stderr);
        fputs(": ", stderr);
    }
    fputs(ls_sysmsg(), stderr);
    putc('\n', stderr);
}
