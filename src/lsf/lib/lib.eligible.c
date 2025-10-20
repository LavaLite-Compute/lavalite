/* $Id: lib.eligible.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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

hTab rtask_table;
hTab ltask_table;

static char listok;

static int inittasklists_(void);

char *
ls_resreq(char *task)
{
    static char resreq[MAXLINELEN];

    if (!ls_eligible(task, resreq, LSF_LOCAL_MODE))
        return NULL;
    else
        return resreq;

}


int
ls_eligible(char *task, char *resreqstr, char mode)
{
    hEnt *mykey;
    char *p;

    resreqstr[0] = '\0';
    if (! listok)
        if (inittasklists_() < 0)
            return false;

    lserrno = LSE_NO_ERR;

    if (! task)
        return false;

    if ((p=getenv("LSF_TRS")) == NULL) {
        if ((p = strrchr(task, '/')) == NULL)
            p = task;
        else
            p++;
    }
    else
        p = task;

    if (  mode == LSF_REMOTE_MODE
          && h_getEnt_(&ltask_table, (char *) p) != NULL)
    {
        return false;
    }

    if ((mykey = h_getEnt_(&rtask_table, (char *)p)) != NULL) {
        if (mykey->hData)
            strcpy(resreqstr,(char *)mykey->hData);
    }

    return (mode == LSF_REMOTE_MODE || mykey != NULL);

}

static int
inittasklists_(void)
{
    char filename[MAXFILENAMELEN];
    char *homep;
    char *clName;

    h_initTab_(&rtask_table, 0);
    h_initTab_(&ltask_table, 0);

    if (initenv_(NULL, NULL) <0)
        return -1;

    sprintf(filename, "%s/lsf.task", genParams_[LSF_CONFDIR].paramValue);
    if (readtaskfile_(filename, NULL, NULL, &ltask_table, &rtask_table,
                      false) < 0) {
        return -1;
    }

    clName = ls_getclustername();
    if (clName == NULL)
        return -1;

    sprintf(filename, "%s/lsf.task.%s",
            genParams_[LSF_CONFDIR].paramValue, clName);
    if (readtaskfile_(filename, NULL, NULL, &ltask_table, &rtask_table,
                      false) < 0) {
        return-1;
    }

    /* Bug. No $HOME?
     */
    homep = getenv("HOME");
    if (homep == NULL) {
        return -1;
    }

    strcpy(filename, homep);
    strcat(filename, "/.lsftask");
    if (readtaskfile_(filename,
                      NULL,
                      NULL,
                      &ltask_table,
                      &rtask_table,
                      false) < 0) {
        return -1;
    }

    return 0;
}

