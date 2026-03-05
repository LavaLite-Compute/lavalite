/* $Id: sbd.misc.c,v 1.4 2007/08/15 22:18:46 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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

#include <stdlib.h>

#include "lsbatch/daemons/sbd.h"

#include "../../lsf/lib/lsi18n.h"

extern short mbdExitVal;
extern int mbdExitCnt;

void milliSleep(int msec)
{
    struct timeval dtime;

    if (msec < 1)
        return;
    dtime.tv_sec = msec / 1000;
    dtime.tv_usec = (msec - dtime.tv_sec * 1000) * 1000;

    select(0, 0, 0, 0, &dtime);
}

char window_ok(struct jobCard *jobPtr)
{
    windows_t *wp;
    struct dayhour dayhour;
    char active;
    time_t ckTime;
    time_t now;

    now = time(0);
    active = jobPtr->active;

    if (active && (jobPtr->jobSpecs.options & SUB_WINDOW_SIG))
        ckTime = now + WARN_TIME;
    else
        ckTime = now;

    if (jobPtr->windEdge > ckTime || jobPtr->windEdge == 0)
        return jobPtr->active;

    getDayHour(&dayhour, ckTime);
    if (jobPtr->week[dayhour.day] == NULL) {
        jobPtr->active = TRUE;
        jobPtr->windEdge = now + (24.0 - dayhour.hour) * 3600.0;
        return jobPtr->active;
    }

    jobPtr->active = FALSE;
    jobPtr->windEdge = now + (24.0 - dayhour.hour) * 3600.0;
    for (wp = jobPtr->week[dayhour.day]; wp; wp = wp->nextwind)
        checkWindow(&dayhour, &jobPtr->active, &jobPtr->windEdge, wp, now);

    if (active && !jobPtr->active && now - jobPtr->windWarnTime >= WARN_TIME &&
        (jobPtr->jobSpecs.options & SUB_WINDOW_SIG)) {
        if (!(jobPtr->jobSpecs.jStatus & JOB_STAT_RUN))
            job_resume(jobPtr);
        jobsig(jobPtr, sig_decode(jobPtr->jobSpecs.sigValue), TRUE);
        jobPtr->windWarnTime = now;
    }

    return jobPtr->active;
}
void shout_err(struct jobCard *jobPtr, char *msg)
{
    char buf[MSGSIZE];

    sprintf(buf, "We are unable to run your job %s:<%s>. The error is:\n%s.",
            lsb_jobid2str(jobPtr->jobSpecs.jobId), jobPtr->jobSpecs.command,
            msg);

    if (jobPtr->jobSpecs.options & SUB_MAIL_USER)
        merr_user(jobPtr->jobSpecs.mailUser, jobPtr->jobSpecs.fromHost, buf,
                  "error");
    else
        merr_user(jobPtr->jobSpecs.userName, jobPtr->jobSpecs.fromHost, buf,
                  "error");
}

void child_handler(int sig)
{
    int pid;
    int status;
    struct rusage rusage;
    register float cpuTime;
    struct lsfRusage lsfRusage;
    struct jobCard *jobCard;
    static short lastMbdExitVal = MASTER_NULL;
    static int sbd_finish_sleep = -1;

    cleanRusage(&rusage);
    now = time(0);
    while ((pid = wait3(&status, WNOHANG, &rusage)) > 0) {
        if (pid == mbdPid) {
            int sig = WTERMSIG(status);
            if (mbdExitCnt > 150)
                mbdExitCnt = 150;
            mbdExitVal = WIFSIGNALED(status);
            if (mbdExitVal) {
                ls_syslog(LOG_ERR, "mbatchd died with signal <%d> termination",
                          sig);
                if (WCOREDUMP(status))
                    ls_syslog(LOG_ERR, "mbatchd core dumped");
                mbdExitVal = sig;
                if (mbdExitVal == lastMbdExitVal)
                    mbdExitCnt++;
                else {
                    mbdExitCnt = 0;
                    lastMbdExitVal = mbdExitVal;
                }
                continue;
            } else {
                mbdExitVal = WEXITSTATUS(status);

                if (mbdExitVal == lastMbdExitVal)
                    mbdExitCnt++;
                else {
                    mbdExitCnt = 0;
                    lastMbdExitVal = mbdExitVal;
                }
                if (mbdExitVal == MASTER_RECONFIG) {
                    ls_syslog(LOG_NOTICE,
                              "mbatchd resigned for reconfiguration");
                    start_master();
                } else
                    ls_syslog(LOG_NOTICE, "mbatchd exited with value <%d>",
                              mbdExitVal);
                continue;
            }
        }

        ls_ruunix2lsf(&rusage, &lsfRusage);
        cpuTime = lsfRusage.ru_utime + lsfRusage.ru_stime;

        for (jobCard = jobQueHead->forw; (jobCard != jobQueHead);
             jobCard = jobCard->forw) {
            if (jobCard->exitPid == pid) {
                jobCard->w_status = LS_STATUS(status);
                jobCard->exitPid = -1;
                if (logclass & LC_EXEC) {
                    ls_syslog(LOG_DEBUG,
                              "child_handler: Job <%s> exitPid <%d> status "
                              "<%d> exitcode <%d>",
                              lsb_jobid2str(jobCard->jobSpecs.jobId), pid,
                              jobCard->w_status, WEXITSTATUS(status));
                }
            }

            if (jobCard->jobSpecs.jobPid == pid) {
                jobCard->collectedChild = TRUE;
                jobCard->cpuTime = cpuTime;
                jobCard->w_status = LS_STATUS(status);
                jobCard->exitPid = -1;
                memcpy((char *) &jobCard->lsfRusage, (char *) &lsfRusage,
                       sizeof(struct lsfRusage));
                jobCard->notReported++;

                if (sbd_finish_sleep < 0) {
                    if (lsbParams[LSB_SBD_FINISH_SLEEP].paramValue) {
                        errno = 0;
                        sbd_finish_sleep =
                            atoi(lsbParams[LSB_SBD_FINISH_SLEEP].paramValue);
                        if (errno)
                            sbd_finish_sleep = 1000;
                    } else {
                        sbd_finish_sleep = 1000;
                    }
                }
                if (sbd_finish_sleep > 0) {
                    millisleep_(sbd_finish_sleep);
                }

                if (logclass & LC_EXEC) {
                    ls_syslog(LOG_DEBUG,
                              "child_handler: Job <%s> Pid <%d> status <%d> "
                              "exitcode <%d>",
                              lsb_jobid2str(jobCard->jobSpecs.jobId), pid,
                              jobCard->w_status, WEXITSTATUS(status));
                }
                need_checkfinish = TRUE;

                break;
            }
        }
    }
}

#ifndef BSIZE
#define BSIZE 1024
#endif

int fcp(char *file1, char *file2, struct hostent *hp)
{
    static char fname[] = "fcp()";
    struct stat sbuf;
    int fd1, fd2;
    char buf[BSIZE];
    int cc;

    fd1 = myopen_(file1, O_RDONLY, 0, hp);
    if (fd1 < 0)
        return -1;

    if (fstat(fd1, &sbuf) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "fstat", file1);
        close(fd1);
        return -1;
    }

    fd2 = myopen_(file2, O_CREAT | O_TRUNC | O_WRONLY, (int) sbuf.st_mode, hp);
    if (fd2 < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "creat", file1);
        close(fd1);
        return -1;
    }

    for (;;) {
        cc = read(fd1, buf, BSIZE);
        if (cc == 0)
            break;
        if (cc < 0) {
            close(fd1);
            close(fd2);
            return -1;
        }
        if (write(fd2, buf, cc) != cc) {
            close(fd1);
            close(fd2);
            return -1;
        }
    }

    close(fd1);
    close(fd2);
    return 0;
}

#include <sys/dir.h>

int rmDir(char *dir)
{
    DIR *dirp;
    struct direct *dp;
    char path[MAXPATHLEN];

    if ((dirp = opendir(dir)) == NULL)
        return -1;

    readdir(dirp);
    readdir(dirp);

    for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
        sprintf(path, "%s/%s", dir, dp->d_name);
        rmdir(path);
        unlink(path);
    }

    closedir(dirp);
    return (rmdir(dir));
}

void closeBatchSocket(void)
{
    if (batchSock > 0) {
        chan_close(batchSock);
        batchSock = -1;
    }
}

void getManagerId(struct sbdPackage *sbdPackage)
{
    static char fname[] = "getManagerId";
    struct passwd *pw;
    int i;

    FREEUP(lsbManager);

    if (sbdPackage->nAdmins <= 0) {
        ls_syslog(LOG_ERR, "%s: No LSF administrator defined in sbdPackage",
                  fname);
        die(FATAL_ERR);
    }

    for (i = 0; i < sbdPackage->nAdmins; i++) {
        if ((pw = getpwlsfuser_(sbdPackage->admins[i])) != NULL) {
            lsbManager = safeSave(sbdPackage->admins[i]);
            managerId = pw->pw_uid;
            break;
        }
    }

    if (lsbManager == NULL) {
        ls_syslog(LOG_ERR, "%s: No LSF administrator defined in sbdPackage",
                  fname);
        die(FATAL_ERR);
    }
}
