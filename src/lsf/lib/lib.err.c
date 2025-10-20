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

#define NL_SETN   23
int    lserrno = LSE_NO_ERR;
int    masterLimDown = false;
int    ls_nerr = LSE_NERR;

#ifdef  I18N_COMPILE
static int ls_errmsg_ID[] = {
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
    130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
    140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
    150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
    160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
    170, 172, 173, 174, 175, 176, 177, 179,
    180, 181, 182, 183, 184, 186, 188, 189,
    190, 191, 195, 196, 197, 198,
    200, 201, 202, 203, 204, 205, 206
};
#endif

char   *ls_errmsg[] = {
    /* 0 */       "Error 0",                 /* catgets  100 */
    /* 1 */       "XDR operation error", /* catgets 101 */
    /* 2 */       "Failed in sending/receiving a message",  /* catgets 102 */
    /* 3 */       "Bad arguments",  /* catgets 103 */
    /* 4 */       "Cannot locate master LIM now, try later",  /* catgets 104 */
    /* 5 */       "LIM is down; try later", /* catgets 105 */
    /* 6 */       "LIM protocol error",  /* catgets 106 */
    /* 7 */       "A socket operation has failed", /* catgets 107 */
    /* 8 */       "Failed in an accept system call", /* catgets 108 */
    /* 9 */       "Bad LSF task configuration file format",  /* catgets 109 */
    /* 10 */      "Not enough host(s) currently eligible",  /* catgets 110 */
    /* 11 */      "No host is eligible", /* catgets 111 */
    /* 12 */      "Communication time out", /* catgets 112 */
    /* 13 */      "Nios has not been started",  /* catgets 113 */
    /* 14 */      "Operation permission denied by LIM", /* catgets 114 */
    /* 15 */      "Operation ignored by LIM",  /* catgets 115 */
    /* 16 */      "Host name not recognizable by LIM", /* catgets 116 */
    /* 17 */      "Host already locked",  /* catgets 117 */
    /* 18 */      "Host was not locked",  /* catgets 118 */
    /* 19 */      "Unknown host model",   /* catgets 119 */
    /* 20 */      "A signal related system call failed",  /* catgets 120 */
    /* 21 */      "Bad resource requirement syntax",  /* catgets 121 */
    /* 22 */      "No remote child",  /* catgets 122 */
    /* 23 */      "Memory allocation failed", /* catgets 123 */
    /* 24 */      "Unable to open file lsf.conf", /* catgets 124 */
    /* 25 */      "Bad configuration environment, something missing in lsf.conf?", /* catgets 125 */
    /* 26 */      "Lim is not a registered service", /* catgets 126 */
    /* 27 */      "Res is not a registered service", /* catgets 127 */
    /* 28 */      "RES is serving too many connections", /* catgets 128 */
    /* 29 */      "Bad user ID",  /* catgets 129 */
    /* 30 */      "Root user rejected", /* catgets 130 */
    /* 31 */      "User permission denied", /* catgets 131 */
    /* 32 */      "Bad operation code",  /* catgets 132 */
    /* 33 */      "Protocol error with RES", /* catgets 133 */
    /* 34 */      "RES callback fails; see RES error log for more details", /* catgets 134 */
    /* 35 */      "RES malloc fails", /* catgets 135 */
    /* 36 */      "Fatal error in RES; check RES error log for more details", /* catgets 136 */
    /* 37 */      "RES cannot alloc pty", /* catgets 137 */
    /* 38 */      "RES cannot allocate socketpair as stdin/stdout/stderr for task", /* catgets 138 */
    /* 39 */      "RES fork fails", /* catgets 139 */
    /* 40 */      "Running out of privileged socks", /* catgets 140 */
    /* 41 */      "getwd failed",  /* catgets 141 */
    /* 42 */      "Connection is lost",  /* catgets 142 */
    /* 43 */      "No such remote child", /* catgets 143 */
    /* 44 */      "Permission denied", /* catgets 144 */
    /* 45 */      "Ptymode inconsistency on ls_rtask", /* catgets 145 */
    /* 46 */      "Bad host name",  /* catgets 146 */
    /* 47 */      "NIOS protocol error", /* catgets 147 */
    /* 48 */      "A wait system call failed", /* catgets 148 */
    /* 49 */      "Bad parameters for setstdin", /* catgets 149 */
    /* 50 */      "Insufficient list length for returned rpids", /* catgets 150 */
    /* 51 */      "Invalid cluster name", /* catgets 151 */
    /* 52 */      "Incompatible versions of tty params",  /* catgets 152 */
    /* 53 */      "Failed in a execv() system call", /* catgets 153 */
    /* 54 */      "No such directory",  /* catgets 154 */
    /* 55 */      "Directory may not be accessible",  /* catgets 155 */
    /* 56 */      "Invalid service Id", /* catgets 156 */
    /* 57 */      "Request from a non-LSF host rejected", /* catgets 157 */
    /* 58 */      "Unknown resource name", /* catgets 158 */
    /* 59 */      "Unknown resource value", /* catgets 159 */
    /* 60 */      "Task already exists", /* catgets 160 */
    /* 61 */      "Task does not exist", /* catgets 161 */
    /* 62 */      "Task table is full",  /* catgets 162 */
    /* 63 */      "A resource limit system call failed", /* catgets 163 */
    /* 64 */      "Bad index name list", /* catgets 164 */
    /* 65 */      "LIM malloc failed", /* catgets 165 */
    /* 66 */      "NIO not initialized", /* catgets 166 */
    /* 67 */      "Bad syntax in lsf.conf", /* catgets 167 */
    /* 68 */      "File operation failed", /* catgets 168 */
    /* 69 */      "A connect sys call failed", /* catgets 169 */
    /* 70 */      "A select system call failed", /* catgets 170 */
    /* 71 */      "End of file", /* catgets 171 */
    /* 72 */      "Bad lsf accounting record format", /* catgets 172 */
    /* 73 */      "Bad time specification", /* catgets 173 */
    /* 74 */      "Unable to fork child", /* catgets 174 */
    /* 75 */      "Failed to setup pipe", /* catgets 175 */
    /* 76 */      "Unable to access esub/eexec file",  /* catgets 176 */
    /* 77 */      "External authentication failed",   /* catgets 177 */
    /* 78 */      "Cannot open file", /* catgets 178 */
    /* 79 */      "Out of communication channels",        /* catgets 179 */
    /* 80 */      "Bad communication channel",            /* catgets 180 */
    /* 81 */      "Internal library error", /* catgets 181 */
    /* 82 */      "Protocol error with server", /* catgets 182 */
    /* 83 */      "A system call failed", /* catgets 183 */
    /* 84 */      "Failed to get rusage", /* catgets 184 */
    /* 85 */      "No shared resources", /* catgets 185 */
    /* 86 */      "Bad resource name", /* catgets 186 */
    /* 87 */      "Failed to contact RES parent", /* catgets 187 */
    /* 88 */      "i18n setlocale failed",   /* catgets 188 */
    /* 89 */      "i18n catopen failed", /* catgets 189 */
    /* 90 */      "i18n malloc failed",  /* catgets 190 */
    /* 91 */      "Cannot allocate memory", /* catgets 191 */
    /* 92 */      "Close a NULL-FILE pointer", /* catgets 192 */
    /* 93 */      "Slave LIM configuration is not ready yet", /* catgets 193 */
    /* 94 */       "Master LIM is down; try later", /* catgets 194 */
    /* 95 */       "Requested label is not valid", /* catgets 195 */
    /* 96 */       "Requested label is above your allowed range", /* catgets 196 */
    /* 97 */       "Request label rejected by /etc/rhost.conf", /* catgets 197 */
    /* 98 */       "Request label doesn't dominate current label", /* catgets 198 */
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
    char  buf[16384];
    char tmpbuf[4096];
    char verBuf[16384];

    int save_errno = errno;

    vsprintf(buf, err_str_(save_errno, fmt, tmpbuf), ap);

    now = time(0);
    if (lastmsg[0] && (strcmp(buf, lastmsg) == 0) && (now - lastime < 600)) {
        count++;
        lastcall = now;
        return;
    } else {
        if (count) {
            (void)fprintf(fp, "%s %d ", _i18n_ctime(ls_catd,  CTIME_FORMAT_b_d_T_Y, &lastcall), (int) getpid());
            (void)fprintf(fp,(_i18n_msg_get(ls_catd,NL_SETN,1, "Last message repeated %d times\n")),count);   /* catgets 1 */
        }
        (void)fprintf(fp, "%s %d ", _i18n_ctime(ls_catd,  CTIME_FORMAT_b_d_T_Y, &now), (int) getpid());
    }

    if (level >= 0) {
        snprintf(verBuf, sizeof(verBuf), "%d %.8000s %.8000s",
                 level, _LAVALITE_VERSION_, buf);
    } else {
        snprintf(verBuf, sizeof(verBuf), "%.8000s %.8000s",
                 _LAVALITE_VERSION_, buf);
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
        sprintf(buf, I18N(2, "Error %d"), lserrno); /* catgets 2 */
        return buf;
    }

    if (LSE_SYSCALL(lserrno)) {
        if (strerror(save_errno) != NULL && save_errno > 0)
            sprintf(buf, "%s: %s", _i18n_msg_get(ls_catd, NL_SETN, ls_errmsg_ID[lserrno], ls_errmsg[lserrno]), strerror(save_errno));
        else
            sprintf(buf, (_i18n_msg_get(ls_catd,NL_SETN,3, "%s: unknown system error %d")), ls_errmsg[lserrno], save_errno);  /* catgets 3 */
        return buf;
    }

    if ((lserrno == LSE_LIM_DOWN) && (masterLimDown == true)) {
        return(_i18n_msg_get(ls_catd, NL_SETN, ls_errmsg_ID[LSE_MASTER_LIM_DOWN],ls_errmsg[LSE_MASTER_LIM_DOWN]));
    }

    return(_i18n_msg_get(ls_catd, NL_SETN, ls_errmsg_ID[lserrno],ls_errmsg[lserrno]));

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