int
readtaskfile_(char *filename, hTab *minusListl, hTab *minusListr,
              hTab *localList, hTab *remoteList, char useMinus)
{
    FILE  *fp;
    enum phase {ph_begin, ph_remote, ph_local} phase;
    char *line;
    char *word;
    char minus;

    phase = ph_begin;
    if ((fp = fopen(filename,"r")) == NULL) {
        lserrno = LSE_FILE_SYS;
        return -1;
    }

    while ((line = getNextLine_(fp, true)) != NULL) {
        switch (phase) {
        case ph_begin:
            word = getNextWord_(&line);
            if (strcasecmp(word, "begin") != 0) {
                fclose(fp);
                lserrno = LSE_BAD_TASKF;
                return -1;
            }
            word = getNextWord_(&line);
            if (word == NULL) {
                fclose(fp);
                lserrno = LSE_BAD_TASKF;
                return -1;
            }
            if (strcasecmp(word, "remotetasks") == 0)
                phase = ph_remote;
            else if (strcasecmp(word, "localtasks") == 0)
                phase = ph_local;
            else {
                fclose(fp);
                lserrno = LSE_BAD_TASKF;
                return -1;
            }
            break;
        case ph_remote:
            word = getNextValueQ_(&line,'"','"');
            if (strcasecmp(word, "end") == 0) {
                word = getNextWord_(&line);
                if (word == NULL) {
                    fclose(fp);
                    lserrno = LSE_BAD_EXP;
                    return -1;
                }
                if (strcasecmp(word, "remotetasks") == 0) {
                    phase = ph_begin;
                    break;
                }
                fclose(fp);
                lserrno = LSE_BAD_EXP;
                return -1;
            }

            minus = false;
            if (strcmp(word, "+") == 0 || strcmp(word, "-") == 0)  {
                minus = (strcmp(word, "-") == 0);
                word = getNextValueQ_(&line,'"','"');
                if (word == NULL) {
                    fclose(fp);
                    lserrno = LSE_BAD_EXP;
                    return -1;
                }
            } else if (word[0] == '+' || word[0] == '-') {
                minus = word[0] == '-';
                word++;
            }

            if (minus) {
                if ((deletetask_(word, remoteList) < 0) && useMinus)
                    (void)inserttask_(word, minusListr);
            } else
                (void)inserttask_(word, remoteList);
            break;

        case ph_local:
            word = getNextWord_(&line);
            if (strcasecmp(word, "end") == 0) {
                word = getNextWord_(&line);
                if (word == NULL) {
                    fclose(fp);
                    lserrno = LSE_BAD_EXP;
                    return -1;
                }
                if (strcasecmp(word, "localtasks") == 0) {
                    phase = ph_begin;
                    break;
                }
                fclose(fp);
                lserrno = LSE_BAD_EXP;
                return -1;
            }
            minus = false;
            if (strcmp(word, "+") == 0 || strcmp(word, "-") == 0)  {
                minus = (strcmp(word, "-") == 0);
                word = getNextWord_(&line);
                if (word == NULL) {
                    fclose(fp);
                    lserrno = LSE_BAD_EXP;
                    return -1;
                }
            } else if (word[0] == '+' || word[0] == '-') {
                minus = word[0] == '-';
                word++;
            }

            if (minus) {
                if ((deletetask_(word, localList) < 0) && useMinus)
                    (void)inserttask_(word, minusListl);

            } else
                (void)inserttask_(word, localList);
            break;
        }
    }

    if (phase != ph_begin) {
        fclose(fp);
        lserrno = LSE_BAD_TASKF;
        return -1;
    }

    fclose(fp);
    return 0;
}

int
writetaskfile_(char *filename, hTab *minusListl, hTab *minusListr,
               hTab *localList, hTab *remoteList)
{
    char **tlist;
    int  i, num;
    FILE *fp;

    if ((fp = fopen(filename, "w")) == NULL) {

        lserrno = LSE_FILE_SYS;
        return -1;
    }

    fprintf(fp, "Begin LocalTasks\n");
    num = listtask_(&tlist, localList, true);
    for (i=0;i<num;i++)
        fprintf(fp, "+ %s\n", tlist[i]);

    num = listtask_(&tlist, minusListl, true);
    for (i=0;i<num;i++)
        fprintf(fp, "- %s\n", tlist[i]);

    fprintf(fp, "End LocalTasks\n\n");

    fprintf(fp, "Begin RemoteTasks\n");
    num = listtask_(&tlist, remoteList, true);
    for (i=0;i<num;i++)
        fprintf(fp, "+ \"%s\"\n", tlist[i]);

    num = listtask_(&tlist, minusListr, true);
    for (i=0;i<num;i++)
        fprintf(fp, "- %s\n", tlist[i]);

    fprintf(fp, "End RemoteTasks\n\n");
    fclose(fp);
    return 0;

}

int
ls_insertrtask(char *task)
{
    char *sp;
    char  *p;

    if (! listok)
        if (inittasklists_() < 0)
            return -1;

    if ((p=getenv("LSF_TRS")) != NULL)
        sp = strchr(task, *p);
    else
        sp = strchr(task, '/');

    if (sp && expSyntax_(sp+1) < 0) {
        return -1;
    }
    inserttask_(task, &rtask_table);
    return 0;

}

int
ls_insertltask(char *task)
{

    if (! listok)
        if (inittasklists_() < 0)
            return -1;

    inserttask_(task, &ltask_table);
    return 0;

}

