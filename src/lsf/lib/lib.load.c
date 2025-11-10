/* $Id: lib.load.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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

#define LOAD_INFO_THRESHOLD 75

struct hostLoad *loadinfo_(char *resReq,
                           struct decisionReq *loadReqPtr,
                           char *fromhost,
                           int *numHosts,
                           char ***outnlist);

static char *namestofilter_(char **);

extern int sharedResConfigured_;
static char *
namestofilter_(char **indxs)
{
    int i, len;
    static char *filter;

    if (filter)
        free(filter);

    for (i = 0, len = 0; indxs[i]; i++)
        len += strlen(indxs[i]);

    if (!len || len > MAXLINELEN) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }

    len += (i + 1);

    if ( (filter = malloc(len)) == NULL) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    filter[0]='\0';
    for (i = 0; indxs[i]; i++)  {
        strcat(filter, indxs[i]);
        strcat(filter, ":");
    }
    return filter;
}

struct hostLoad *
ls_loadinfo(char *resreq,
            int *numhosts,
            int options,
            char *fromhost,
            char **hostlist,
            int listsize,
            char ***indxnamelist)
{
    static char fname[] = "ls_loadinfo";
    struct decisionReq loadReq;
    char *indexfilter;
    int i, j, *num;
    int one = 1;
    char  tempresreq[MAXLINELEN];

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (!indxnamelist) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }
    if (hostlist && listsize)
        loadReq.ofWhat = OF_HOSTS;
    else
        loadReq.ofWhat = OF_ANY;

    loadReq.options = options;
    strcpy(loadReq.hostType, " ");

    if (numhosts == 0)
        num = &one;
    else
        num = numhosts;

    loadReq.numHosts = *num;

    if (! *num) {
        if (loadReq.ofWhat == OF_HOSTS)
            loadReq.numHosts = listsize;
        else
            loadReq.numHosts = 9999;
        loadReq.options &= ~EXACT;
    }

    if (loadReq.ofWhat == OF_HOSTS) {
        char clusterinlist=0;

        loadReq.numPrefs = listsize + 1;
        loadReq.preferredHosts = (char **)
            calloc(loadReq.numPrefs, sizeof (char *));
        if (loadReq.preferredHosts == NULL) {
            lserrno = LSE_MALLOC;
            return NULL;
        }

        for (i = 1; i < loadReq.numPrefs; i++) {

            loadReq.preferredHosts[i] = strdup(hostlist[i-1]);
            clusterinlist = 1;
            if (loadReq.preferredHosts[i] == NULL) {
                for (j = 1; j < i; j++)
                    free(loadReq.preferredHosts[j]);
                lserrno = LSE_MALLOC;
                break;
            }
        }

        if (i < loadReq.numPrefs) {
            for (j = 1; j < i; j++)
                free(loadReq.preferredHosts[j]);
            free(loadReq.preferredHosts);
            return NULL;
        }

        if (*num > listsize && (loadReq.options & EXACT) && !clusterinlist) {
            lserrno = LSE_NO_HOST;
            for (j = 1; j < i; j++)
                free(loadReq.preferredHosts[j]);
            free(loadReq.preferredHosts);
            return NULL;
        }

        if (clusterinlist && !*num) {
            loadReq.numHosts = 9999;
            loadReq.options &= ~EXACT;
        }

    } else {

        loadReq.numPrefs = 1;
        loadReq.preferredHosts = calloc(loadReq.numPrefs, sizeof(char *));
        if (loadReq.preferredHosts == NULL) {
            lserrno = LSE_MALLOC;
            return NULL;
        }
    }

    if (*indxnamelist) {
        indexfilter = namestofilter_(*indxnamelist);
        if (!indexfilter)
            return NULL;
    } else
        indexfilter = NULL;

    if (resreq && indexfilter) {
        if ((strlen(resreq) + strlen(indexfilter)) < MAXLINELEN - 8) {
            char tmp[MAXLINELEN/2];
            sprintf(tmp,"filter[%s]", indexfilter);
            strcpy(tempresreq, resreq);
            strcat(tempresreq, tmp);
            resreq = tempresreq;
        } else {
            lserrno = LSE_BAD_ARGS;
            return NULL;
        }
    } else if (indexfilter) {
        sprintf(tempresreq,"filter[%s]",indexfilter);
        resreq = tempresreq;
    }

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "\
%s: loadReq.ofWhat=%d loadReq.numHosts=%d loadReq..numPrefs=%d",
                  __func__, loadReq.ofWhat, loadReq.numHosts, loadReq.numPrefs);

    return loadinfo_(resreq, &loadReq, fromhost, num, indxnamelist);
}

struct hostLoad *
ls_load(char *resreq, int *numhosts, int options, char *fromhost)
{
    char **dummynl = NULL;

    return ls_loadinfo(resreq,
                       numhosts,
                       options,
                       fromhost,
                       NULL,
                       0,
                       &dummynl);

}

struct hostLoad *
ls_loadofhosts(char *resreq,
               int *numhosts,
               int options,
               char *fromhost,
               char **hostlist,
               int listsize)
{
    char **dummynl = NULL;

    return ls_loadinfo(resreq,
                       numhosts,
                       options,
                       fromhost,
                       hostlist,
                       listsize,
                       &dummynl);
}

struct hostLoad *
loadinfo_(char *resReq,
          struct decisionReq *loadReqPtr,
          char *fromhost,
          int *numHosts,
          char ***outnlist)
{
    static char fname[] = "loadinfo_";
    static struct loadReply loadReply;
    int i;
    char *hname;
    int options = 0;

    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (loadReqPtr->numHosts <=0) {
        lserrno = LSE_BAD_ARGS;
        goto error;
    }

    if (!fromhost) {
        if ((hname = ls_getmyhostname()) == NULL)
            goto error;
        loadReqPtr->preferredHosts[0] = putstr_(hname);
    } else
        loadReqPtr->preferredHosts[0] = putstr_(fromhost);

    if (loadReqPtr->preferredHosts[0] == NULL) {
        lserrno = LSE_MALLOC;
        goto error;
    }

    if (resReq) {
        strncpy(loadReqPtr->resReq, resReq, MAXLINELEN);
    } else
        strcpy(loadReqPtr->resReq, " ");

    loadReqPtr->resReq[MAXLINELEN-1] = '\0';
    if (loadReqPtr->ofWhat == OF_HOSTS && loadReqPtr->numPrefs == 2
        && loadReqPtr->numHosts == 1
        && equal_host(loadReqPtr->preferredHosts[1],
                      loadReqPtr->preferredHosts[0]))
        options |= _LOCAL_;
    else
        options |= _USE_TCP_;

    if (callLim_(LIM_LOAD_REQ, loadReqPtr, xdr_decisionReq,
                 &loadReply, xdr_loadReply, NULL, options, NULL) < 0)
        goto error;
    if (loadReply.flags & LOAD_REPLY_SHARED_RESOURCE) {
        sharedResConfigured_ = true;
    }
    for (i=0; i<loadReqPtr->numPrefs; i++)
        FREEUP(loadReqPtr->preferredHosts[i]);
    FREEUP (loadReqPtr->preferredHosts);
    *numHosts = loadReply.nEntry;
    *outnlist = loadReply.indicies;
    return (loadReply.loadMatrix);

error:
    for (i=0; i<loadReqPtr->numPrefs; i++)
        FREEUP(loadReqPtr->preferredHosts[i]);
    FREEUP (loadReqPtr->preferredHosts);
    return NULL;

}

int
ls_loadadj(char *resreq, struct placeInfo *placeinfo, int listsize)
{
    struct jobXfer loadadjReq;

    if (listsize <= 0 || placeinfo == NULL) {
        lserrno = LSE_BAD_ARGS;
        return -1;
    }

    loadadjReq.numHosts = listsize;
    loadadjReq.placeInfo = placeinfo;

    if (resreq != NULL) {
        strcpy(loadadjReq.resReq, resreq);
    } else
        loadadjReq.resReq[0] = 0;

    if (callLim_(LIM_LOAD_ADJ,
                 &loadadjReq,
                 xdr_jobXfer,
                 NULL,
                 NULL,
                 NULL,
                 _USE_TCP_,
                 NULL) < 0) {
        return -1;
    }

    return 0;
}
