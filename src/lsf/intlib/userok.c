/* $Id: userok.c,v 1.7 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/intlib/libllcore.h"
#include "lsf/lib/lproto.h"
#include "lsf/lim/limout.h"

#define LSF_AUTH            9
#define LSF_USE_HOSTEQUIV   10
#define LSF_ID_PORT         11
#define _USE_TCP_           0x04
extern struct config_param genParams_[];
extern int callLim_(enum limReqCode, void *, bool_t (*)(), void *, bool_t (*)(), char *, int, struct packet_header *);

#define LOOP_ADDR       0x7F000001
#define SIZE 256
#define TIMEOUT 4

struct hostAttribT {
    char*  hostNameP;
    int    hostAttrib;
};

struct clstHostAttribT {
    struct hostAttribT * attrHostsP;
    int numHosts;
    int lasttime;
    int updateInterval;
    char *myClusterNameP;
};

static int getAttribOfHosts(struct clstHostAttribT* hostAttrTableP,
                            int options, int updateIntvl);
static void alarmer_(int);

static struct clstHostAttribT hostAttrLocalClst = {NULL, 0, 0, 0, NULL};

extern int    masterknown_;

char *
auth_user(u_long in, u_short local, u_short remote)
{
    static char fname[] = "auth_user";
    static char ruser[SIZE];
    char rbuf[SIZE];
    char readBuf[SIZE];
    char *bp;
    int s, n, bufsize;
    struct sockaddr_in saddr;
    struct servent *svp;
    u_short rlocal, rremote;
    static u_short id_port;
    char   *authd_port;
    struct itimerval old_itimer;
    unsigned int oldTimer;
    sigset_t newMask, oldMask;
    struct sigaction action, old_action;

    if (id_port == 0) {
        authd_port = genParams_[LSF_ID_PORT].paramValue;
        if (authd_port != NULL) {
            if ((id_port = atoi(authd_port)) == 0) {
                ls_syslog(LOG_ERR,
                          "%s: LSF_ID_PORT in lsf.conf must be positive number",
                          fname);
                return NULL;
            }
            id_port = htons(id_port);
        } else {
            svp = getservbyname("ident", "tcp");
            if (! svp) {
                ls_syslog(LOG_ERR,
                          "%s: %s(%s/tcp failed: %m",
                          fname, "getservbyname", "ident");
                return NULL;
            }
            id_port = svp->s_port;
        }
    }

    if ((s = socket(AF_INET,SOCK_STREAM,0)) == -1)
        return 0;

    saddr.sin_family = AF_INET;

    saddr.sin_port = id_port;

    saddr.sin_addr.s_addr = in;

    ls_syslog(LOG_DEBUG,"%s: Calling for authentication at <%s>, port:<%d>",
              fname, inet_ntoa(saddr.sin_addr),id_port);

    if (b_connect_(s,(struct sockaddr *)&saddr,
                   sizeof(struct sockaddr_in), TIMEOUT) == -1) {
        int realerrno = errno;
        close(s);
        errno = realerrno;
        return 0;
    }

    if (getitimer(ITIMER_REAL, &old_itimer) < 0)
        return 0;

    action.sa_flags = 0;
    action.sa_handler = alarmer_;

    sigfillset(&action.sa_mask);
    sigaction(SIGALRM, &action, &old_action);

    bp = rbuf;
    sprintf(bp,"%u , %u\r\n",(unsigned int) remote,(unsigned int) local);
    bufsize = strlen(bp);

    blockSigs_(SIGALRM, &newMask, &oldMask);

    oldTimer = alarm(TIMEOUT);

    while ((n = write(s, bp, bufsize)) < bufsize) {
        if (n <= 0) {
            close(s);
            if (errno == EINTR) {
                errno = ETIMEDOUT;
            }
            alarm(oldTimer);
            setitimer(ITIMER_REAL, &old_itimer, NULL);
            sigaction(SIGALRM, &old_action, NULL);
            sigprocmask(SIG_SETMASK, &oldMask, NULL);
            return 0;
        } else {
            bp += n;
            bufsize -= n;
        }
    }

    alarm(TIMEOUT);
    bp = rbuf;
    n = read(s, readBuf, SIZE);
    if (n > 0) {
        int i;
        readBuf[n] = '\0';
        for (i = 0; i <= n ; i++) {
            if ((readBuf[i] != ' ') && (readBuf[i] != '\t') && (readBuf[i] != '\r'))
                *bp++ = readBuf[i];
            if ((bp - rbuf == sizeof(rbuf) - 1) || (readBuf[i] == '\n'))
                break;
        }
        *bp = '\0';
    }
    close(s);

    if (n <= 0) {
        if (errno == EINTR) {
            errno = ETIMEDOUT;
        }
        alarm(oldTimer);
        setitimer(ITIMER_REAL, &old_itimer, NULL);
        sigaction(SIGALRM, &old_action, NULL);
        sigprocmask(SIG_SETMASK, &oldMask, NULL);
        return 0;
    }

    alarm(oldTimer);
    setitimer(ITIMER_REAL, &old_itimer, NULL);
    sigaction(SIGALRM, &old_action, NULL);
    sigprocmask(SIG_SETMASK, &oldMask, NULL);

    if (logclass & LC_AUTH)
        ls_syslog(LOG_DEBUG,"%s Authentication buffer (rbuf=<%s>)",fname,rbuf);

    if (sscanf(rbuf, "%hd,%hd: USERID :%*[^:]:%s",
               &rremote,&rlocal,ruser) < 3) {
        ls_syslog(LOG_ERR,
                  "%s: Authentication data format error rbuf=<%s> from %s",
                  fname, rbuf, sockAdd2Str_(&saddr));
        return 0;
    }
    if ((remote != rremote) || (local != rlocal)) {
        ls_syslog(LOG_ERR,
                  "%s: Authentication port mismatch remote=%d, local=%d, rremote=%d, rlocal=%d from %s",
            fname, remote, local, rlocal, rremote, sockAdd2Str_(&saddr));
        return 0;
    }

    return ruser;
}


int
userok(int s, struct sockaddr_in *from, char *hostname,
       struct sockaddr_in *localAddr, struct lsfAuth *auth, int debug)
{
    static char fname[] = "userok";
    unsigned short      remote;
    char                savedUser[MAXHOSTNAMELEN];
    int                 user_ok;
    char                *authKind;

    if (debug) {
        char lsfUserName[MAXLSFNAMELEN];

        if (getpwnam2(lsfUserName) == NULL) {
            return false;
        }
        if (strcmp(lsfUserName, auth->lsfUserName) != 0) {
            ls_syslog(LOG_ERR,
                      "%s: %s rejected in single user mode",
                      fname, auth->lsfUserName);
            return false;
        }
    }

    authKind = genParams_[LSF_AUTH].paramValue;
    if (authKind != NULL) {
        if (strcmp(authKind, AUTH_PARAM_EAUTH))
            authKind = NULL;
    }

    if (from->sin_family != AF_INET) {
        ls_syslog(LOG_ERR, "%s: sin_family != AF_INET", fname);
        return false;
    }

    remote = ntohs(from->sin_port);

    authKind = genParams_[LSF_AUTH].paramValue;
    if (authKind == NULL) {
        if (!debug) {
            if (remote >= IPPORT_RESERVED || remote < IPPORT_RESERVED/2) {
                ls_syslog(LOG_ERR,
                          "%s: Request from bad port %d, denied",
                          fname, remote);
                return false;
            }
        }
    } else {
        if (strcmp(authKind, AUTH_IDENT) == 0) {
            struct sockaddr_in  saddr;
            socklen_t size = sizeof(struct sockaddr_in);
            char *user;
            unsigned short      local;

            if (remote >= IPPORT_RESERVED || remote < IPPORT_RESERVED/2) {

                if (getsockname(s, (struct sockaddr *)&saddr, &size) == -1) {
                    return false;
                }
                local = ntohs(saddr.sin_port);

                if (getpeername(s, (struct sockaddr *)&saddr, &size) == -1) {
                    return false;
                }

                user = auth_user(saddr.sin_addr.s_addr,local,remote);
                if (!user) {
                    if (errno == ETIMEDOUT) {
                        ls_syslog(LOG_INFO,
                                  "%s: auth_user %s returned NULL, retrying",
                                  fname, sockAdd2Str_(from));

                        millisleep_(500);
                        user = auth_user(from->sin_addr.s_addr, local, remote);
                        if (!user) {
                            ls_syslog(LOG_ERR,
                                      "%s: auth_user %s retry failed: %m",
                                      fname, sockAdd2Str_(from));
                            return false;
                        } else {
                            ls_syslog(LOG_INFO,
                                      "%s: auth_user %s retry succeeded",
                                      fname, sockAdd2Str_(from));
                        }
                    } else {
                        ls_syslog(LOG_INFO, "%s: auth_user %s returned NULL",
                                  fname, sockAdd2Str_(from));
                        return false;
                    }
                } else {
                    if (strcmp(user, auth->lsfUserName) != 0) {
                        ls_syslog(LOG_ERR,
                                  "%s: Forged username suspected from %s: %s/%s",
                                  fname, sockAdd2Str_(from), auth->lsfUserName, user);
                        return false;
                    }
                }
            }
        } else {
            if (!strcmp(authKind, AUTH_PARAM_EAUTH)) {
                if (auth->kind != CLIENT_EAUTH) {
                    ls_syslog(LOG_ERR,
                              "%s: Client %s is not using <%d/eauth> authentication",
                              fname, sockAdd2Str_(from), (int) auth->kind);
                    return false;
                }

                if (verifyEAuth_(auth, from) == -1) {
                    ls_syslog(LOG_ERR,
                              "%s: %s authentication failed for %s/%s",
                              fname, "eauth", auth->lsfUserName, sockAdd2Str_(from));
                    return false;
                }
            } else {
                ls_syslog(LOG_ERR,
                          "%s: Unkown authentication type <%d> from %s/%s; denied",
                          fname, auth->kind, auth->lsfUserName, sockAdd2Str_(from));
                return false;
            }

        }
    }

    if (genParams_[LSF_USE_HOSTEQUIV].paramValue) {
        strcpy(savedUser, auth->lsfUserName);
        user_ok = ruserok(hostname, 0, savedUser, savedUser);
        if (user_ok == -1) {
            return false;
        }
    }

    return true;

}

int
hostOk(char *fromHost, int options)
{
    if(getAttribOfHosts(&hostAttrLocalClst, LOCAL_ONLY,
                        LOCAL_HATTRIB_UPDATE_INTERVAL ) < 0) {
        return -1;
    }

    if (hostAttrLocalClst.attrHostsP == NULL || fromHost == NULL)
        return -1;

    return 1;
}

int
hostIsLocal(char *hname)
{
    int ii;

    if(getAttribOfHosts(&hostAttrLocalClst, LOCAL_ONLY | NEED_MY_CLUSTER_NAME,
                        LOCAL_HATTRIB_UPDATE_INTERVAL) < 0) {
        return false;
    }

    if(hostAttrLocalClst.myClusterNameP) {
        if (strcmp(hname, hostAttrLocalClst.myClusterNameP) == 0) {
            return true;
        }
    }

    for (ii = 0; ii < hostAttrLocalClst.numHosts; ii++) {
        if (strcasecmp(hname,
                       hostAttrLocalClst.attrHostsP[ii].hostNameP) == 0) {
            return true;
        }
    }

    return false;
}

static int
getAttribOfHosts(struct clstHostAttribT* hostAttrTableP, int options,
                 int updateIntvl)
{
    static char fname[] = "getAttribOfHosts";
    struct hostInfo *hostList;
    struct clstHostAttribT lastHAttrLst =
    {NULL, 0, 0, 0, NULL};
    int thistime = 0;
    int ii;
    char *ptr;
    int putstrFailed = 0;

    if ((options & NEED_MY_CLUSTER_NAME) &&
        hostAttrTableP->myClusterNameP == NULL) {
        if ( (ptr = ls_getclustername()) == NULL ) {
            ls_syslog(LOG_ERR, "%s: ls_getclustername: %M", fname);
            return -1;
        }
        hostAttrTableP->myClusterNameP = putstr_(ptr);
        if(hostAttrTableP->myClusterNameP == NULL) {
            return -1;
        }
    }

    thistime = time(0);
    if (thistime >= hostAttrTableP->lasttime + updateIntvl ||
        hostAttrTableP->attrHostsP == NULL) {

        memcpy(&lastHAttrLst, hostAttrTableP, sizeof(struct clstHostAttribT));

        hostList = ls_gethostinfo("-", &hostAttrTableP->numHosts, NULL, 0,
                                  LOCAL_ONLY);
        if (hostList != NULL) {

            hostAttrTableP->attrHostsP =
                (struct hostAttribT *)calloc(hostAttrTableP->numHosts,
                                             sizeof(struct hostAttribT));
            if (hostAttrTableP->attrHostsP == NULL) {

                ls_syslog(LOG_DEBUG,
                          "%s: calloc() failed, use the last list: %M", fname);
                memcpy(hostAttrTableP, &lastHAttrLst,
                       sizeof(struct clstHostAttribT));
            } else {
                for (ii=0; ii<hostAttrTableP->numHosts; ii++) {
                    if((hostAttrTableP->attrHostsP[ii].hostNameP =
                        putstr_(hostList[ii].hostName)) == NULL) {
                        putstrFailed = 1;
                    }
                    if(hostList[ii].isServer) {
                        hostAttrTableP->attrHostsP[ii].hostAttrib |=
                            HOST_ATTR_SERVER;
                    } else {
                        hostAttrTableP->attrHostsP[ii].hostAttrib |=
                            HOST_ATTR_CLIENT;
                    }
                }
                if(putstrFailed == 0) {

                    if(lastHAttrLst.attrHostsP != NULL) {
                        for (ii=0; ii<lastHAttrLst.numHosts; ii++)
                            free(lastHAttrLst.attrHostsP[ii].hostNameP);
                        free(lastHAttrLst.attrHostsP);
                    }
                } else {

                    for (ii=0; ii<hostAttrTableP->numHosts; ii++) {
                        if (hostAttrTableP->attrHostsP[ii].hostNameP != NULL)
                            free(hostAttrTableP->attrHostsP[ii].hostNameP);
                    }
                    free(hostAttrTableP->attrHostsP);

                    memcpy(hostAttrTableP, &lastHAttrLst,
                           sizeof(struct clstHostAttribT));
                }
            }
            hostAttrTableP->lasttime = thistime;
        } else {

            if (hostAttrTableP->attrHostsP == NULL) {
                ls_syslog(LOG_INFO, "%s: ls_gethostinfo: %M", fname);
                return -1;
            } else {
                ls_syslog(LOG_DEBUG, "%s: ls_gethostinfo: %M", fname);
            }
        }
    }

    if (hostAttrTableP->attrHostsP == NULL) {
        ls_syslog(LOG_ERR,
                  "%s: attrHostsP is still NULL when returning ", fname);
        return -1;
    }

    return 0;
}

static void
alarmer_(int s)
{
}