void
inserttask_(char *taskstr, hTab *tasktb)
{
    int succ;
    char *task;
    char *resreq;
    hEnt *hEntPtr;
    int *oldcp;
    char  *p;
    int   taskResSep;

    if ((p=getenv("LSF_TRS")) != NULL)
        taskResSep = *p;
    else
        taskResSep = '/';

    task = putstr_(taskstr);
    if ((resreq = strchr(task, taskResSep)) != NULL) {
        *resreq++ = '\0';
        if (*resreq == '\0')
            resreq = NULL;
    }

    hEntPtr = h_addEnt_(tasktb, task, &succ);

    if (!succ) {
        oldcp = hEntPtr->hData;
        hEntPtr->hData = (int *)NULL;
        if (oldcp != (int *)NULL) {
            free(oldcp);
        }
    }

    hEntPtr->hData = ((resreq == NULL) ? NULL : ((int *) putstr_(resreq)));
    free(task);
}

int
ls_deletertask(char *task)
{
    if (! listok)
        if (inittasklists_() <0)
            return -1;

    return (deletetask_(task, &rtask_table));

}

int
ls_deleteltask(char *task)
{
    if (! listok)
        if (inittasklists_() < 0)
            return -1;

    return (deletetask_(task, &ltask_table));

}

int
deletetask_(char *taskstr, hTab *tasktb)
{
    hEnt *hEntPtr;
    char *sp;
    char *task;
    char *p;

    task = putstr_(taskstr);
    if ((p = getenv("LSF_TRS")) != NULL)
        sp = strchr(task, *p);
    else
        sp = strchr(task, '/');

    if (sp != NULL)
        *sp = '\0';

    hEntPtr = h_getEnt_(tasktb, (char *)task);
    if (hEntPtr == (hEnt *)NULL) {
        lserrno = LSE_BAD_ARGS;
        free(task);
        return -1;
    }

    h_delEnt_(tasktb, hEntPtr);

    free(task);
    return 0;

}

int
ls_listrtask(char ***taskList, int sortflag)
{
    if (! listok)
        if (inittasklists_() < 0)
            return -1;

    return (listtask_(taskList, &rtask_table, sortflag));

}

int
ls_listltask(char ***taskList, int sortflag)
{
    if (! listok)
        if (inittasklists_() < 0)
            return -1;

    return (listtask_(taskList, &ltask_table, sortflag));

}

static int tcomp_(const void *tlist1, const void *tlist2);

int
listtask_(char ***taskList, hTab *tasktb, int sortflag)
{
    static char **tlist;
    hEnt *hEntPtr;
    hLinks *hashLinks;
    int  nEntry;
    int  index;
    int  listindex;
    int  tasklen;
    char buf[MAXLINELEN];
    char  *p;

    if ((nEntry = tasktb->numEnts) <= 0) {
        return 0;
    }

    if (tlist != (char **)NULL) {
        for (index = 0; tlist[index]; index++) {
            free(tlist[index]);
        }
        free(tlist);
    }

    tlist = (char **) malloc((nEntry+1) * sizeof(char *));

    for (index = 0, listindex = 0; index < tasktb->size; index++) {
        hashLinks = &(tasktb->slotPtr[index]);
        for ( hEntPtr = (hEnt *) hashLinks->bwPtr;
              hEntPtr != (hEnt *) hashLinks;
              hEntPtr = (hEnt *) ((hLinks *) hEntPtr)->bwPtr) {
            strcpy(buf, hEntPtr->keyname);
            if (hEntPtr->hData != (int *)NULL) {
                tasklen = strlen(buf);

                if ((p=getenv("LSF_TRS")) != NULL)
                    buf[tasklen] = *p;
                else
                    buf[tasklen] = '/';
                strcpy(buf + tasklen + 1, (char *)hEntPtr->hData);
            }

            tlist[listindex] = putstr_(buf);
            listindex++;
        }
    }

    tlist[listindex] = NULL;
    if ( sortflag && listindex != 0 )
        qsort(tlist, listindex, sizeof(char *), tcomp_);
    *taskList = tlist;
    return nEntry;

}

static int
tcomp_(const void *tlist1, const void *tlist2)
{
    return(strcmp(*(char **)tlist1, *(char **)tlist2));
}
