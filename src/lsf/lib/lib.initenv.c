/* $Id: lib.initenv.c,v 1.7 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.channel.h"
#include "lsf/lib/ll.sysenv.h"

struct config_param genParams[LSF_PARAM_COUNT] = {
    // Common
    [LSF_CONFDIR] = {"LSF_CONFDIR", NULL},
    [LSF_SERVERDIR] = {"LSF_SERVERDIR", NULL},
    [LSF_LOGDIR] = {"LSF_LOGDIR", NULL},
    [LSF_LIM_DEBUG] = {"LSF_LIM_DEBUG", NULL},
    [LSF_LIM_PORT] = {"LSF_LIM_PORT", NULL},
    [LSF_RES_PORT] = {"LSF_RES_PORT", NULL},
    [LSF_LOG_MASK] = {"LSF_LOG_MASK", NULL},
    [LSF_MASTER_LIST] = {"LSF_MASTER_LIST", NULL},
    [LSF_ROOT_REX] = {"LSF_ROOT_REX", NULL},

    // LIM-specific
    [LSF_DEBUG_LIM] = {"LSF_DEBUG_LIM", NULL},
    [LSF_TIME_LIM] = {"LSF_TIME_LIM", NULL},
    [LSF_LIM_IGNORE_CHECKSUM] = {"LSF_LIM_IGNORE_CHECKSUM", NULL},
    [LSF_LIM_JACKUP_BUSY] = {"LSF_LIM_JACKUP_BUSY", NULL},

    // LIB-specific
    [LSF_SERVER_HOSTS] = {"LSF_SERVER_HOSTS", NULL},
    [LSF_AUTH] = {"LSF_AUTH", NULL},
    [LSF_API_CONNTIMEOUT] = {"LSF_API_CONNTIMEOUT", NULL},
    [LSF_API_RECVTIMEOUT] = {"LSF_API_RECVTIMEOUT", NULL},
    [LSF_INTERACTIVE_STDERR] = {"LSF_INTERACTIVE_STDERR", NULL},

    // SBD
    [LSB_SBD_PORT] = {"LSB_SBD_PORT", NULL},
    [LSB_DEBUG_SBD] = {"LSB_DEBUG_SBD", NULL},
    [LSB_TIME_SBD] = {"LSB_TIME_SBD", NULL},
    [LSB_SBD_CONNTIMEOUT] = {"LSB_SBD_CONNTIMEOUT", NULL},
    [LSB_SBD_READTIMEOUT] = {"LSB_SBD_READTIMEOUT", NULL},

    // MBD
    [LSB_MBD_PORT] = {"LSB_MBD_PORT", NULL},

    // Legacy placeholder several code depend on this...
    [LSF_NULL_PARAM] = {NULL, NULL},
};

static_assert(LSF_PARAM_COUNT == (sizeof genParams / sizeof genParams[0]),
              "genParams size must match ll_params_t count");

int errLineNum_ = 0;
char *lsTmpDir_ = "/tmp";

static char **m_masterCandidates = NULL;
static int m_numMasterCandidates, m_isMasterCandidate;

static int parseLine(char *line, char **keyPtr, char **valuePtr);
static int matchEnv(char *, struct config_param *);
static int setConfEnv(char *, char *, struct config_param *);
// Lavalite
static int check_ll_conf(const char *);
int static readconfenv_(struct config_param *, struct config_param *,
                        const char *);

static int doEnvParams_(struct config_param *plp)
{
    char *sp, *spp;

    if (!plp)
        return 0;

    for (; plp->paramName != NULL; plp++) {
        if ((sp = getenv(plp->paramName)) != NULL) {
            if (NULL == (spp = putstr_(sp))) {
                lserrno = LSE_MALLOC;
                return -1;
            }
            FREEUP(plp->paramValue);
            plp->paramValue = spp;
        }
    }
    return 0;
}

int initenv_(struct config_param *userEnv, char *pathname)
{
    int Error = 0;
    char *envdir;
    static int lsfenvset = false;

    /* Initialiaze the channel as first thing
     */
    chan_init();

    if ((envdir = getenv("LSF_ENVDIR")) != NULL)
        pathname = envdir;
    else if (pathname == NULL)
        pathname = LL_CONF;

    if (check_ll_conf(pathname) < 0) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    if (lsfenvset) {
        if (userEnv == NULL) {
            return 0;
        }
        if (readconfenv_(NULL, userEnv, pathname) < 0) {
            return -1;
        } else if (doEnvParams_(userEnv) < 0) {
            return -1;
        }
        return 0;
    }

    if (readconfenv_(genParams, userEnv, pathname) < 0)
        return -1;
    if (doEnvParams_(genParams) < 0)
        return -1;
    lsfenvset = true;
    if (doEnvParams_(userEnv) < 0)
        Error = 1;

    if (!genParams[LSF_CONFDIR].paramValue ||
        !genParams[LSF_SERVERDIR].paramValue) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    if (genParams[LSF_SERVER_HOSTS].paramValue != NULL) {
        char *sp;
        for (sp = genParams[LSF_SERVER_HOSTS].paramValue; *sp != '\0'; sp++)
            if (*sp == '\"')
                *sp = ' ';
    }

    if (Error)
        return -1;

    return 0;
}

