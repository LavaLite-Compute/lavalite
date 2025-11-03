/* $Id: lib.eauth.c,v 1.6 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.h"

#define exit(a) _exit(a)

static int getEAuth(struct eauth *, char *);
static char *getLSFAdmin(void);

int
getAuth_(struct lsfAuth *auth, char *host)
{
    auth->uid = getuid();

    struct passwd *pwd = getpwuid2(auth->uid);
    if (pwd == NULL) {
        lserrno = LSE_BADUSER;
        return -1;
    }

    strcpy(auth->lsfUserName, pwd->pw_name);
    auth->gid = getgid();
    auth->kind = CLIENT_EAUTH;

    return getEAuth(&auth->k.eauth, host);
}

#define EAUTHNAME "eauth"

static int
getEAuth(struct eauth *eauth, char *host)
{
    char *argv[4];
    char path[PATH_MAX];
    struct lenData ld;

    sprintf(path, "%s/%s", genParams_[LSF_SERVERDIR].paramValue, EAUTHNAME);
    argv[0] = path;
    argv[1] = "-c";
    argv[2] = host;
    argv[3] = NULL;

    if (runEClient_(&ld, argv) == -1) {
        lserrno = LSE_EAUTH;
        return -1;
    }

    if (ld.len == 0) {
        FREEUP(ld.data);
        lserrno = LSE_EAUTH;
        return -1;
    }

    if (ld.len > LL_BUFSIZ_4K) {
        FREEUP(ld.data);
        lserrno = LSE_EAUTH;
        return -1;
    }

    memcpy(eauth->data, ld.data, ld.len);
    eauth->data[ld.len] = '\0';
    eauth->len = ld.len;

    FREEUP(ld.data);
    return 0;
}

int
verifyEAuth_(struct lsfAuth *auth, struct sockaddr_in *from)
{
    static char fname[] = "verifyEAuth/lib.eauth.c";
    char path[MAXPATHLEN], uData[256], ok;
    char *eauth_client;
    char *eauth_server;
    char *eauth_aux_data;
    char *eauth_aux_status;
    int cc;
    int i;
    static int connected = false;
    static int in[2];
    static int out[2];

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s ...", fname);

    if (!(genParams_[LSF_AUTH].paramValue &&
          !strcmp(genParams_[LSF_AUTH].paramValue, AUTH_PARAM_EAUTH)))
        return -1;

    eauth_client = getenv("LSF_EAUTH_CLIENT");
    eauth_server = getenv("LSF_EAUTH_SERVER");
    eauth_aux_data = getenv("LSF_EAUTH_AUX_DATA");
    eauth_aux_status = getenv("LSF_EAUTH_AUX_STATUS");

    sprintf(uData, "%d %d %s %s %d %d %s %s %s %s\n", auth->uid, auth->gid,
            auth->lsfUserName, inet_ntoa(from->sin_addr),
            (int) ntohs(from->sin_port), auth->k.eauth.len,
            (eauth_client ? eauth_client : "NULL"),
            (eauth_server ? eauth_server : "NULL"),
            (eauth_aux_data ? eauth_aux_data : "NULL"),
            (eauth_aux_status ? eauth_aux_status : "NULL"));

    memset(path,0,sizeof(path));
    ls_strcat(path,sizeof(path),genParams_[LSF_SERVERDIR].paramValue);
    ls_strcat(path,sizeof(path),"/");
    ls_strcat(path,sizeof(path),EAUTHNAME);

    if (logclass & (LC_AUTH | LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: <%s> path <%s> connected=%d", fname, uData,
                  path, connected);

    if (connected) {
        struct timeval tv;
        fd_set  mask;

        FD_ZERO(&mask);
        FD_SET(out[0], &mask);

        tv.tv_sec = 0;
        tv.tv_usec = 0;

        if ((cc = select(out[0] + 1, &mask, NULL, NULL, &tv)) > 0) {
            if (logclass & (LC_AUTH | LC_TRACE))
                ls_syslog(LOG_DEBUG, "%s: <%s> got exception",
                          fname, uData);
            connected = false;
            close(in[1]);
            close(out[0]);
        } else {
            if (cc < 0)
                ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "select", uData);
        }

        if (logclass & (LC_AUTH | LC_TRACE))
            ls_syslog(LOG_DEBUG, "%s: <%s> select returned cc=%d", fname,
                      uData, cc);

    }

    if (!connected) {

        int pid;
        char *user;

        {
            if ((user = getLSFAdmin()) == NULL) {
                return -1;
            }
        }

        if (pipe(in) < 0) {
            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "pipe(in)", uData);
            lserrno = LSE_SOCK_SYS;
            return -1;
        }

        if (pipe(out) < 0) {
            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "pipe(out)", uData);
            lserrno = LSE_SOCK_SYS;
            return -1;
        }

        if ((pid = fork()) == 0) {
            char *myargv[3];
            struct passwd *pw;

            if ((pw = getpwnam2(user)) == NULL) {
                ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "getpwnam2", user);
                exit(-1);
            }

            if (setuid(pw->pw_uid) < 0) {
                ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", fname, "setuid",
                          pw->pw_uid);
                exit(-1);
            }

            for (i = 1; i < NSIG; i++)
                Signal_(i, SIG_DFL);

            alarm(0);

            close(in[1]);
            if (dup2(in[0], 0) == -1) {
                ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "dup2(in[0])", uData);
            }

            close(out[0]);
            if (dup2(out[1], 1) == -1) {
                ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "dup2(out[1])", uData);
            }

            for (i = 3; i < sysconf(_SC_OPEN_MAX); i++)
                close(i);

            myargv[0] = path;
            myargv[1] = "-s";
            myargv[2] = NULL;

            execvp(myargv[0], myargv);
            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "execvp", myargv[0]);
            exit(-1);
        }

        close(in[0]);
        close(out[1]);

        if (pid == -1) {
            ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m", fname, "fork", path);
            close(in[1]);
            close(out[0]);
            lserrno = LSE_FORK;
            return -1;
        }

        connected = true;
    }

    i = strlen(uData);

    if ((cc = b_write_fix(in[1], uData, i)) != i) {
        ls_syslog(LOG_ERR, "%s: b_write_fix <%s> failed, cc=%d, i=%d: %m",
                  fname, uData, cc, i);
        CLOSEHANDLE(in[1]);
        CLOSEHANDLE(out[0]);
        connected = false;
        return -1;
    }
    if(logclass & (LC_AUTH | LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: b_write_fix <%s> ok, cc=%d, i=%d",
                  fname, uData, cc, i);

    if ((cc = b_write_fix(in[1], auth->k.eauth.data, auth->k.eauth.len))
        != auth->k.eauth.len) {
        ls_syslog(LOG_ERR, "%s: b_write_fix <%s> failed, eauth.len=%d, cc=%d",
                  fname, uData, auth->k.eauth.len, cc);
        CLOSEHANDLE(in[1]);
        CLOSEHANDLE(out[0]);
        connected = false;
        return -1;
    }
    if(logclass & (LC_AUTH | LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: b_write_fix <%s> ok, eauth.len=%d, eauth.data=%.*s cc=%d:",
                  fname, uData, auth->k.eauth.len,
                  auth->k.eauth.len, auth->k.eauth.data,cc);

    if ((cc = b_read_fix(out[0], &ok, 1)) != 1) {
        ls_syslog(LOG_ERR, "%s: b_read_fix <%s> failed, cc=%d: %m",
                  fname, uData, cc);
        CLOSEHANDLE(in[1]);
        CLOSEHANDLE(out[0]);
        connected = false;
        return -1;
    }

    if (ok != '1') {
        ls_syslog(LOG_ERR, "%s: eauth <%s> len=%d failed, rc=%c",
                  fname, uData, auth->k.eauth.len, ok);
        return -1;
    }

    return 0;
}

static char *
getLSFAdmin(void)
{
    static char admin[MAXLSFNAMELEN];
    static char fname[] = "getLSFAdmin";
    char *mycluster;
    struct clusterInfo *clusterInfo;
    struct passwd *pw;
    char *lsfUserName;

    if (admin[0] != '\0')
        return admin;

    if ((mycluster = ls_getclustername()) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "ls_getclustername");
        return NULL;
    }
    if ((clusterInfo = ls_clusterinfo(NULL, NULL, NULL, 0, 0)) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "ls_clusterinfo");
        return NULL;
    }

    lsfUserName = (clusterInfo->nAdmins == 0 ? clusterInfo->managerName :
                   clusterInfo->admins[0]);

    if ((pw = getpwnam2(lsfUserName)) == NULL) {
        ls_syslog(LOG_ERR, "%s: %s(%s) failed: %m",
                  fname, "getpwnam2", lsfUserName);
        return NULL;
    }

    strcpy(admin, lsfUserName);
    return admin;
}

#define EAUTH_ENV_BUF_LEN       (MAXPATHLEN+32)

static int putEnvVar(char *buf, const char *envVar, const char *envValue)
{
    int rc, str_size;

    sprintf(buf, "%s=", envVar);
    if (envValue && strlen(envValue)) {
        str_size = strlen(buf) + strlen(envValue) + 1;
        if (str_size > EAUTH_ENV_BUF_LEN) {
            return -2;
        }
        strcat(buf, envValue);
    }

    rc = putenv(buf);
    if (rc != 0) {
        return -1;
    }

    return 0;
}

int
putEauthClientEnvVar(char *client)
{
    static char eauth_client[EAUTH_ENV_BUF_LEN];

    return putEnvVar(eauth_client, "LSF_EAUTH_CLIENT", client);

}

int
putEauthServerEnvVar(char *server)
{
    static char eauth_server[EAUTH_ENV_BUF_LEN];

    return putEnvVar(eauth_server, "LSF_EAUTH_SERVER", server);

}

#ifdef INTER_DAEMON_AUTH
int
putEauthAuxDataEnvVar(char *value)
{
    static char eauth_aux_auth_data[EAUTH_ENV_BUF_LEN];

    return putEnvVar(eauth_aux_auth_data, "LSF_EAUTH_AUX_DATA", value);

}

int
putEauthAuxStatusEnvVar(char *value)
{
    static char eauth_aux_auth_status[EAUTH_ENV_BUF_LEN];

    return putEnvVar(eauth_aux_auth_status, "LSF_EAUTH_AUX_STATUS", value);
}

#endif
