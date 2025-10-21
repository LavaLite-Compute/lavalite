/* $Id: lib.misc.c,v 1.5 2007/08/15 22:18:50 tmizan Exp $
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

// Thread-local like ctime_r users expect (one buffer per thread)
static __thread char ctime2_buf[MICROBUF_SIZ];

/*
 * ctime2() — same as ctime(), but:
 *   - thread-safe (uses localtime_r)
 *   - no trailing '\n'
 *   - returns pointer to static buffer (like ctime)
 */
const char *
ctime2(const time_t *tp)
{
    if (!tp) {
        lserrno = LSE_BAD_TIME;
        return "";
    }

    struct tm tm;
    if (!localtime_r(tp, &tm)) {
        lserrno = LSE_BAD_TIME;
        return "";
    }

    // "%a %b %e %T %Y" -> e.g., "Wed Jun  3 11:22:33 2020"
    if (strftime(ctime2_buf, sizeof ctime2_buf, "%a %b %e %T %Y", &tm) == 0)
        return "";

    return ctime2_buf;
}

/* Bug. Various miscellaneus functions more or less useful but mostly bogus
 * ready for removal as we have ctypes and string headers for the functionality.
*/

#define BADCH   ":"

extern int optind;
extern char *optarg;
extern int  opterr;
extern int  optopt;

#define PRINT_ERRMSG(errMsg, fmt, msg1, msg2)   \
{                                           \
    if (errMsg == NULL)                     \
    fprintf(stderr, fmt, msg1, msg2);   \
    else                                    \
    sprintf(*errMsg, fmt, msg1, msg2);  \
}

struct LSFAdmins {
    int     numAdmins;
    char    **names;
} LSFAdmins;

bool_t isLSFAdmin(const char *);

char
isanumber_(char *word)
{
    char **eptr;
    double number;

    if (!word || *word == '\0')
        return false;

    if (errno == ERANGE)
        errno = 0;

    eptr = &word;
    number = strtod (word, eptr);
    if (**eptr == '\0' &&  errno != ERANGE)
        if (number <= FLT_MAX && number > -FLT_MAX)
            return true;
    return false;

}

char
islongint_(const char *word)
{
    long long int number;

    if (!word || *word == '\0')
        return false;

    if (!isdigitstr_(word))
        return false;

    if (errno == ERANGE)
        errno = 0;

    sscanf(word, "%lld", &number);
    if (errno != ERANGE) {
        if (number <= INFINIT_LONG_INT && number > -INFINIT_LONG_INT)
            return true;
    }
    return false;
}

int
isdigitstr_(const char *string)
{
    int i;

    for(i = 0; i < strlen(string); i++) {
        if (!isdigit(string[i])) {
            return false;
        }
    }
    return true;
}

int64_t
atoi64_(const char *word)
{
    int64_t number;

    if (!word || *word == '\0')
        return 0;

    if (errno == ERANGE)
        errno = 0;

    sscanf(word, "%ld", &number);
    if (errno != ERANGE) {
        if (number <= INFINIT_LONG_INT && number > -INFINIT_LONG_INT)
            return number;
    }
    return 0;
}

char
isint_(char *word)
{
    char **eptr;
    int number;

    if (!word || *word == '\0')
        return false;

    if (errno == ERANGE)
        errno = 0;
    eptr = &word;
    number = strtol (word, eptr, 10);
    if (**eptr == '\0' &&  errno != ERANGE) {
        if (number <= INFINIT_INT && number > -INFINIT_INT)
            return true;
    }
    return false;

}

char *
putstr_(const char *s)
{
    char *p;

    if (s == NULL) {
        s = "";
    }

    p = malloc(strlen(s)+1);
    if (!p)
        return NULL;

    strcpy(p, s);

    return p;
}

short
getRefNum_(void)
{
    static short reqRefNum = MIN_REF_NUM;

    reqRefNum++;
    if (reqRefNum >= MAX_REF_NUM)
        reqRefNum = MIN_REF_NUM;
    return reqRefNum;
}

