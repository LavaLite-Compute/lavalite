/* $Id: res.tasklog.c,v 1.3 2007/08/15 22:18:56 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/res/res.h"

extern int flock(int, int);

#define NL_SETN		29

void initResLog(void);
void resAcctWrite(struct child *);
static void openResAcctFileInTmp(char *);



int resLogOn = 0;

int resLogcpuTime = -1;

char resAcctFN[MAXFILENAMELEN];

void
initResLog(void)
{
    static char fname[] = "initResLog()";
    int fd;
    char *acctDir = NULL;
    struct stat st;

    if (resLogcpuTime == -1) {
	if (resParams[LSF_RES_ACCT].paramValue) {
	    resLogOn = 1;
	    resLogcpuTime = 0;
	    if (resParams[LSF_RES_ACCT].paramValue[0] != '\0') {
		resLogcpuTime = atoi(resParams[LSF_RES_ACCT].paramValue);
		if (resLogcpuTime < 0) {
		    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5700,
			"%s: LSF_RES_ACCT cputime value <%s> must be a non-negative value; 0 msec assumed"), /* catgets 5700 */
			fname,
			resParams[LSF_RES_ACCT].paramValue);
		    resLogcpuTime = 0;
		}
	    }
	}
    }

    if (resLogOn != 1)
	return;

    acctDir = resParams[LSF_RES_ACCTDIR].paramValue;




    if (acctDir != NULL)
	strcpy(resAcctFN, acctDir);
    else
	strcpy(resAcctFN, "/tmp");

    strcat(resAcctFN, "/lsf.acct.");

    strcat(resAcctFN, Myhost);

    if (lstat(resAcctFN, &st) < 0) {
	if (errno == ENOENT) {
	    if ((fd = open(resAcctFN, O_CREAT,
			   S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", resAcctFN);
		ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5701,
"%s: Using /tmp/lsf.acct for task logging"), fname); /* catgets 5701 */
		strcpy(resAcctFN, "/tmp/lsf.acct.");
		strcat(resAcctFN, Myhost);
		openResAcctFileInTmp(resAcctFN);
            } else
		close(fd);
	} else {
	    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "stat", resAcctFN);
	    resLogOn = -1;
	}
    } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {

	if ((fd = open(resAcctFN, O_APPEND,
				  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", resAcctFN);
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5701,
"%s: Using /tmp/lsf.acct for task logging"), fname); /* catgets 5701 */
            strcpy(resAcctFN, "/tmp/lsf.acct.");
            strcat(resAcctFN, Myhost);
	    openResAcctFileInTmp(resAcctFN);
	} else
	    close(fd);
    } else {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5705,
"%s: file <%s> is not a regular file, file untouched"), fname, resAcctFN);
							/* catgets 5705 */
	resLogOn = -1;
    }

    if (resLogOn != -1)
	ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5702, "Task Log ON: Logging tasks with cpuTime > %d msec to file <%s>")), resLogcpuTime, resAcctFN);   /* catgets 5702 */

}

static void
openResAcctFileInTmp(char *resAcctFN)
{
    static char fname[] = "openResAcctFileInTmp";
    struct stat st;
    int fd;

    if (lstat(resAcctFN, &st) < 0) {
        if (errno == ENOENT) {
            if ((fd = open(resAcctFN, O_CREAT,
                            	S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", resAcctFN);
		resLogOn = -1;
                return;
            }
	    close(fd);
	    return;
        } else {
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "stat", resAcctFN);
	    resLogOn = -1;
            return;
        }
    } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {

        if ((fd = open(resAcctFN, O_APPEND,
				  S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "open", resAcctFN);
	    resLogOn = -1;
            return;
        }
	close(fd);
	return;
    }
    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5705,
"%s: file <%s> is not a regular file, file untouched"), fname, resAcctFN );
							/* catgets 5705 */
    resLogOn = -1;
    return;
}

