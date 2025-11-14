/* $Id: cmd.bhc.c,v 1.3 2007/08/15 22:18:44 tmizan Exp $
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

#include "lsbatch/cmd/cmd.h"

extern char *myGetOpt(int nargc, char **nargv, char *ostr);
extern int getConfirm(char *);

static void ctrlHost(char *, int, int);
static int doConfirm(int, char *);
static int exitrc;
static char *opStr;
static struct hostInfoEnt *getHostList(int *numHosts, char **inputHosts);

int bhc(int argc, char *argv[], int opCode)
{
    extern int optind;
    struct hostInfoEnt *hostInfo;
    char **hostPoint;
    char **hosts = NULL;
    char *optName;
    int i, numReq;
    int fFlag = FALSE;
    int all = FALSE, numHosts = 0;
    int inquerFlag = FALSE;

    while ((optName = myGetOpt(argc, argv, "f|")) != NULL) {
        switch (optName[0]) {
        case 'f':
            fFlag = TRUE;
            break;
        default:
            return (-2);
        }
    }
    switch (opCode) {
    case HOST_OPEN:
        opStr = ("Open");
        break;
    case HOST_CLOSE:
        opStr = ("Close");
        break;
    case HOST_REBOOT:
        opStr = ("Restart slave batch daemon on");
        break;
    case HOST_SHUTDOWN:
        opStr = ("Shut down slave batch daemon on");
        break;
    default:
        fprintf(stderr, ("Unknown operation code\n"));
        exit(-1);
    }

    exitrc = 0;
    numHosts = getNames(argc, argv, optind, &hosts, &all, "hostC");
    numReq = numHosts;
    hostPoint = NULL;
    if (!numHosts && !all)
        numHosts = 1;
    else if (numHosts)
        hostPoint = hosts;

    if ((opCode == HOST_REBOOT || opCode == HOST_SHUTDOWN) &&
        !(numHosts == 0 && all)) {
        if ((hostInfo = getHostList(&numHosts, hostPoint)) == NULL)
            return (-1);
    } else {
        if ((hostInfo = lsb_hostinfo(hostPoint, &numHosts)) == NULL) {
            lsb_perror(NULL);
            return (-1);
        }
    }

    if (!fFlag && all && (opCode == HOST_REBOOT || opCode == HOST_SHUTDOWN))
        inquerFlag = !doConfirm(opCode, NULL);

    for (i = 0; i < numHosts; i++) {
        if (strcmp(hostInfo[i].host, "lost_and_found") == 0 &&
            (opCode == HOST_REBOOT || opCode == HOST_SHUTDOWN)) {
            if (!all)
                fprintf(stderr,
                        ("<lost_and_found> is not a real host, ignored\n"));
            continue;
        }
        if (inquerFlag && !(doConfirm(opCode, hostInfo[i].host)))
            continue;

        fprintf(stderr, "%s <%s> ...... ", opStr, hostInfo[i].host);

        ctrlHost(hostInfo[i].host, hostInfo[i].hStatus, opCode);
    }
    return (exitrc);
}

static void ctrlHost(char *host, int hStatus, int opCode)
{
    if (lsb_hostcontrol(host, opCode) < 0) {
        char i18nBuf[100];
        sprintf(i18nBuf, "Failed: %s", "Host control");
        lsb_perror(i18nBuf);
        exitrc = -1;
        return;
    }
    if (opCode == HOST_OPEN) {
        if (hStatus &
            (HOST_STAT_BUSY | HOST_STAT_WIND | HOST_STAT_LOCKED_MASTER |
             HOST_STAT_LOCKED | HOST_STAT_FULL)) {
            fprintf(stderr, ("done : host remains closed due to "));
            if (hStatus & HOST_STAT_LOCKED)
                fprintf(stderr, ("being locked; "));
            else if (hStatus & HOST_STAT_LOCKED_MASTER)
                fprintf(stderr, ("being locked by master LIM;"));
            else if (hStatus & HOST_STAT_WIND)
                fprintf(stderr, ("dispatch window; "));
            else if (hStatus & HOST_STAT_FULL)
                fprintf(stderr, ("job limit; "));
            else if (hStatus & HOST_STAT_BUSY)
                fprintf(stderr, ("load threshold; "));
            fprintf(stderr, " \n");
            return;
        }
    }

    fprintf(stderr, ("done\n"));
}

static int doConfirm(int opCode, char *host)
{
    char msg[MAXLINELEN];

    if (host == NULL)
        host = ("all the hosts");

    sprintf(msg, "\n%s %s? [y/n] ", opStr, host);

    return (getConfirm(msg));
}

static struct hostInfoEnt *getHostList(int *numHosts, char **inputHosts)
{
    static struct hostInfoEnt *hostInfo = NULL;
    int i;
    char *localHost;

    FREEUP(hostInfo);

    if ((hostInfo = calloc(*numHosts + 1, sizeof(struct hostInfoEnt))) ==
        NULL) {
        perror("calloc");
        return (NULL);
    }

    if (inputHosts) {
        for (i = 0; i < *numHosts; i++)
            hostInfo[i].host = inputHosts[i];
    } else {
        if ((localHost = ls_getmyhostname()) == NULL)
            hostInfo[0].host = "localhost";
        else
            hostInfo[0].host = localHost;
    }

    return hostInfo;
}
