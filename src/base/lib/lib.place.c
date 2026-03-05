/* $Id: lib.place.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

char **ls_placereq(char *resreq, int *numhosts, int options, char *fromhost)
{
    struct decisionReq placeReq;
    int *num;
    int one = 1;

    if (!numhosts)
        num = &one;
    else
        num = numhosts;

    placeReq.ofWhat = OF_ANY;
    placeReq.options = options;
    strcpy(placeReq.hostType, " ");
    placeReq.numHosts = *num;

    if (!*num) {
        placeReq.numHosts = 9999;
        placeReq.options &= ~EXACT;
    }
    placeReq.numPrefs = 0;

    return placement_(resreq, &placeReq, fromhost, num);
}

char **ls_placeofhosts(char *resreq, int *numhosts, int options, char *fromhost,
                       char **hostlist, int listsize)
{
    struct decisionReq placeReq;
    int *num;
    int one = 1;

    if (!listsize || !hostlist) {
        lserrno = LSE_BAD_ARGS;
        return NULL;
    }

    if (!numhosts)
        num = &one;
    else
        num = numhosts;

    placeReq.ofWhat = OF_HOSTS;
    placeReq.options = options;
    strcpy(placeReq.hostType, " ");
    placeReq.numHosts = *num;

    if (!*num) {
        placeReq.numHosts = listsize;
        placeReq.options &= ~EXACT;
    }

    placeReq.numPrefs = listsize + 1;
    placeReq.preferredHosts = calloc(placeReq.numPrefs, sizeof(char *));
    if (placeReq.preferredHosts == NULL) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    int i;
    for (i = 1; i < placeReq.numPrefs; i++) {
        placeReq.preferredHosts[i] = strdup(hostlist[i - 1]);
    }

    int j;
    if (i < placeReq.numPrefs) {
        for (j = 1; j < i; j++)
            free(placeReq.preferredHosts[j]);
        free(placeReq.preferredHosts);
        return NULL;
    }

    return placement_(resreq, &placeReq, fromhost, num);
}

char **placement_(char *resReq, struct decisionReq *placeReqPtr, char *fromhost,
                  int *numhosts)
{
    static struct placeReply placeReply;
    static char **hostnames;
    int numnames, i, j, k;
    char *hname;

    if (initenv_(NULL, NULL) < 0)
        return NULL;

    if (placeReqPtr->numHosts <= 0) {
        lserrno = LSE_BAD_ARGS;
        goto error;
    }

    if (!fromhost) {
        if ((hname = ls_getmyhostname()) == NULL)
            goto error;
        placeReqPtr->preferredHosts[0] = putstr_(hname);
    } else
        placeReqPtr->preferredHosts[0] = fromhost;

    if (placeReqPtr->preferredHosts[0] == NULL) {
        lserrno = LSE_MALLOC;
        goto error;
    }

    if (resReq != NULL) {
        strcpy(placeReqPtr->resReq, resReq);
    } else
        placeReqPtr->resReq[0] = '\0';

    if (callLim_(LIM_PLACEMENT, placeReqPtr, xdr_decisionReq, &placeReply,
                 xdr_placeReply, NULL, _USE_TCP_, NULL) < 0)
        goto error;

    for (i = 0; i < placeReqPtr->numPrefs; i++)
        FREEUP(placeReqPtr->preferredHosts[i]);
    FREEUP(placeReqPtr->preferredHosts);
    *numhosts = placeReply.numHosts;

    for (i = 0, numnames = 0; i < *numhosts; i++)
        numnames += placeReply.placeInfo[i].numtask;

    if (hostnames)
        free(hostnames);
    hostnames = calloc(numnames, sizeof(char *));
    if (hostnames == NULL) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    for (i = 0, j = 0; i < *numhosts; i++)
        for (k = 0; k < placeReply.placeInfo[i].numtask; k++)
            hostnames[j++] = placeReply.placeInfo[i].hostName;

    *numhosts = numnames;
    return hostnames;

error:
    for (i = 0; i < placeReqPtr->numPrefs; i++)
        FREEUP(placeReqPtr->preferredHosts[i]);
    FREEUP(placeReqPtr->preferredHosts);
    return NULL;
}