void
resAcctWrite(struct child *child)
{
    static char fname[] = "resAcctWrite()";
    FILE *fd;
    int l = 0, j = 0;
    struct lsfAcctRec acctRec;
    char acctFile[MAXFILENAMELEN];


    if (debug >1)
	ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 5703, "resAcctWrite")));  /* catgets 5703 */

    if (sbdMode) {
	if (access(resAcctFN, F_OK) == 0) {

	    return;
	}

	strcpy(acctFile, resAcctFN);
    } else {

	sprintf(acctFile, "%s/lsf.acct.%s.%d.%d",
		LSTMPDIR, Myhost, (int) getpid(), (int) time(NULL));
    }


    fd = fopen(acctFile, "w");
    if (fd == (FILE *) NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen",
	    acctFile);
	return;
    }

    acctRec.pid = child->pid;
    acctRec.username = child->username;
    acctRec.exitStatus = LS_STATUS(child->wait);
    acctRec.dispTime = child->dpTime;
    acctRec.termTime = time(NULL);
    acctRec.fromHost = child->fromhost;
    acctRec.execHost = Myhost;
    acctRec.cwd = child->cwd;



    while (child->cmdln[j] != NULL) {
        l += strlen(child->cmdln[j]) + 1;
        j++;
    }
    acctRec.cmdln = (char *) malloc (l * (sizeof(char)));
    if (acctRec.cmdln == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
	FCLOSEUP(&fd);
	unlink(acctFile);
	return;
    } else {
        j = 0;
        strcpy (acctRec.cmdln, child->cmdln[j++]);
        while (child->cmdln[j] != NULL) {
            strcat(acctRec.cmdln, " ");
            strcat(acctRec.cmdln, child->cmdln[j++]);
        }
    }

    cleanLsfRusage(&acctRec.lsfRu);
    if (child->sigStatRu)
        ls_ruunix2lsf(&(child->sigStatRu->ru), &acctRec.lsfRu);

    if (ls_putacctrec(fd, &acctRec) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_putacctrec");
	FCLOSEUP(&fd);
	free(acctRec.cmdln);
	unlink(acctFile);
	return;
    }

    if (logclass & LC_EXEC) {
	ls_syslog(LOG_DEBUG, I18N(5704,
	   	  "%s: pid <%d> status <%d> exitcode <%d>"), /*catgets 5704*/
		  fname, child->pid, child->wait, WEXITSTATUS(child->wait));
    }

    FCLOSEUP(&fd);
    free(acctRec.cmdln);

    if (!sbdMode) {

	resAck ack;
	struct LSFHeader msgHdr;
	struct stringLen str;

	str.len = strlen(acctFile) + 1;
	str.name = acctFile;

	initLSFHeader_(&msgHdr);
	msgHdr.opCode = RES_ACCT;

	if ((ack = sendResParent(&msgHdr, (char *) &str, xdr_stringLen)) !=
	    RESE_OK)
	    ls_syslog(LOG_DEBUG, "lsfAcctWrite: file <%s>: parent failed to process cct info, ack=%d", acctFile, (int) ack);

	unlink(acctFile);
    }


}

void
resParentWriteAcct(struct LSFHeader *msgHdr, XDR *xdrs, int sock)
{
    static char fname[] = "resParentWriteAcct";
    char acctFile[MAXFILENAMELEN];
    FILE *fp;
    char *buf = NULL;
    struct stat sbuf;
    struct stringLen str;
    struct stat st;

    const char *readMode = "r";
    const char *appendMode = "a";

    str.name = acctFile;
    str.len = sizeof(acctFile);

    if (!xdr_stringLen(xdrs, &str, msgHdr)) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "xdr_stringLen");
	sendReturnCode(sock, RESE_REQUEST);
	return;
    }

    if (stat(acctFile, &sbuf) < 0) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "stat", acctFile);
	sendReturnCode(sock, RESE_DENIED);
	return;
    }

    if ((buf = malloc((int) sbuf.st_size)) == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
	sendReturnCode(sock, RESE_NOMEM);
	return;
    }

    if ((fp = fopen(acctFile, readMode)) == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen",
	    acctFile);
	sendReturnCode(sock, RESE_FILE);
	free(buf);
	return;
    }

    if (fread(buf, 1, sbuf.st_size, fp) != sbuf.st_size) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_D_FAIL_M, fname, "fread",
	    acctFile,
	    (int) sbuf.st_size);
	sendReturnCode(sock, RESE_FILE);
	free(acctFile);
	free(buf);
	return;
    }

    FCLOSEUP(&fp);
    if (lstat(resAcctFN, &st) < 0 ) {
	if (errno != ENOENT || (errno == ENOENT
			&& (fp = fopen(resAcctFN, appendMode)) == NULL)) {
	    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen", resAcctFN);
	    sendReturnCode(sock, RESE_FILE);
	    free(buf);
	    return;
	}
    } else if (S_ISREG(st.st_mode) && st.st_nlink == 1) {

	if ((fp = fopen(resAcctFN, appendMode)) == NULL) {
	    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "fopen", resAcctFN);
	    sendReturnCode(sock, RESE_FILE);
	    free(buf);
	    return;
	}
    } else {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5705,
"%s: file <%s> is not a regular file, file untouched"), fname, resAcctFN);
                                                        /* catgets 5705 */
	sendReturnCode(sock, RESE_FILE);
	free(buf);
	return;
    }

    if (fwrite(buf, 1, sbuf.st_size, fp) != sbuf.st_size) {
	ls_syslog(LOG_ERR, I18N_FUNC_S_D_FAIL_M, fname, "fwrite",
	    acctFile, (int) sbuf.st_size);
	sendReturnCode(sock, RESE_FILE);
	free(buf);
	return;
    }

    FCLOSEUP(&fp);
    free(buf);
    sendReturnCode(sock, RESE_OK);

}