char *
chDisplay_(char *disp)
{
    char *sp, *hostName;
    static char dspbuf[MAXHOSTNAMELEN+10];

    sp = disp +8;
    if (strncmp("unix:", sp, 5) == 0)
        sp += 4;
    else if (strncmp("localhost:", sp, 10) == 0)
        sp += 9;

    if (sp[0] == ':') {
        if ((hostName = ls_getmyhostname()) == NULL)
            return disp;
        sprintf(dspbuf, "%s=%s%s", "DISPLAY", hostName, sp);
        return dspbuf;
    }

    return disp;
}

void
strToLower_(char *name)
{
    while (*name != '\0') {
        *name = tolower(*name);
        name++;
    }

}

char *
getNextToken(char **sp)
{
    static char word[MAXLINELEN];
    char *cp;

    if (!*sp)
        return NULL;

    cp = *sp;
    if (cp[0] == '\0')
        return NULL;

    if (cp[0] == ':' || cp[0] == '=' || cp[0] == ' ')
        *sp += 1;
    cp = *sp;
    if (cp[0] == '\0')
        return NULL;

    strcpy(word, cp);
    if ((cp = strchr(word, ':')) != NULL)
        *cp = '\0';
    if ((cp = strchr(word, '=')) != NULL)
        *cp = '\0';

    *sp += strlen(word);
    return word;

}

int
getValPair(char **resReq, int *val1, int *val2)
{
    char *token, *cp, *wd1 = NULL, *wd2 = NULL;
    int len;

    *val1 = INFINIT_INT;
    *val2 = INFINIT_INT;

    token = getNextToken(resReq);
    if (!token)
        return 0;
    len = strlen (token);
    if (len == 0)
        return 0;
    cp = token;
    while (*cp != '\0' && *cp != ',' && *cp != '/')
        cp++;
    if (*cp != '\0') {
        *cp = '\0';
        if (cp - token > 0)
            wd1 = token;
        if (cp - token < len - 1)
            wd2 = ++cp;
    } else
        wd1 = token;
    if (wd1 && !isint_(wd1))
        return -1;
    if (wd2 && !isint_(wd2))
        return -1;
    if (!wd1 && !wd2)
        return -1;
    if (wd1)
        *val1 = atoi(wd1);
    if (wd2)
        *val2 = atoi(wd2);

    return 0;
}

char *
my_getopt(int nargc, char **nargv, char *ostr, char **errMsg)
{
    char svstr [256];
    char *cp1 = svstr;
    char *cp2 = svstr;
    char *optName;
    int i, num_arg;

    if ((optName = nargv[optind]) == NULL)
        return NULL;
    if (optind >= nargc || *optName != '-')
        return NULL;
    if (optName[1] && *++optName == '-') {
        ++optind;
        return NULL;
    }
    if (ostr == NULL)
        return NULL;
    strcpy (svstr, ostr);
    num_arg = 0;
    optarg = NULL;

    while (*cp2) {
        int cp2len = strlen(cp2);
        for (i=0; i<cp2len; i++) {
            if (cp2[i] == '|') {
                num_arg = 0;
                cp2[i] = '\0';
                break;
            }
            else if (cp2[i] == ':') {
                num_arg = 1;
                cp2[i] = '\0';
                break;
            }
        }
        if (i >= cp2len)
            return BADCH;

        if (!strcmp (optName, cp1)) {
            if (num_arg) {
                if (nargc <= optind + 1) {
                    PRINT_ERRMSG (errMsg, ("%s: option requires an argument -- %s\n"), nargv[0], optName);
                    return BADCH;
                }
                optarg = nargv[++optind];
            }
            ++optind;
            return optName;
        } else if (!strncmp(optName, cp1, strlen(cp1))) {
            if (num_arg == 0) {
                PRINT_ERRMSG (errMsg, ("%s: option cannot have an argument -- %s\n"),
                              nargv[0], cp1);
                return BADCH;
            }

            optarg = optName + strlen(cp1);
            ++optind;
            return cp1;
        }

        cp1 = &cp2[i];
        cp2 = ++cp1;
    }
    PRINT_ERRMSG (errMsg, ("%s: illegal option -- %s\n"), nargv[0], optName);
    return BADCH;

}

