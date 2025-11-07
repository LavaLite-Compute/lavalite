/* $Id: lib.host.c,v 1.7 2007/08/15 22:18:50 tmizan Exp $
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

#undef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 128

int cpHostent(struct hostent *, const struct hostent *);
void freeHp(struct hostent *);
static struct hostent *my_gethostbyaddr(const char *);

static struct hostent * getHostEntByName(
    const char *hostName, int options);
static struct hostent *getHostEntByAddr(
    const struct in_addr *hostAddr, int options);
static char* unifyOfficialName(const char* hname);
static int loadHostEntByName(const char *hostName, int options,
                             struct hostent *hostEntP);
static int loadHostEntByAddr(const struct in_addr *hostAddr, int options,
                             struct hostent *hostEntP);
static int packHostEnt(const struct hostent *hp,
                       struct hostent *hostEntP);
int daemonId = 0;

static struct hostent globalHostEnt;

#define IS_SERVER_OR_NOT_LOCAL(hostAttrib)                              \
    ((hostAttrib & HOST_ATTR_SERVER) || (hostAttrib & HOST_ATTR_NOT_LOCAL))
#define IS_LOCAL_CLIENT(hostAttrib)                                     \
    ((hostAttrib & HOST_ATTR_CLIENT) && !(hostAttrib & HOST_ATTR_NOT_LOCAL))

char *
ls_getmyhostname(void)
{
    static char hname[MAXHOSTNAMELEN];
    const char* sp;

    if (hname[0] == 0) {
        gethostname(hname, MAXHOSTNAMELEN);
        sp = getHostOfficialByName_(hname);
        if(sp) {
            strcpy(hname, sp);
        } else {
            hname[0] = 0;
            return NULL;
        }
    }

    return hname;
}

struct hostent *
my_gethostbyname(char *name)
{

#define MY_HOSTSFILE "hosts"
#define MY_LINESIZE 1024
#define MY_NUMALIASES 36

    static char fname[] = "my_gethostbyname";
    FILE *hfile;
    static char buf[MY_LINESIZE];
    char *addrstr;
    static struct hostent host;
    static char *names[MY_NUMALIASES], *addrs[2];
    static u_int addr;
    int n, found = 0;

    if(logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG,"%s: Entering...for %s",fname,name);

    if (initenv_(NULL, NULL) < 0)
        return NULL;

    memset(buf,0,sizeof(buf));
    ls_strcat(buf,sizeof(buf),genParams_[LSF_CONFDIR].paramValue);
    ls_strcat(buf,sizeof(buf),"/");
    ls_strcat(buf,sizeof(buf),MY_HOSTSFILE);

    if (NULL == (hfile = fopen (buf, "r"))) {

        if(logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG,"%s: failed to open %s",fname,buf);
        return((struct hostent *)NULL);
    }

    while (NULL != fgets (buf, MY_LINESIZE, hfile)) {
        char *tmp;
        n = 0;

        if (NULL != (tmp = strpbrk (buf, "\n#")))
            *tmp='\0';

        if (NULL == (addrstr = strtok (buf, " \t")))
            continue;
        addr = inet_addr (addrstr);

        while (NULL != (names[n] = strtok(NULL, " \t"))) {
            if (equalHost_(names[n], name))
                found++;
            n++;
        }
        if (found) {
            host.h_name = names[0];
            host.h_aliases = names+1;
            host.h_addrtype = AF_INET;
            host.h_length = sizeof(addr);
            addrs[1] = NULL;
            addrs[0] = (char *)&addr;
            host.h_addr_list = addrs;
            fclose(hfile);
            return(&host);
        }
    }
    fclose(hfile);
    if(logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG,"%s: host <%s> not in hostfile",fname,name);

    return NULL;
}

struct hostent *
Gethostbyname_(char *hname)
{
    return (Gethostbyname_ex_(hname, 0));
}

struct hostent *
Gethostbyname_ex_(char *hname, int options)

{
    static char fname[] = "Gethostbyname_ex_";
    struct hostent *hostEntP;
    char lhname[MAXHOSTNAMELEN];

    if(logclass &  LC_TRACE)
        ls_syslog(LOG_DEBUG,"%s: Entering, for host :<%s>" ,fname,hname);
    if (strlen(hname) >= MAXHOSTNAMELEN) {
        lserrno = LSE_BAD_HOST;
        return NULL;
    }

    strcpy(lhname, hname);
    strToLower_(lhname);

    hostEntP = getHostEntByName(lhname, options);
    if(hostEntP) {
        if(hostEntP->h_addr == NULL) {
            return NULL;
        }
        if(logclass &  LC_TRACE)
            ls_syslog(LOG_DEBUG,
                      "%s: Exiting this routine, resolved hostent for host: <%s>",
                      fname,hname);
        return hostEntP;
    } else {
        if(lserrno == LSE_MALLOC) {
            ls_syslog(LOG_ERR,
                      "%s: getHostEntByName() failed for host: <%s>, %M",
                      fname, hname);
        } else {
            ls_syslog(LOG_DEBUG,
                      "%s: getHostEntByName() failed for host: <%s>, %M",
                      fname,hname);
        }
        return NULL;
    }
}

int
isValidHost_(const char* hname)
{
    static char fname[] = "isValidHost_";
    struct hostent *hostEntP;

    hostEntP = getHostEntByName(hname, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s failed for host <%s>: %M",
                  fname,hname);
        return 0;
    }

    return 1;
}

char*
getHostOfficialByName_(const char* hname)
{
    static char fname[] = "getHostOfficialByName_";
    static char hnameBuf[MAXHOSTNAMELEN];
    struct hostent *hostEntP;

    hostEntP = getHostEntByName(hname, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s:getHostEntByName() : Couldn't get the official name for <%s>. Maybe <%s> is not a host: %M",
                  fname,hname,hname);
        return NULL;
    }
    strcpy(hnameBuf, hostEntP->h_name);
    return hnameBuf;
}

char*
getHostOfficialByAddr_(const struct in_addr *addr)
{
    static char fname[] = "getHostOfficialByAddr_";
    struct hostent *hostEntP;
    static char hname[MAXHOSTNAMELEN];

    hostEntP = getHostEntByAddr(addr, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s: getHostEntByAddr() failed for host <%s>: %M",
                  fname, inet_ntoa(*addr));
        return NULL;
    }

    strcpy(hname, hostEntP->h_name);
    return hname;
}

struct in_addr *
getHostFirstAddr_(const char* hname)
{
    static char fname[] = "getHostFirstAddr_";
    struct hostent *hostEntP;

    hostEntP = getHostEntByName(hname, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s: getHostEntByName() failed for host <%s>: %M",
                  fname,hname);
        return NULL;
    }

    return (struct in_addr *)hostEntP->h_addr;
}

struct hostent*
getHostEntryByName_(const char* hname)
{
    static char fname[] = "getHostEntryByName_";
    struct hostent *hostEntP;

    hostEntP = getHostEntByName(hname, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s: getHostEntByName() failed for host <%s>: %M",
                  fname,hname);
        return NULL;
    }

    if(hostEntP->h_addr == NULL) {
        return NULL;
    }

    return hostEntP;
}

struct hostent*
getHostEntryByAddr_(const struct in_addr *addr)
{
    static char fname[] = "getHostEntryByAddr_";
    struct hostent *hostEntP;

    hostEntP = getHostEntByAddr(addr, 0);
    if(!hostEntP) {
        ls_syslog(LOG_DEBUG,
                  "%s: getHostEntByAddr() failed for host <%s>: %M",
                  fname, inet_ntoa(*addr));
        return NULL;
    }

    return hostEntP;
}

static struct hostent *
getHostEntByName(const char *hostName, int options)
{
    static char fname[] = "getHostEntByName";
    struct hostent *hostEntP = NULL;

    if (logclass &  LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "%s: Enter this routine, hostName = %s, options = %d",
                  fname, hostName, options);

    if (hostName == NULL) {
        lserrno = LSE_BAD_ARGS;
        ls_syslog(LOG_ERR,"%s Exiting: hostName == NULL, %M", fname);
        goto cleanup;
    }

    freeHp(&globalHostEnt);

    memset(&globalHostEnt, 0, sizeof(struct hostent));

    if (loadHostEntByName(hostName, options, &globalHostEnt) < 0) {
        goto cleanup;
    }

    hostEntP = &globalHostEnt;

cleanup:

    if (logclass &  LC_TRACE) {
        if (hostEntP == NULL) {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, hostent = NULL",
                      fname);
        } else if (hostEntP->h_addr) {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, official = %s, addr = %s",
                      fname, hostEntP->h_name,
                      inet_ntoa(*(struct in_addr*)hostEntP->h_addr));
        } else {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, official = %s, addr = NULL",
                      fname, hostEntP->h_name);
        }
    }

    return hostEntP;
}

static struct hostent *
getHostEntByAddr(const struct in_addr *hostAddr, int options)
{

    static char fname[] = "getHostEntByAddr";
    struct hostent *hostEntP = NULL;

    if (logclass &  LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "%s: Enter this routine, hostAddr = %s, options = %d",
                  fname, inet_ntoa(*hostAddr) , options);

    if (hostAddr == NULL) {
        lserrno = LSE_BAD_ARGS;
        ls_syslog(LOG_ERR,"%s Exiting: hostAddr == NULL, %M", fname);
        goto cleanup;
    }

    freeHp(&globalHostEnt);
    memset(&globalHostEnt, 0, sizeof(struct hostent));

    if (loadHostEntByAddr(hostAddr, options, &globalHostEnt) < 0) {
        goto cleanup;
    }

    hostEntP = &globalHostEnt;

cleanup:

    if (logclass &  LC_TRACE) {
        if(hostEntP == NULL) {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, hostent = NULL",
                      fname);
        } else if (hostEntP->h_addr) {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, official = %s, addr = %s",
                      fname, hostEntP->h_name,
                      inet_ntoa(*(struct in_addr*)hostEntP->h_addr));
        } else {
            ls_syslog(LOG_DEBUG3,
                      "%s: Exit this routine, official = %s, addr = NULL",
                      fname, hostEntP->h_name);
        }
    }

    return hostEntP;

}

void
stripDomain_(char *hname)
{
    char *p;
    for (p = hname; *p != '\0'; p++) {
        if (*p == '.') {
            *p = '\0';
            break;
        }
    }
}

static char*
unifyOfficialName(const char* hname)
{
    static char fname[] = "unifyOfficialName";
    char *officialNameP;
    int hnlen;
    char *strip;
    char *sp;

    officialNameP = strdup(hname);
    if(officialNameP == NULL) {
        ls_syslog(LOG_ERR,"%s: strdup for hname failed", fname);
        lserrno = LSE_MALLOC;
        return NULL;
    }

    if (stripDomains_ != NULL) {
        hnlen = strlen(hname);
        for (strip = stripDomains_ ; *strip ; strip += (*strip + 1)) {

            if (*strip >= hnlen)

                continue;

            sp = officialNameP + hnlen - *strip;
            if (strncasecmp(sp, (strip + 1), *strip) == 0) {

                if (sp > officialNameP && *(sp-1) == '.')
                    *(sp-1) = '\0';
                else
                    *sp = '\0';
                break;
            }
        }
    }

    strToLower_(officialNameP);

    return officialNameP;
}

static int
loadHostEntByName(const char *hostName, int options,
                  struct hostent *hostEntP)
{
    static char fname[] = "loadHostEntByName";
    struct hostent *hp;

    if(logclass &  LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "%s: Enter this routine, load hostent for host <%s>",
                  fname,hostName);

    if((hp = my_gethostbyname((char*)hostName)) == NULL
       && !(options & DISABLE_HNAME_SERVER)
       && !(getenv("DISABLE_HNAME_SERVER"))) {
        if(logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG3, "%s: Calling gethostbyname(%s) ...",
                      fname, hostName);
        hp = gethostbyname(hostName);
    }

    if (! hp) {
        lserrno = LSE_BAD_HOST;
        return -1;
    }

    return packHostEnt(hp, hostEntP);
}

static int
loadHostEntByAddr(const struct in_addr *hostAddr, int options,
                  struct hostent* hostEntP)
{
    static char fname[] = "loadHostEntByAddr";
    struct hostent *hp;
    struct hostent tmpHostEnt;
    char hostInStr[MAXHOSTNAMELEN];
    char *officialNameP;
    char **spp;
    int i, rtnVal;

    strcpy(hostInStr, inet_ntoa(*hostAddr));

    if(logclass &  LC_TRACE)
        ls_syslog(LOG_DEBUG,
                  "%s: Enter this routine, load hostent for host <%s>",
                  fname,hostInStr);

    if((hp = my_gethostbyaddr((const char *)hostAddr)) == NULL
       && !(options & DISABLE_HNAME_SERVER)
       && !(getenv("DISABLE_HNAME_SERVER"))) {
        if(logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG, "%s: Calling gethostbyaddr(%s) ...",
                      fname, hostInStr);
        hp = gethostbyaddr((const char *)hostAddr, sizeof(struct in_addr),
                           AF_INET);
    }

    if (! hp) {
        lserrno = LSE_BAD_HOST;
        return -1;
    }

    officialNameP = unifyOfficialName(hp->h_name);
    if(officialNameP == NULL) {
        return -1;
    }

    if((hp = my_gethostbyname((char*)officialNameP)) == NULL
       && !(options & DISABLE_HNAME_SERVER)
       && !(getenv("DISABLE_HNAME_SERVER"))) {
        if(logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG, "%s: Calling gethostbyname(%s) ...",
                      fname, officialNameP);
        hp = gethostbyname(officialNameP);
    }

    FREEUP(officialNameP);

    if (! hp) {
        lserrno = LSE_BAD_HOST;
        return -1;
    }

    for (i = 0; hp->h_addr_list[i]; i++) {
        if (!memcmp(hp->h_addr_list[i], hostAddr, hp->h_length)) {

            return (packHostEnt(hp, hostEntP));
        }
    }

    if(cpHostent(&tmpHostEnt, hp) == -1) {
        return (packHostEnt(hp, hostEntP));
    }

    if ((spp = (char **) realloc((char *) tmpHostEnt.h_addr_list,
                                 (i + 2) * sizeof(char *))) == NULL) {
        freeHp(&tmpHostEnt);
        return (packHostEnt(hp, hostEntP));
    }

    tmpHostEnt.h_addr_list = spp;

    if ((tmpHostEnt.h_addr_list[i] = (char *) malloc(tmpHostEnt.h_length))
        == NULL) {
        freeHp(&tmpHostEnt);
        return (packHostEnt(hp, hostEntP));
    }

    memcpy(tmpHostEnt.h_addr_list[i], hostAddr, tmpHostEnt.h_length);
    tmpHostEnt.h_addr_list[i+1] = NULL;

    rtnVal = packHostEnt(&tmpHostEnt, hostEntP);

    freeHp(&tmpHostEnt);

    return rtnVal;
}

static int
packHostEnt(const struct hostent *hp, struct hostent *hostEntP)
{
    char *officialNameP;
    int rtn = 0;

    officialNameP = unifyOfficialName(hp->h_name);
    if(officialNameP == NULL) {
        return -1;
    }

    if(cpHostent(hostEntP, hp) == -1) {
        lserrno = LSE_MALLOC;
        free(officialNameP);
        return -1;
    }
    FREEUP(hostEntP->h_name);
    hostEntP->h_name = officialNameP;

    return rtn;
}

void
freeHp(struct hostent *hp)
{
    int i;

    if(hp->h_name) {
        if(hp->h_addr_list) {
            for (i = 0; hp->h_addr_list[i]; i++)
                if(hp->h_addr_list[i]) free(hp->h_addr_list[i]);
            free(hp->h_addr_list);
            hp->h_addr_list = NULL;
        }

        if(hp->h_aliases) {
            for (i = 0; hp->h_aliases[i]; i++)
                if(hp->h_aliases[i]) free(hp->h_aliases[i]);
            free(hp->h_aliases);
            hp->h_aliases = NULL;
        }

        free(hp->h_name);
        hp->h_name = NULL;
    }
}

int
cpHostent(struct hostent *to, const struct hostent *from)
{
    int i, j, n;

    if ((to->h_name = putstr_(from->h_name)) == NULL)
        return -1;

    if(from->h_aliases) {
        for (n = 0; from->h_aliases[n]; n++);

        if ((to->h_aliases = (char **) calloc(n+1, sizeof(char *))) == NULL) {
            free(to->h_name);
            to->h_name = NULL;
            return -1;
        }

        for (j = 0; j < n; j++) {
            if ((to->h_aliases[j] = putstr_(from->h_aliases[j])) == NULL) {
                while(j--)
                    free(to->h_aliases[j]);
                free(to->h_aliases);
                free(to->h_name);
                to->h_name = NULL;
                return -1;
            }
        }
        to->h_aliases[n] = NULL;
    } else {
        to->h_aliases = NULL;
    }

    to->h_addrtype = from->h_addrtype;
    to->h_length = from->h_length;

    if(from->h_addr_list) {
        for (n = 0; from->h_addr_list[n]; n++);

        if ((to->h_addr_list = (char **) calloc(n+1,
                                                sizeof(char *))) == NULL) {
            for (j = 0; to->h_aliases[j]; j++)
                free(to->h_aliases[j]);
            free(to->h_aliases);
            free(to->h_name);
            to->h_name = NULL;
            return -1;
        }

        for (i = 0; i < n; i++) {
            if ((to->h_addr_list[i] = (char*)malloc(from->h_length))
                == NULL) {
                while(i--)
                    free(to->h_addr_list[i]);
                free(to->h_addr_list);
                for (j = 0; to->h_aliases[j]; j++)
                    free(to->h_aliases[j]);
                free(to->h_aliases);
                free(to->h_name);
                to->h_name = NULL;
                return -1;
            }
            memcpy(to->h_addr_list[i], from->h_addr_list[i], from->h_length);
        }
        to->h_addr_list[n] = NULL;
    } else {
        to->h_addr_list = NULL;
    }

    return 0;
}

static struct hostent *
my_gethostbyaddr(const char *faddr)
{
    FILE *hfile;
    static char buf[MY_LINESIZE];
    char *addrstr;
    static struct hostent host;
    static char *names[MY_NUMALIASES], *addrs[2];
    static u_int addr;
    int n;

    if (initenv_(NULL, NULL) < 0)
        return NULL;

    memset(buf,0,sizeof(buf));
    ls_strcat(buf,sizeof(buf), genParams_[LSF_CONFDIR].paramValue);
    ls_strcat(buf,sizeof(buf), "/");
    ls_strcat(buf,sizeof(buf), MY_HOSTSFILE);

    hfile = fopen (buf, "r");
    if (hfile == NULL) {
        return NULL;
    }

    while (fgets (buf, MY_LINESIZE, hfile)) {
        char *tmp;
        n = 0;

        if ((tmp = strpbrk (buf, "\n#")))
            *tmp='\0';

        if ((addrstr = strtok (buf, " \t")) == NULL)
            continue;

        addr = inet_addr (addrstr);

        if (!memcmp(&addr, faddr, sizeof(u_int))) {
            while ((names[n] = strtok(NULL, " \t")) != NULL) {
                n++;
            }

            host.h_name = names[0];
            host.h_aliases = names+1;
            host.h_addrtype = AF_INET;
            host.h_length = sizeof(addr);
            addrs[1] = NULL;
            addrs[0] = (char *)&addr;
            host.h_addr_list = addrs;
            fclose(hfile);
            return &host;
        }
    }
    fclose(hfile);

    return NULL;
}

struct hostent *
Gethostbyaddr_(char *addr, int len, int type)
{
    static char fname[] = "Gethostbyaddr_";
    struct hostent *hostEntP;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: %x %x %x %x len %d", fname,
                  addr[0], addr[1], addr[2], addr[3], len);

    hostEntP = getHostEntByAddr((const struct in_addr *)addr, 0);

    if(hostEntP) {
        if(logclass &  LC_TRACE)
            ls_syslog(LOG_DEBUG,
                      "%s: Exiting this routine, cache for host: <%s>",
                      fname, hostEntP->h_name);
        return hostEntP;
    } else {
        ls_syslog(LOG_DEBUG,
                  "%s: getHostEntByAddr() failed for host: <%s>, %M",
                  fname,(char*)inet_ntoa(*(struct in_addr *)addr));
        return NULL;
    }
}

#define ISBOUNDARY(h1, h2, len)  ( (h1[len]=='.' || h1[len]=='\0') &&   \
                                   (h2[len]=='.' || h2[len]=='\0') )

int
equalHost_(const char *host1, const char *host2)
{
    int len;

    if (strlen(host1) > strlen(host2))
        len = strlen(host2);
    else
        len = strlen(host1);

    if ((strncasecmp(host1, host2, len) == 0) && ISBOUNDARY(host1, host2, len))
        return true;

    return false;
}

char *
sockAdd2Str_(struct sockaddr_in *from)
{
    static char adbuf[24];

    sprintf(adbuf, "%s:%d", inet_ntoa(from->sin_addr),
            (int) ntohs(from->sin_port));
    return adbuf;
}
