/* $Id: lib.conn.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"

hTab conn_table;

typedef struct _hostSock {
    int   socket;
    char *hostname;
    struct _hostSock *next;
} HostSock;
static HostSock *hostSock;

#define MAXCONNECT 1024
static struct connectEnt connlist[MAXCONNECT];
static char   *connnamelist[MAXCONNECT+1];

int cli_nios_fd[2] = {-1, -1};

void hostIndex_(char *hostName, int sock);
extern int chanSock_(int chfd);
int delhostbysock_(int sock);

void
inithostsock_(void)
{
    hostSock = NULL;
}

void
initconntbl_(void)
{
    h_initTab_(&conn_table, 0);
}

int
connected_(char *hostName, int sock1, int sock2, int seqno)
{
    int new;
    hEnt *hEntPtr;
    int *sp;

    hEntPtr = h_addEnt_(&conn_table, hostName, &new);
    if (!new) {
        sp = hEntPtr->hData;
    } else {
        sp = calloc(3, sizeof(int));
        sp[0] = -1;
        sp[1] = -1;
        sp[2] = -1;
    }

    if (sock1 >= 0) {
        sp[0] = sock1;
        hostIndex_(hEntPtr->keyname, sock1);
    }

    if (sock2 >= 0)
        sp[1] = sock2;

    if (seqno >= 0)
        sp[2] = seqno;

    hEntPtr->hData = (int *)sp;

    return 0;
}

void
hostIndex_(char *hostName, int sock)
{

    HostSock *newSock;

    newSock = (HostSock *)malloc(sizeof(HostSock));
    if (newSock == NULL) {
        ls_syslog(LOG_ERR, "hostIndex_ : malloc HostSock failed");
        /* Bug. Abort
         */
        abort();
        exit(-1);
    }
    newSock->socket = sock;
    newSock->hostname = hostName;
    newSock->next = hostSock;
    hostSock = newSock;

}

int
delhostbysock_(int sock)
{
    HostSock *tmpSock;

    tmpSock = hostSock;

    if (tmpSock->socket == sock) {
        hostSock = hostSock->next;
        free(tmpSock);
        return 0;
    }

    while (tmpSock->next != NULL) {
        if (tmpSock->next->socket == sock) {
            HostSock *rmSock = tmpSock->next;
            tmpSock->next = rmSock->next;
            free(rmSock);
            return 0;
        }
        tmpSock = tmpSock->next;
    }

    return -1;
}

int
gethostbysock_(int sock, char *hostName)
{
    HostSock *tmpSock;

    if (hostName == NULL) {
        return -1;
    }

    tmpSock = hostSock;

    while (tmpSock != NULL) {
        if (tmpSock->socket == sock) {
            if (tmpSock->hostname != NULL) {
                strcpy(hostName, tmpSock->hostname);
                return 0;
            }
        }
        tmpSock = tmpSock->next;
    }

    strcpy(hostName, "LSF_HOST_NULL");
    return -1;

}

int *
_gethostdata_(char *hostName)
{
    hEnt *hEntPtr;
    int *sp;
    const char *officialName;
    char official[MAXHOSTNAMELEN];

    officialName = getHostOfficialByName_(hostName);
    if (!officialName)
        return ((int *)NULL);

    strcpy(official, officialName);
    hEntPtr = h_getEnt_(&conn_table, official);
    if (hEntPtr == (hEnt *)NULL)
        return ((int *)NULL);

    if ((sp = (int *)hEntPtr->hData) == (int *)NULL) {
        return ((int *)NULL);
    }
    return sp;
}
int
_isconnected_(char *hostName, int *sock)
{
    int *sp;
    sp = _gethostdata_(hostName);
    if (sp == NULL)
        return false;

    sock[0] = sp[0];
    sock[1] = sp[1];
    return true;

}

int
_getcurseqno_(char *hostName)
{
    int *sp;

    sp = _gethostdata_(hostName);
    if (sp == NULL)
        return -1;

    return(sp[2]);
}

void
_setcurseqno_(char *hostName, int seqno)
{
    int *sp;

    sp = _gethostdata_(hostName);
    if (sp == NULL)
        return;

    sp[2] = seqno;
}

int
ls_isconnected(char *hostName)
{
    hEnt *hEntPtr;
    const char *officialName;
    char official[MAXHOSTNAMELEN];

    officialName = getHostOfficialByName_(hostName);
    if (!officialName)
        return false;

    strcpy(official, officialName);
    hEntPtr = h_getEnt_(&conn_table, official);
    if (hEntPtr == (hEnt *)NULL)
        return false;

    return true;

}

int
getConnectionNum_(char *hostName)
{
    hEnt *hEntPtr;
    const char *officialName;
    char official[MAXHOSTNAMELEN];
    int connNum;

    officialName = getHostOfficialByName_(hostName);
    if (!officialName)
        return -1;

    strcpy(official, officialName);
    if ((hEntPtr = h_getEnt_(&conn_table, official)) == NULL)
        return -1;

    connNum = hEntPtr->hData[0];
    delhostbysock_(connNum);
    h_delEnt_(&conn_table, hEntPtr);
    return connNum;

}

int
_findmyconnections_(struct connectEnt **connPtr)
{
    int n = 0;
    sTab hashSearchPtr;
    hEnt *hEntPtr;

    hEntPtr = h_firstEnt_(&conn_table,&hashSearchPtr);
    if (hEntPtr == (hEnt *)NULL) {
        return 0;
    }

    while (hEntPtr) {
        connlist[n].hostname = hEntPtr->keyname;
        connlist[n].csock[0] = hEntPtr->hData[0];
        connlist[n].csock[1] = hEntPtr->hData[1];
        hEntPtr = h_nextEnt_(&hashSearchPtr);
        n++;
    }

    *connPtr = connlist;
    return n;

}

char **
ls_findmyconnections(void)
{
    int n = 0;
    sTab hashSearchPtr;
    hEnt *hEntPtr;

    hEntPtr = h_firstEnt_(&conn_table, &hashSearchPtr);

    while (hEntPtr) {
        connnamelist[n] = hEntPtr->keyname;
        hEntPtr = h_nextEnt_(&hashSearchPtr);
        n++;
    }
    connnamelist[n] = NULL;

    return connnamelist;
}