int putEnv(char *env, char *val)
{

    char *buf;

    buf = malloc(strlen(env) + strlen(val) + 4);
    if (buf == NULL)
        return -1;
    sprintf(buf, "%s=%s", env, val);
    return(putenv(buf));
}

void
initLSFHeader_(struct LSFHeader *hdr)
{
    hdr->refCode = 0;
    hdr->version = _XDR_VERSION_0_1_0;
    hdr->reserved0.High = 0;
    hdr->reserved0.Low = 0;
    hdr->length    = 0;

}

void *
myrealloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return malloc(size);
    } else {
        return realloc(ptr, size);
    }
}

int
Bind_(int sockfd, struct sockaddr *myaddr, int addrlen)
{
    struct sockaddr_in *cliaddr;
    ushort port;
    int i;

    cliaddr = (struct sockaddr_in *)myaddr;
    if (cliaddr->sin_port != 0)
        return(bind(sockfd, myaddr, addrlen));
    else {
        for (i = 1 ; i <= BIND_RETRY_TIMES; i++) {
            if (bind(sockfd, (struct sockaddr *)cliaddr, addrlen) == 0)
                return 0;
            else {
                if (errno == EADDRINUSE) {
                    if (i == 1) {
                        port = (ushort) (time(0) | getpid());
                        port = ((port < 1024) ? (port + 1024) : port);
                    }
                    else {
                        port++;
                        port = ((port < 1024) ? (port + 1024) : port);
                    }
                    ls_syslog(LOG_ERR,("%s: retry <%d> times, port <%d> will be bound" ),
                              "Bind_", i, port);
                    cliaddr->sin_port = htons(port);
                }
                else
                    return -1;
            }
        }
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, "Bind_", "bind", BIND_RETRY_TIMES);
        return -1;
    }
}
int
isMasterCrossPlatform(void)
{
    static char fname[] ="isMasterCrossPlatform()";
    char masterName[MAXHOSTNAMELEN], masterType[MAXLSFNAMELEN];
    char localType[MAXLSFNAMELEN];
    char *sp;
    static int crossPlatform = -1;

    if (crossPlatform >= 0)
        return crossPlatform;

    if ((sp = ls_getmastername()) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_getmastername");
        return false;
    }
    strcpy(masterName, sp);

    if ((sp = ls_gethosttype(masterName)) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_gethosttype");
        return false;
    }
    strcpy(masterType, sp);

    if ((sp = ls_gethosttype(NULL)) == NULL ) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_gethosttype");
        return false;
    }
    strcpy(localType, sp);

    if ((!strncasecmp(masterType, "NT", 2) && strncasecmp(localType, "NT", 2))
        ||
        (!strncasecmp(localType, "NT", 2) && strncasecmp(masterType, "NT", 2)))
        crossPlatform = true;
    else
        crossPlatform = false;

    return crossPlatform;
}
int
isAllowCross(char *paramValue)
{
    int cross = true;

    if (paramValue != NULL &&
        !strcasecmp(paramValue, "NO"))
        cross = false;

    return cross;
}

const char*
getCmdPathName_(const char *cmdStr, int* cmdLen)
{
    char* pRealCmd;
    char* sp1;
    char* sp2;

    for (pRealCmd = (char*)cmdStr; *pRealCmd == ' '
         || *pRealCmd == '\t'
         || *pRealCmd == '\n'
         ; pRealCmd++);

    if (pRealCmd[0] == '\'' || pRealCmd[0] == '"') {
        sp1 = &pRealCmd[1];
        sp2 = strchr(sp1, pRealCmd[0]);
    } else {
        int i;

        sp1 = pRealCmd;
        for (i = 0; sp1[i] != '\0'; i++) {
            if (sp1[i] == ';' || sp1[i] == ' ' ||
                sp1[i] == '&' || sp1[i] == '>' ||
                sp1[i] == '<' || sp1[i] == '|' ||
                sp1[i] == '\t' || sp1[i] == '\n')
                break;
        }

        sp2 = &sp1[i];
    }

    if (sp2) {
        *cmdLen = sp2 - sp1;
    } else {
        *cmdLen = strlen(sp1);
    }
    return sp1;
}

