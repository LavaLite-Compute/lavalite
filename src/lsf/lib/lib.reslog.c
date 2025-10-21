/* $Id: lib.reslog.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

int
ls_putacctrec(FILE *log_fp, struct lsfAcctRec *acctRec)
{
    if (fprintf(log_fp, "%d", acctRec->pid) < 0
        || addQStr(log_fp, acctRec->username) < 0
        || fprintf(log_fp, " %d", acctRec->exitStatus) < 0
        || fprintf(log_fp, " %d", (int)acctRec->dispTime) < 0
        || fprintf(log_fp, " %d", (int)acctRec->termTime) < 0
        || addQStr(log_fp, acctRec->fromHost) < 0
        || addQStr(log_fp, acctRec->execHost) < 0
        || addQStr(log_fp, acctRec->cwd) < 0
        || addQStr(log_fp, acctRec->cmdln) < 0
        || lsfRu2Str(log_fp, &acctRec->lsfRu) < 0
        || fprintf(log_fp, "\n") < 0) {
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    return 0;

}

struct lsfAcctRec *
ls_getacctrec(FILE *log_fp, int *lineNum)
{
    int cc, ccount;
    int len, disp, term;
    int sar = sizeof(struct lsfAcctRec);
    char *line;
    static struct lsfAcctRec *acctRec = NULL;

    if (acctRec != NULL) {
        free(acctRec);
        acctRec = NULL;
    }

    (*lineNum)++;

    if ((line = getNextLine_(log_fp, false)) == NULL) {
        lserrno = LSE_EOF;
        return NULL;
    }

    len = strlen(line)*sizeof(char);
    acctRec = malloc(sar + 5*len);
    if (acctRec == NULL) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    acctRec->username = (char *) acctRec + sar;
    acctRec->fromHost = acctRec->username + len;
    acctRec->execHost = acctRec->fromHost + len;
    acctRec->cwd = acctRec->execHost + len;
    acctRec->cmdln = acctRec->cwd + len;

    if ((cc = sscanf(line, "%d%n", &acctRec->pid, &ccount)) != 1) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((ccount = stripQStr(line, acctRec->username)) < 0) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    cc = sscanf(line, "%d%d%d%n", &acctRec->exitStatus, &disp, &term, &ccount);
    acctRec->dispTime = disp;
    acctRec->termTime = term;

    if (cc != 3) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((ccount = stripQStr(line, acctRec->fromHost)) < 0) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((ccount = stripQStr(line, acctRec->execHost)) < 0) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((ccount = stripQStr(line, acctRec->cwd)) < 0) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((ccount = stripQStr(line, acctRec->cmdln)) < 0) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }
    line += ccount + 1;

    if ((cc = str2lsfRu(line, &acctRec->lsfRu, &ccount)) != 19) {
        lserrno = LSE_ACCT_FORMAT;
        return NULL;
    }

    return acctRec;
}