int ls_readconfenv(struct config_param *paramList, const char *conf_path)
{
    return (readconfenv_(NULL, paramList, conf_path));
}

int static readconfenv_(struct config_param *conf_array,
                        struct config_param *conf_array0, const char *path)
{
    char *key;
    char *value;
    char *line;
    FILE *fp;
    char filename[PATH_MAX];
    int lineNum = 0;
    int saveErrNo;

    if (path == NULL) {
        char *ep = getenv("LSF_ENVDIR");
        if (ep == NULL) {
            lserrno = LSE_LSFCONF;
            return -1;
        }
        sprintf(filename, "%s/lsf.conf", ep);
    }

    fp = fopen(filename, "r");
    if (!fp) {
        lserrno = LSE_LSFCONF;
        return -1;
    }

    lineNum = 0;
    errLineNum_ = 0;
    saveErrNo = 0;
    while ((line = getNextLineC_(fp, &lineNum, true)) != NULL) {
        int cc = parseLine(line, &key, &value);
        if (cc < 0 && errLineNum_ == 0) {
            errLineNum_ = lineNum;
            saveErrNo = lserrno;
            continue;
        }
        if (!matchEnv(key, conf_array) && !matchEnv(key, conf_array0))
            continue;

        if (!setConfEnv(key, value, conf_array) ||
            !setConfEnv(key, value, conf_array0)) {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);
    if (errLineNum_ != 0) {
        lserrno = saveErrNo;
        return -1;
    }

    return 0;
}

static int parseLine(char *line, char **keyPtr, char **valuePtr)
{
    char *sp = line;
#define L_MAXLINELEN_4ENV (8 * MAXLINELEN)
    static char key[L_MAXLINELEN_4ENV];
    static char value[L_MAXLINELEN_4ENV];
    char *word;
    char *cp;

    if (strlen(sp) >= L_MAXLINELEN_4ENV - 1) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    *keyPtr = key;
    *valuePtr = value;

    word = getNextWord_(&sp);

    strcpy(key, word);
    cp = strchr(key, '=');

    if (cp == NULL) {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    *cp = '\0';

    sp = strchr(line, '=');

    if (sp[1] == ' ' || sp[1] == '\t') {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    if (sp[1] == '\0') {
        value[0] = '\0';
        return 0;
    }

    sp++;
    word = getNextValueQ_(&sp, '\"', '\"');
    if (!word)
        return -1;

    strcpy(value, word);

    word = getNextValueQ_(&sp, '\"', '\"');
    if (word != NULL || lserrno != LSE_NO_ERR) {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    return 0;
}

static int matchEnv(char *name, struct config_param *paramList)
{
    if (paramList == NULL)
        return false;

    for (; paramList->paramName; paramList++)
        if (strcmp(paramList->paramName, name) == 0)
            return true;

    return false;
}

static int setConfEnv(char *name, char *value, struct config_param *paramList)
{
    if (paramList == NULL)
        return 1;

    if (value == NULL)
        value = "";

    for (; paramList->paramName; paramList++) {
        if (strcmp(paramList->paramName, name) == 0) {
            FREEUP(paramList->paramValue);
            paramList->paramValue = putstr_(value);
            if (paramList->paramValue == NULL) {
                lserrno = LSE_MALLOC;
                return 0;
            }
        }
    }
    return 1;
}

// Bug revisit the LSF_MASTER_LIST
int initMasterList_(void)
{
    char *nameList;
    char *hname;
    int i;
    char *paramValue = genParams[LSF_MASTER_LIST].paramValue;

    if (m_masterCandidates) {
        return 0;
    }

    if (paramValue == NULL) {
        m_isMasterCandidate = true;
        return 0;
    }

    for (nameList = paramValue; *nameList != '\0'; nameList++) {
        if (*nameList == '\"') {
            *nameList = ' ';
        }
    }

    nameList = paramValue;
    m_numMasterCandidates = 0;

    while ((hname = getNextWord_(&nameList)) != NULL) {
        m_numMasterCandidates++;
    }

    if (m_numMasterCandidates == 0) {
        lserrno = LSE_NO_HOST;
        return -1;
    }

    m_masterCandidates = calloc(m_numMasterCandidates, sizeof(char *));
    if (m_masterCandidates == NULL) {
        lserrno = LSE_MALLOC;
        return -1;
    }
    for (i = 0; i < m_numMasterCandidates; i++) {
        m_masterCandidates[i] = NULL;
    }

    i = 0;
    nameList = paramValue;

    while ((hname = getNextWord_(&nameList)) != NULL) {
        struct ll_host hp;
        int cc = get_host_by_name(hname, &hp);
        if (cc < 0) {
            lserrno = LSE_BAD_HOST;
            return -1;
        }
        m_masterCandidates[i] = putstr_(hp.name);
    }

    if (m_numMasterCandidates == 0) {
        lserrno = LSE_NO_HOST;
        return -1;
    }

    if (getMasterCandidateNoByName_(ls_getmyhostname()) >= 0) {
        m_isMasterCandidate = true;
    } else {
        m_isMasterCandidate = false;
    }

    return 0;
}

short getMasterCandidateNoByName_(char *hname)
{
    short count;

    for (count = 0; count < m_numMasterCandidates; count++) {
        if ((m_masterCandidates[count] != NULL) &&
            equal_host(m_masterCandidates[count], hname)) {
            return count;
        }
    }

    return -1;
}

char *getMasterCandidateNameByNo_(short candidateNo)
{
    if (candidateNo < m_numMasterCandidates)
        return m_masterCandidates[candidateNo];

    return NULL;
}

int getNumMasterCandidates_(void)
{
    return m_numMasterCandidates;
}

int getIsMasterCandidate_(void)
{
    return m_isMasterCandidate;
}

void freeupMasterCandidate_(int index)
{
    FREEUP(m_masterCandidates[index]);
    m_masterCandidates[index] = NULL;
}

static int check_ll_conf(const char *file)
{
    struct stat stat_buf;
    char buf[PATH_MAX];

    if (file == NULL)
        return -1;

    sprintf(buf, "%s/lsf.conf", file);
    int cc = stat(buf, &stat_buf);
    if (cc < 0) {
        return -1;
    }

    return 0;
}

#if 0
// Lavalite line parser enable later
int parse_env_line(const char *line,
                   char *key, size_t key_len,
                   char *value, size_t value_len)
{
    const char *p = line;
    const char *eq;
    const char *start;
    const char *end;
    size_t len;

    if (!line || !key || !value || key_len == 0 || value_len == 0) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    // skip leading spaces
    while (*p && isspace((unsigned char)*p))
        p++;

    // must contain '='
    eq = strchr(p, '=');
    if (!eq) {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    // key is [p, eq)
    start = p;
    end = eq;

    // no spaces in key (like old code semantics)
    for (const char *t = start; t < end; t++) {
        if (isspace((unsigned char)*t)) {
            lserrno = LSE_CONF_SYNTAX;
            return -1;
        }
    }

    len = (size_t)(end - start);
    if (len == 0 || len >= key_len) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    memcpy(key, start, len);
    key[len] = '\0';

    // move past '='
    p = eq + 1;

    // old parseLine: value cannot start with space or tab
    if (*p == ' ' || *p == '\t') {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    // empty value: KEY=
    if (*p == '\0') {
        if (value_len == 0) {
            lserrno = LSE_BAD_ENV;
            return -1;
        }
        value[0] = '\0';
        return 0;
    }

    // quoted value: KEY="..."
    if (*p == '"') {
        p++;                    // skip opening quote
        start = p;

        while (*p && *p != '"')
            p++;

        if (*p != '"') {
            // no closing quote
            lserrno = LSE_CONF_SYNTAX;
            return -1;
        }

        end = p;               // position of closing quote

        len = (size_t)(end - start);
        if (len >= value_len) {
            lserrno = LSE_BAD_ENV;
            return -1;
        }

        memcpy(value, start, len);
        value[len] = '\0';

        p++;                   // skip closing quote

        // skip trailing spaces after closing quote
        while (*p && isspace((unsigned char)*p))
            p++;

        // extra garbage after quoted value → syntax error
        if (*p != '\0') {
            lserrno = LSE_CONF_SYNTAX;
            return -1;
        }

        return 0;
    }

    // unquoted value: read until space/tab or end
    start = p;
    while (*p && !isspace((unsigned char)*p))
        p++;
    end = p;

    len = (size_t)(end - start);
    if (len >= value_len) {
        lserrno = LSE_BAD_ENV;
        return -1;
    }

    memcpy(value, start, len);
    value[len] = '\0';

    // skip trailing spaces
    while (*p && isspace((unsigned char)*p))
        p++;

    // extra junk after value → syntax error
    if (*p != '\0') {
        lserrno = LSE_CONF_SYNTAX;
        return -1;
    }

    return 0;
}
#endif
