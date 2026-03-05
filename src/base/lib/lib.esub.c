/* $Id: lib.esub.c,v 1.4 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/ll.sysenv.h"

#define ESUBNAME "esub"
#define EEXECNAME "eexec"
#define EGROUPNAME "egroup"

static int getEData(struct lenData *, char **, const char *);

int runEsub_(struct lenData *ed, char *path)
{
    char esub[MAXPATHLEN];
    char *myargv[6];
    struct stat sbuf;

    ed->len = 0;
    ed->data = NULL;

    myargv[0] = esub;
    if (path == NULL) {
        sprintf(esub, "%s/%s", genParams[LSF_SERVERDIR].paramValue, ESUBNAME);
        myargv[1] = NULL;
    } else {
        if (*path == '\0')
            strcpy(esub, ESUBNAME);
        else
            sprintf(esub, "%s/%s", path, ESUBNAME);
        myargv[1] = "-r";
        myargv[2] = NULL;
    }

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "runEsub(): esub=<%s>", esub);

    if (stat(esub, &sbuf) < 0) {
        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG, "runEsub: stat(%s) failed: %m", esub);
        lserrno = LSE_ESUB;
        return -1;
    }

    if (runEClient_(ed, myargv) == -1)
        return -2;

    return 0;
}

int runEClient_(struct lenData *ed, char **argv)
{
    char buf[LL_BUFSIZ_32] = {0};
    return getEData(ed, argv, buf);
}

static int getEData(struct lenData *ed, char **argv, const char *user)
{
    char *buf;
    char *sp;
    int ePorts[2];
    int pid;
    int cc;
    int size;
    int status;
    char *abortVal;
    uid_t uid;

    if (get_uid(user, &uid) < 0) {
        syslog(LOG_ERR, "%s: get_uid() failed: %m", __func__);
        return -1;
    }

    if (pipe(ePorts) < 0) {
        if (logclass & (LC_TRACE | LC_AUTH))
            ls_syslog(LOG_DEBUG, "%s: pipe failed: %m", __func__);
        lserrno = LSE_PIPE;
        return -1;
    }

    if ((pid = fork()) == 0) {
        close(ePorts[0]);
        dup2(ePorts[1], 1);

        execvp(argv[0], argv);
        ls_syslog(LOG_DEBUG, "%s: execvp(%s) failed: %m", __func__, argv[0]);
        exit(-1);
    }

    if (pid == -1) {
        if (logclass & (LC_TRACE | LC_AUTH))
            ls_syslog(LOG_DEBUG, "%s: fork failed aborting child", __func__);
        close(ePorts[0]);
        close(ePorts[1]);
        lserrno = LSE_FORK;
        return -1;
    }

    close(ePorts[1]);

    ed->len = 0;
    ed->data = NULL;

    buf = calloc((MSGSIZE + 1), sizeof(char));
    if (buf == NULL) {
        lserrno = LSE_MALLOC;
        goto errorReturn;
    }

    for (size = MSGSIZE, ed->len = 0, sp = buf;
         (cc = read(ePorts[0], sp, size));) {
        if (cc == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        ed->len += cc;
        sp += cc;
        size -= cc;
        if (size == 0) {
            if ((sp = realloc(buf, ed->len + MSGSIZE + 1)) == NULL) {
                if (logclass & (LC_TRACE | LC_AUTH))
                    ls_syslog(LOG_DEBUG, "%s: realloc failed: %m", __func__);
                lserrno = LSE_MALLOC;
                free(buf);
                goto errorReturn;
            }
            buf = sp;
            sp = buf + ed->len;
            size = MSGSIZE;
        }
    }

    close(ePorts[0]);
    ed->data = buf;

    ed->data[ed->len] = '\0';

    while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
        ;

    if ((abortVal = getenv("LSB_SUB_ABORT_VALUE"))) {
        if ((WIFEXITED(status) && WEXITSTATUS(status) == atoi(abortVal)) ||
            WIFSIGNALED(status)) {
            FREEUP(ed->data);
            ed->len = 0;
            return -1;
        }
    }

    if (ed->len == 0) {
        FREEUP(ed->data);
    }

    return 0;

errorReturn:
    close(ePorts[0]);
    kill(pid, SIGKILL);
    while (waitpid(pid, 0, 0) == -1 && errno == EINTR)
        ;
    ed->len = 0;
    ed->data = NULL;
    return -1;
}

int runEexec_(char *option, int job, struct lenData *eexec, char *path)
{
    static char fname[] = "runEexec";
    char eexecPath[MAXFILENAMELEN], *myargv[3];
    int pid = -1, i, p[2], cc, isRenew = false;
    struct stat sbuf;

    if (strcmp(option, "-r") == 0)
        isRenew = true;

    if (isRenew == true) {
        if (path[0] == '\0')
            strcpy(eexecPath, EEXECNAME);
        else
            sprintf(eexecPath, "%s/%s", path, EEXECNAME);
    } else {
        sprintf(eexecPath, "%s/%s", genParams[LSF_SERVERDIR].paramValue,
                EEXECNAME);
    }

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG,
                  "%s: eexec path, option and data length of job/task <%d> are "
                  "<%s>, <%s> and <%d>",
                  fname, job, eexecPath, option, eexec->len);

    if (stat(eexecPath, &sbuf) < 0) {
        if (logclass & LC_TRACE)
            ls_syslog(
                LOG_DEBUG,
                "%s: Job/task <%d> eexec will not be run, stat(%s) failed: %m",
                fname, job, eexecPath);
        lserrno = LSE_ESUB;
        return -1;
    }

    i = 0;
    myargv[i++] = eexecPath;
    if (strcmp(option, "-r") == 0)
        myargv[i++] = "-r";

    myargv[i] = NULL;

    if (pipe(p) < 0) {
        lserrno = LSE_PIPE;
        return -1;
    }

    if ((pid = fork()) == 0) {
        char *user;
        uid_t uid;

        {
            struct passwd *pw;

            if ((user = getenv("LSFUSER")) != NULL) {
                if (get_uid(user, &uid) < 0) {
                    ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname,
                              "getOSUid_", user);
                    exit(-1);
                }
            } else {
                user = getenv("USER");
                if ((pw = getpwnam2(user)) == NULL) {
                    ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname,
                              "getpwnam2", user);
                    exit(-1);
                }
                uid = pw->pw_uid;
            }
        }

        if (setuid(uid) < 0) {
            ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", fname, "setuid",
                      (int) uid);
            exit(-1);
        }

        if (setpgid(0, getpid()) < 0) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "setpgid");
            exit(-1);
        }

        for (i = 1; i < NSIG; i++)
            signal_set(i, SIG_DFL);

        alarm(0);

        close(p[1]);
        if (dup2(p[0], 0) == -1) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "dup2(stdin,0)");
        }

        for (i = 3; i < sysconf(_SC_OPEN_MAX); i++)
            close(i);

        execvp(myargv[0], myargv);
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "execvp", myargv[0]);
        exit(-1);
    }

    if (pid == -1) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "fork");
        close(p[0]);
        close(p[1]);
        lserrno = LSE_FORK;
        return -1;
    }

    close(p[0]);

    if (eexec->len > 0) {
        if ((cc = b_write_fix(p[1], eexec->data, eexec->len)) != eexec->len &&
            strcmp(option, "-p")) {
            ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", fname, "b_write_fix",
                      eexec->len);
        }
    }

    close(p[1]);

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
        ;

    return 0;
}

char *runEGroup_(char *type, char *gname)
{
    static char fname[] = "runEGroup";
    struct lenData ed;
    char lsfUserName[MAXLSFNAMELEN];
    char egroupPath[MAXFILENAMELEN];
    char *argv[4];
    char *managerIdStr;
    int uid;
    struct stat sbuf;

    sprintf(egroupPath, "%s/%s", genParams[LSF_SERVERDIR].paramValue,
            EGROUPNAME);

    argv[0] = egroupPath;
    argv[1] = type;
    argv[2] = gname;
    argv[3] = NULL;

    uid = getuid();
    if (uid == 0 && (managerIdStr = getenv("LSB_MANAGERID")) != NULL) {
        uid = atoi(managerIdStr);
        if (getpwuid(uid)) {
            ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", fname,
                      "getLSFUserByUid_", uid);
            return NULL;
        }
    } else {
        if (getpwnam2(lsfUserName) == NULL) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "getpwnam2");
            return NULL;
        }
    }

    if (stat(egroupPath, &sbuf) < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "stat", egroupPath);
        return NULL;
    }

    if (getEData(&ed, argv, lsfUserName) < 0) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "getEData",
                  egroupPath);
        return NULL;
    }

    if (ed.len > 0) {
        ed.data[ed.len] = '\0';
        return ed.data;
    }

    return NULL;
}
