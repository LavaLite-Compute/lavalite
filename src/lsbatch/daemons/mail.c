/* $Id: mail.c,v 1.3 2007/08/15 22:18:45 tmizan Exp $
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

#include "lsbatch/daemons/daemons.h"

#ifdef NO_MAIL
void lsb_mperr (char *msg) {}
void lsb_merr (char *s) {}
void merr_user (char *user, char *host, char *msg, char *type) {}
static void addr_process (char *adbuf, char *user, char *tohost, char *spec) {}
FILE * smail (char *to, char *tohost) {return fopen("/dev/null", "w");}
void mclose (FILE *file) {fclose(file);}

#else
void
lsb_mperr (char *msg)
{
    static char fname[] = "lsb_mperr()";
    char *p;
    char err[MAXLINELEN];

    if (lsb_CheckMode)
	return;

    p=strchr(msg,'\n');
    if(p != NULL )
        *p = '\0';

    if (strerror(errno) != NULL && errno >= 0)
        strcpy(err, strerror(errno));
    else
        sprintf(err, "%s: Unknown error", fname);

    lsb_merr2("%s: %s\n", msg, err);
}

void
lsb_merr (char *s)
{
    char fname[] = "lsb_merr";
    FILE *mail;
    char *myhostnm;

    if (lsb_CheckMode)
	return;

    if ((myhostnm = ls_getmyhostname()) == NULL) {
        ls_syslog(LOG_ERR, "%s", __func__, "ls_getmyhostname");
        if (masterme)
            die(MASTER_FATAL);
        else
            die (SLAVE_FATAL);
    }
    if (lsbManager == NULL || (getpwlsfuser_(lsbManager)) == NULL) {
        if (lsbManager == NULL)
            ls_syslog(LOG_ERR, "%s: LSF administrator name is NULL",
		fname);
        else
            ls_syslog(LOG_ERR, "%s: Bad LSF administrator name <%s>",
		fname,
		lsbManager);
        if (masterme)
            die (MASTER_FATAL);
        else
            die (SLAVE_FATAL);
    }
    mail = smail(lsbManager, myhostnm);

    if (masterme)
        fprintf(mail, "Subject: mbatchd on %s: %s\n",
	    myhostnm, s);
    else
        fprintf(mail, "Subject: sbatchd on %s: %s\n",
	    myhostnm, s);

    mclose(mail);
}

void
merr_user (char *user, char *host, char *msg, char *type)
{
    char fname[] = "merr_user";
    FILE *mail;
    char *myhostnm;

    if ((myhostnm = ls_getmyhostname()) == NULL) {
        ls_syslog(LOG_ERR, "%s", __func__, "ls_getmyhostname");
	die(MASTER_FATAL);
    }

    mail = smail(user, host);
    fprintf(mail, "Subject: job %s report from %s\n",
	type,
	myhostnm);
    fprintf(mail, "\n\nDear %s,\n\n%s\n\n",
	user,
	msg);
    mclose(mail);
}

static void
addr_process (char *adbuf, char *user, char *tohost, char *spec)
{
    char *bp, *sp, *up;

    if (strrchr(user, '@') != NULL) {
        strcpy(adbuf, user);
        return;
    }

    bp = adbuf;
    for (sp = spec ; *sp ; sp++)
    {
	if ((*sp == '^') || (*sp == '!'))
	{
	    switch (*++sp)
	    {
	    case 'U':
		for (up = user ; *up ; )
		    *bp++ = *up++;
		continue;
	    case 'H':
		for (up = tohost ; *up ; )
		    *bp++ = *up++;
		continue;
	    default:
		sp -= 1;

	    }
	}
	*bp++ = *sp;
    }
    *bp = 0;
}

FILE *
smail (char *to, char *tohost)
{
    char fname[] = "smail";
    FILE *fmail;
    int pid;
    int maild[2];
    char toaddr[256];
    char smcmd[500];
    char *sendmailp;
    char osUserName[MAXLINELEN];
    uid_t userid;

    if (lsb_CheckMode)
	return stderr;

    if ((to == NULL) || (tohost == NULL)) {
        ls_syslog(LOG_ERR, "%s: Internal error: user name or host name is null", fname);
        return stderr;
    }

    if (getOSUserName_(to, osUserName, MAXLINELEN) != 0) {
        strncpy(osUserName, to, MAXLINELEN);
        osUserName[MAXLINELEN - 1] = '\0';
    }

    if (debug > 2)
	return stderr;

    if(pipe(maild) < 0) {
        ls_syslog(LOG_ERR, "%s", __func__, "pipe", osUserName);
        return stderr;
    }
    addr_process(toaddr, osUserName, tohost,
			 daemonParams[LSB_MAILTO].paramValue);
    if (logclass & (LC_TRACE | LC_EXEC))
        ls_syslog(LOG_DEBUG1, "%s: user=%s host=%s toaddr=%s spec=%s", fname,
            osUserName, tohost, toaddr, daemonParams[LSB_MAILTO].paramValue);
    switch(pid = fork()) {
    case 0:
        if (maild[0] != 0) {
            close(0);
            dup2(maild[0], 0);
            close(maild[0]);
        }
        close(maild[1]);

	sendmailp = daemonParams[LSB_MAILPROG].paramValue;

      userid = geteuid();
      chuser(getuid());
      setuid(userid);

        sprintf(smcmd, "%s -oi -F%s -f%s %s", sendmailp,
                       "'LSF'", lsbManager, toaddr);

        execle("/bin/sh", "sh", "-c", smcmd, (char *)0, environ);
        ls_syslog(LOG_ERR, "%s", __func__, "execle",
            daemonParams[LSB_MAILPROG].paramValue);
        exit(-1);

    case -1:
        close(maild[1]);
        close(maild[0]);
        ls_syslog(LOG_ERR, "%s", __func__, "fork", osUserName);
        return stderr;

    default:
        close(maild[0]);
    }
    fmail = fdopen(maild[1], "w");
    if(fmail == NULL) {
        ls_syslog(LOG_ERR, "%s", __func__, "fdopen", osUserName);
        close(maild[1]);
        return stderr;
    }

    fprintf(fmail, "%s: %s\n", I18N_To, toaddr);
    fprintf(fmail, "%s: LSF <%s>\n", I18N_From, lsbManager);
    fflush(fmail);
    if (ferror(fmail)) {
        fclose(fmail);
        ls_syslog(LOG_ERR, "%s", __func__, "fprintf", "header",
	    osUserName);
        return stderr;
    }
    return fmail;
}

void
mclose (FILE *file)
{
    if(file != stderr)
        (void) fclose(file);
    else
        (void) fflush(file);
}
#endif