int
replace1stCmd_(const char* oldCmdArgs, const char* newCmdArgs,
               char* outCmdArgs, int outLen)
{
    const char *sp1;
    const char *sp2;
    int len2;
    const char *sp3;
    char *curSp;
    const char* newSp;
    int newLen;
    int len;

    newSp = getCmdPathName_(newCmdArgs, &newLen);
    sp1 = oldCmdArgs;
    sp2 = getCmdPathName_(sp1, &len2);
    if (newLen - len2 + strlen(sp1) >= outLen) {
        return -1;
    }
    sp3 = sp2 + len2;

    len = sp2 - sp1;
    curSp = memcpy(outCmdArgs, sp1, len);
    curSp = memcpy(curSp + len, newSp, newLen);
    strcpy(curSp + newLen, sp3);

    return 0;
}

const char*
getLowestDir_(const char* filePath)
{
    static char dirName[MAXFILENAMELEN];
    const char *sp1, *sp2;
    int len;

    sp1 = strrchr(filePath, '/');
    if (sp1 == NULL) {
        sp1 = filePath;
    }
    sp2 = strrchr(filePath, '\\');
    if (sp2 == NULL) {
        sp2 = filePath;
    }
    len = (sp2 > sp1) ? sp2-filePath : sp1-filePath;

    if(len) {
        memcpy(dirName, filePath, len);
        dirName[len] = 0;
    } else {
        return NULL;
    }

    return dirName;
}

void
getLSFAdmins_(void)
{
    struct clusterInfo    *clusterInfo;
    int i;

    clusterInfo = ls_clusterinfo(NULL, NULL, NULL, 0, 0);
    if (clusterInfo == NULL) {
        return;
    }

    if (LSFAdmins.numAdmins != 0) {
        FREEUP(LSFAdmins.names);
    }

    LSFAdmins.numAdmins = clusterInfo->nAdmins;

    LSFAdmins.names = calloc(LSFAdmins.numAdmins, sizeof(char *));
    if (LSFAdmins.names == NULL) {
        LSFAdmins.numAdmins = 0;
        return;
    }

    for (i = 0; i < LSFAdmins.numAdmins; i ++) {
        LSFAdmins.names[i] = putstr_(clusterInfo->admins[i]);
        if (LSFAdmins.names[i] == NULL) {
            int j;

            for (j = 0; j < i; j ++) {
                FREEUP(LSFAdmins.names[j]);
            }
            FREEUP(LSFAdmins.names);
            LSFAdmins.numAdmins = 0;

            return;
        }
    }
}

bool_t
isLSFAdmin_(const char *name)
{
    int    i;

    for (i = 0; i < LSFAdmins.numAdmins; i++) {
        if (strcmp(name, LSFAdmins.names[i]) == 0) {
            return true;
        }
    }

    return false;

}

int
ls_strcat(char *trustedBuffer, int bufferLength, char *strToAdd)
{
    int start = strlen(trustedBuffer);
    int remainder = bufferLength - start;
    int i;

    if ((start > bufferLength) || strToAdd == NULL) {
        return -1;
    }

    for(i = 0; i < remainder; i++) {
        trustedBuffer[start+i] = strToAdd[i];
        if (strToAdd[i] == '\0' ) {
            break;
        }
    }
    if (i == remainder) {
        trustedBuffer[bufferLength-1] = '\0';
        return -1;
    }
    return 0;
}

int
get_uid(const char *user, uid_t *uid)
{
    struct passwd *pwd;

    if (user == NULL)
        return LSE_BADUSER;

    pwd = getpwnam(user);
    if (pwd == NULL)
        return LSE_BADUSER;

    *uid = pwd->pw_uid;

    return LSE_NO_ERR;
}
