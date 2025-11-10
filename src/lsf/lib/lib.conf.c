/* $Id: lib.conf.c,v 1.6 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.conf.h"
#include "lsf/lib/ll.host.h"

#define IS_DIGIT(s)  ( (s) >= '0' && (s) <= '9')

static char do_Cluster(FILE *, int *, char *);
static char do_HostTypes(FILE *, int *, char *);
static char do_HostModels(FILE *, int *, char *);
static char do_Resources(FILE *, int *, char *);
static char do_Index(FILE *, int *, char *);
static char do_Manager (FILE *, char *, int *, char *, int);
static char do_Hosts (FILE *, char *, int *, struct lsInfo *);
static char do_Clparams (FILE *, char *, int *);

static char addHostType(char *);
static char addHostModel(char *, char *, float);
static char setIndex(struct keymap *, char *, int);
static char addHost(struct hostInfo *, char *, int *);
static int  setAdmins(struct admins *, int);

static void initClusterInfo(struct clusterInfo *);
static void freeClusterInfo(struct clusterInfo *);
static void initHostInfo(struct hostInfo *);
static void freeHostInfo(struct hostInfo *);

static int  initResTable(void);
static int  resNameDefined(char *);
static void putThreshold(int, struct hostInfo *, int, char *, float);
static int  getClusAdmins(char *, char *, int *, char *, int);
static struct admins * getAdmins(char *, char *, int *, char *, int);
static int  setAdmins(struct admins *, int);
static int parse_time(char *, float *, int *);
static int validWindow(char *, char *);

struct builtIn builtInRes[] = {
    {"r15s",
     "15-second CPU run queue length",
     LS_NUMERIC, INCR, TYPE1 | RESF_RELEASE, 15},
    {"r1m" ,
     "1-minute CPU run queue length (alias: cpu)",
     LS_NUMERIC, INCR, TYPE1 | RESF_RELEASE, 15},
    {"r15m",
     "15-minute CPU run queue length",
     LS_NUMERIC, INCR, TYPE1 | RESF_RELEASE, 15},
    {"ut",
     "1-minute CPU utilization (0.0 to 1.0)",
     LS_NUMERIC, INCR , TYPE1, 15},
    {"pg",
     "Paging rate (pages/second)"  ,
     LS_NUMERIC, INCR, TYPE1, 15},
    {"io",
     "Disk IO rate (Kbytes/second)"  ,
     LS_NUMERIC, INCR, TYPE1, 15},
    {"ls",
     "Number of login sessions (alias: login)"  ,
     LS_NUMERIC, INCR, TYPE1, 30},
    {"it",
     "Idle time (minutes) (alias: idle)"  ,
     LS_NUMERIC, DECR, TYPE1, 30},
    {"tmp",
     "Disk space in /tmp (Mbytes)"  ,
     LS_NUMERIC, DECR, TYPE1, 120},
    {"swp",
     "Available swap space (Mbytes) (alias: swap)" ,
     LS_NUMERIC, DECR, TYPE1, 15},
    {"mem",
     "Available memory (Mbytes)"  ,
     LS_NUMERIC, DECR, TYPE1, 15},
    {"ncpus",
     "Number of CPUs" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"ndisks",
     "Number of local disks" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"maxmem",
     "Maximum memory (Mbytes)" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"maxswp",
     "Maximum swap space (Mbytes)" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"maxtmp",
     "Maximum /tmp space (Mbytes)" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"cpuf",
     "CPU factor" ,
     LS_NUMERIC, DECR, TYPE2, 0},
    {"type",
     "Host type" ,
     LS_STRING,  NA, TYPE2, 0},
    {"model",
     "Host model" ,
     LS_STRING,  NA, TYPE2, 0},
    {"status",
     "Host status" ,
     LS_STRING,  NA, TYPE2, 0},
    {"rexpri",
     "Remote execution priority" ,
     LS_NUMERIC, NA, TYPE2, 0},
    {"server",
     "LSF server host" ,
     LS_BOOLEAN, NA, TYPE2, 0},
    {"hname",
     "Host name" ,
     LS_STRING,  NA, TYPE2, 0},
    { NULL,
      NULL,
      LS_NUMERIC, INCR, TYPE1, 0}
};

struct sharedConf *sConf = NULL;
static struct lsInfo lsinfo;
struct clusterConf *cConf = NULL;
static struct clusterInfo clinfo;

static void freeKeyList(struct keymap *);
static int validType (char *type);
static int doResourceMap(FILE *fp, char *lsfile, int *LineNum);
static int addResourceMap (char *resName, char *location, char *lsfile,
                           int LineNum);
static int parseHostList (char *hostList, char *lsfile, int LineNum,
                          char ***hosts);
static struct lsSharedResourceInfo *addResource (char *resName, int nHosts,
                                                 char **hosts, char *value,
                                                 char *fileName, int LineNum);
static int addHostInstance (struct lsSharedResourceInfo *sharedResource,
                            int nHosts, char **hostNames, char *value);

int convertNegNotation_(char**, struct HostsArray*);
static int resolveBaseNegHosts(char*, char**, struct HostsArray*);

struct sharedConf *
ls_readshared(char *fname)
{
    FILE   *fp;
    char   *cp;
    char *word;
    char modelok, resok, clsok, typeok;
    int lineNum = 0;

    lserrno = LSE_NO_ERR;
    if (fname == NULL) {
        ls_syslog(LOG_ERR, ("%s: filename is NULL"),  "ls_readshared");
        lserrno = LSE_NO_FILE;
        return NULL;
    }

    lsinfo.nRes = 0;
    FREEUP(lsinfo.resTable);
    lsinfo.nTypes = 0;
    lsinfo.nModels = 0;
    lsinfo.numIndx = 0;
    lsinfo.numUsrIndx = 0;

    if (sConf == NULL) {
        if ((sConf =  malloc(sizeof(struct sharedConf)))
            == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "ls_readshared", "malloc")
;
            lserrno = LSE_MALLOC;
            return NULL;
        }
        sConf->lsinfo = &lsinfo;
        sConf->clusterName = NULL;
        sConf->servers = NULL;
    } else {
        FREEUP(sConf->clusterName);
        FREEUP(sConf->servers);
        sConf->lsinfo = &lsinfo;
    }
    modelok = false;
    resok = false;
    clsok = false;
    typeok = false;

    if (initResTable() < 0) {
        lserrno = LSE_MALLOC;
        return NULL;
    }

    fp = fopen(fname, "r");
    if (fp == NULL) {

        ls_syslog(LOG_ERR, ("%s: Can't open configuration file <%s>.")  , "ls_readshared", fname);
        lserrno = LSE_NO_FILE;
        return NULL;
    }

    for (;;) {
        if ((cp = getBeginLine(fp, &lineNum)) == NULL) {
            fclose(fp);
            if (! modelok) {
                ls_syslog(LOG_ERR, ("%s: HostModel section missing or invalid"),
                          fname);
            }
            if (!resok) {
                ls_syslog(LOG_ERR, ("%s: Resource section missing or invalid"),
                          fname);
            }
            if (!typeok) {
                ls_syslog(LOG_ERR, ("%s: HostType section missing or invalid"),
                          fname);
            }
            if (!clsok) {
                ls_syslog(LOG_ERR, ("%s: Cluster section missing or invalid"),
                          fname);
            }
            return sConf;
        }

        word = getNextWord_(&cp);
        if (!word) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: Section name expected after Begin; ignoring section"),
                "ls_readshared", fname, lineNum);
            doSkipSection(fp, &lineNum, fname, "unknown");
            continue;
        } else {
            if (strcasecmp(word, "host") == 0) {
                ls_syslog(LOG_INFO, "%s: %s(%d): section %s no longer needed in this version, ignored",
                          "ls_readshared", fname, lineNum, word);
                continue;
            }

            if (strcasecmp(word, "hosttype") == 0) {
                if (do_HostTypes(fp, &lineNum, fname))
                    typeok = true;
                continue;
            }

            if (strcasecmp(word, "hostmodel") == 0) {
                if (do_HostModels(fp, &lineNum, fname))
                    modelok = true;
                continue;
            }

            if (strcasecmp(word, "resource") == 0) {
                if (do_Resources(fp, &lineNum, fname))
                    resok = true;
                continue;
            }

            if (strcasecmp(word, "cluster") == 0)  {
                if (do_Cluster(fp, &lineNum, fname))
                    clsok = true;
                continue;
            }

            if (strcasecmp(word, "newindex") == 0) {
                do_Index(fp, &lineNum, fname);
                continue;
            }

            ls_syslog(LOG_ERR, ("%s: %s(%d: Invalid section name %s; ignoring section"),
                      "ls_readshared", fname, lineNum, word);
            doSkipSection(fp, &lineNum, fname, word);
        }
    }
}

static int
initResTable(void)
{
    struct resItem *resTable;
    int   i;

    resTable = calloc(1000, sizeof(struct resItem));
    if (!resTable) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "initResTable", "malloc" )
;
        return -1;
    }
    i = 0;
    lsinfo.numIndx = 0;
    lsinfo.numUsrIndx = 0;
    while(builtInRes[i].name != NULL) {
        strcpy(resTable[i].name, builtInRes[i].name);
        strcpy(resTable[i].des, builtInRes[i].des);
        resTable[i].valueType = builtInRes[i].valueType;
        resTable[i].orderType = builtInRes[i].orderType;
        resTable[i].interval  = builtInRes[i].interval;
        resTable[i].flags     = builtInRes[i].flags;
        if (resTable[i].flags &  RESF_DYNAMIC)
            lsinfo.numIndx++;
        i++;
    }
    lsinfo.nRes = i;
    lsinfo.resTable = resTable;
    return 0;
}

static char
do_Index (FILE *fp, int *lineNum, char *fname)
{
    char *linep;
    struct keymap keyList[] = {
        {"INTERVAL", NULL, 0},
        {"INCREASING", NULL, 0},
        {"DESCRIPTION", NULL, 0},
        {"NAME", NULL, 0},
        { NULL, NULL, 0}
    };

    linep = getNextLineC_(fp, lineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Index", fname, *lineNum, "index");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "newindex")) {
        ls_syslog(LOG_WARNING, ("%s: %s(%d: empty section"),
                  "do_Index", fname, *lineNum);
        return false;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section newindex; ignoring section"),
                      "do_Index", fname, *lineNum);
            doSkipSection(fp, lineNum, fname, "newindex");
            return false;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, "newindex"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section newindex; ignoring line"),
                          "do_Index", fname, *lineNum);
                continue;
            }

            setIndex(keyList, fname, *lineNum);
        }
    } else {
        if (readHvalues(keyList, linep, fp, fname,
                        lineNum, true, "newindex") <0)
            return false;
        setIndex(keyList, fname, *lineNum);
        return true;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_Index", fname, *lineNum, "newindex");
    return true;
}

static char
setIndex (struct keymap *keyList, char *fname, int linenum)
{
    int resIdx;

    if (keyList == NULL)
        return false;

    if (strlen(keyList[3].val) >= MAXLSFNAMELEN) {
        ls_syslog(LOG_ERR, ("%s: %s(%d: Name %s is too long (maximum is %d chars); ignoring index"),
                  "setIndex",  fname, linenum, keyList[3].val, MAXLSFNAMELEN-1);
        return false;
    }

    if (strpbrk(keyList[3].val, ILLEGAL_CHARS) != NULL) {
        ls_syslog(LOG_ERR, ("%s: %s(%d: illegal character (one of %s), ignoring index %s"),
                  "setIndex", fname, linenum, ILLEGAL_CHARS, keyList[3].val);
        return false;
    }

    if ((resIdx = resNameDefined(keyList[3].val)) >= 0) {
        if (!(lsinfo.resTable[resIdx].flags & RESF_DYNAMIC)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: Name %s is not a dynamic resource; ignored"),
                      "setIndex", fname, linenum, keyList[3].val);
            return false;
        }

        ls_syslog(LOG_ERR, ("%s: %s(%d: Name %s reserved or previously defined; overriding previous index definition") ,
                  "setIndex",  fname, linenum, keyList[3].val);
    } else {
        resIdx = lsinfo.nRes;
    }

    lsinfo.resTable[resIdx].interval = atoi(keyList[0].val);
    lsinfo.resTable[resIdx].orderType =
        (strcasecmp(keyList[1].val, "y") == 0) ? INCR: DECR;

    strcpy(lsinfo.resTable[resIdx].des, keyList[2].val);
    strcpy(lsinfo.resTable[resIdx].name, keyList[3].val);
    lsinfo.resTable[resIdx].valueType = LS_NUMERIC;
    lsinfo.resTable[resIdx].flags = RESF_DYNAMIC | RESF_GLOBAL;

    if (resIdx == lsinfo.nRes) {
        lsinfo.numUsrIndx++;
        lsinfo.numIndx++;
        lsinfo.nRes++;
    }

    FREEUP(keyList[0].val);
    FREEUP(keyList[1].val);
    FREEUP(keyList[2].val);
    FREEUP(keyList[3].val);
    return true;
}

static char
do_HostTypes(FILE *fp, int *lineNum, char *fname)
{
    struct keymap keyList[] = {
        {"TYPENAME", NULL, 0},
        {NULL, NULL, 0}
    };
    char *linep;

    linep = getNextLineC_(fp, lineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_HostTypes",  fname, *lineNum, "hostType");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "HostType"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section HostType, ignoring section"),
                      "do_HostTypes", fname, *lineNum);
            doSkipSection(fp, lineNum, fname, "HostType");
            return false;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, "HostType"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section cluster, ignoring line"),
                          "do_HostTypes", fname, *lineNum);
                continue;
            }

            if (strpbrk(keyList[0].val, ILLEGAL_CHARS) != NULL) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: illegal character (one of %s), ignoring type %s"),
                          "do_HostTypes", fname, *lineNum, ILLEGAL_CHARS, keyList[0].val);
                FREEUP(keyList[0].val);
                continue;
            }

            addHostType(keyList[0].val);
            FREEUP(keyList[0].val);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented", "do_HostTypes",
                  fname, *lineNum, "HostType");
        doSkipSection(fp, lineNum, fname, "HostType");
        return false;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_HostTypes", fname, *lineNum, "hostType");

    return true;
}

static char
addHostType(char *type)
{
    int i;

    if (type == NULL)
        return false;

    if (lsinfo.nTypes == LL_HOSTTYPE_MAX) {
        ls_syslog(LOG_ERR, ("%s: Too many host types defined in section HostType. You can only define up to %d host types; host type %s ignored"),
                  "addHostType", LL_HOSTTYPE_MAX, type);
        return false;
    }

    for (i=0;i<lsinfo.nTypes;i++) {
        if (strcmp(lsinfo.hostTypes[i], type) != 0)
            continue;
        ls_syslog(LOG_ERR, ("%s: host type %s multiply defined"),
                  "addHostType", type);
        return false;
    }

    strcpy(lsinfo.hostTypes[lsinfo.nTypes], type);
    lsinfo.nTypes++;

    return true;
}

static char
do_HostModels(FILE *fp, int *lineNum, char *fname)
{
    char  *linep;
    struct keymap keyList[] = {
        {"MODELNAME", NULL, 0},
        {"CPUFACTOR", NULL, 0},
        {"ARCHITECTURE", NULL, 0},
        {NULL, NULL, 0}
    };
    char *sp, *word;

    linep = getNextLineC_(fp, lineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_HostModels", fname, *lineNum, "hostModel");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "hostmodel"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section hostmodel, ignoring section"),   "do_HostModels", fname, *lineNum);
            doSkipSection(fp, lineNum, fname, "do_HostModels");
            return false;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, "hostmodel"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section hostmodel, ignoring line"),
                          "do_HostModels", fname, *lineNum);
                continue;
            }

            if (! isanumber_(keyList[1].val) || atof(keyList[1].val) <= 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: Bad cpuFactor for host model %s, ignoring line"),
                          "do_HostModels", fname, *lineNum, keyList[0].val);
                FREEUP(keyList[0].val);
                FREEUP(keyList[1].val);
                FREEUP(keyList[2].val);
                continue;
            }

            if (strpbrk(keyList[0].val, ILLEGAL_CHARS) != NULL) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: illegal character (one of %s), ignoring model %s"),
                          "do_HostModels", fname, *lineNum, ILLEGAL_CHARS, keyList[0].val);
                FREEUP(keyList[0].val);
                FREEUP(keyList[1].val);
                FREEUP(keyList[2].val);
                continue;
            }

            sp = keyList[2].val;
            if (sp && sp[0])
                while ((word = getNextWord_(&sp)) != NULL)
                    addHostModel(keyList[0].val, word, atof(keyList[1].val));
            else
                addHostModel(keyList[0].val, NULL, atof(keyList[1].val));

            FREEUP(keyList[0].val);
            FREEUP(keyList[1].val);
            FREEUP(keyList[2].val);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented", "do_HostModels",
                  fname, *lineNum, "HostModel");
        doSkipSection(fp, lineNum, fname, "HostModel");
        return false;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_HostModels", fname, *lineNum, "HostModel");
    return true;
}

static char
addHostModel(char *model, char *arch, float factor)
{
    int i;

    if (model == NULL)
        return false;

    if (lsinfo.nModels == LL_HOSTMODEL_MAX) {
        ls_syslog(LOG_ERR, ("%s: Too many host models defined in section HostModel. You can only define up to %d host models; host model %s ignored"),
                  "addHostModel", LL_HOSTMODEL_MAX, model);
        return false;
    }

    for (i = 0; i < lsinfo.nModels; ++i) {
        if (arch == 0 || strcmp(lsinfo.hostArchs[i], arch) != 0)
            continue;

        ls_syslog(LOG_ERR, "%s: Duplicate host architecture type found in section HostModel. Architecture type must be unique; host model %s ignored",
                  "addHostModel", model);

        return false;
    }

    strcpy(lsinfo.hostModels[lsinfo.nModels], model);
    strcpy(lsinfo.hostArchs[lsinfo.nModels], arch? arch: "");
    lsinfo.cpuFactor[lsinfo.nModels] = factor;
    lsinfo.nModels++;
    return true;
}

static char
do_Resources(FILE *fp, int *lineNum, char *fname)
{
    char *linep;
    struct keymap keyList[] = {
#define RKEY_RESOURCENAME 0
        {"RESOURCENAME", NULL, 0},
#define RKEY_TYPE         1
        {"TYPE", NULL, 0},
#define RKEY_INTERVAL     2
        {"INTERVAL", NULL, 0},
#define RKEY_INCREASING   3
        {"INCREASING", NULL, 0},
#define RKEY_RELEASE      4
        {"RELEASE", NULL, 0},
#define RKEY_DESCRIPTION  5
        {"DESCRIPTION", NULL, 0},
        {NULL, NULL, 0}
    };
    int nres=0;

    linep = getNextLineC_(fp, lineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Resources", fname, *lineNum, "resource");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "resource"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section resource, ignoring section"),
                      "do_Resources", fname, *lineNum);
            ls_syslog(LOG_ERR, "do_Resources: %s",linep);
            doSkipSection(fp, lineNum, fname, "resource");
            return false;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, "resource"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section resource, ignoring line"),
                          "do_Resources", fname, *lineNum);
                continue;
            }

            if (strlen(keyList[RKEY_RESOURCENAME].val) >= MAXLSFNAMELEN-1) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: Resource name %s too long in section resource. Should be less than %d characters,  ignoring line"),
                          "do_Resources", fname, *lineNum, keyList[RKEY_RESOURCENAME].val, MAXLSFNAMELEN-1);
                freeKeyList(keyList);
                continue;
            }

            if (resNameDefined(keyList[RKEY_RESOURCENAME].val) >= 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: Resource name %s reserved or previously defined. Ignoring line"),
                          "do_Resources", fname, *lineNum, keyList[RKEY_RESOURCENAME].val);
                freeKeyList(keyList);
                continue;
            }

            if (strpbrk(keyList[RKEY_RESOURCENAME].val, ILLEGAL_CHARS) != NULL) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: illegal character (one of %s): in resource name:%s, section resource, ignoring line"),
                          "do_Resources", fname, *lineNum, ILLEGAL_CHARS,
                          keyList[RKEY_RESOURCENAME].val);
                freeKeyList(keyList);
                continue;
            }

            if (IS_DIGIT (keyList[RKEY_RESOURCENAME].val[0])) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: Resource name <%s> begun with a digit is illegal; ignored"),
                          "do_Resources", fname, *lineNum,
                          keyList[RKEY_RESOURCENAME].val);
                freeKeyList (keyList);
                continue;
            }

            lsinfo.resTable[lsinfo.nRes].name[0] = '\0';
            lsinfo.resTable[lsinfo.nRes].des[0] = '\0';
            lsinfo.resTable[lsinfo.nRes].flags     = RESF_GLOBAL;
            lsinfo.resTable[lsinfo.nRes].valueType = LS_BOOLEAN;
            lsinfo.resTable[lsinfo.nRes].orderType = NA;
            lsinfo.resTable[lsinfo.nRes].interval = 0;

            strcpy(lsinfo.resTable[lsinfo.nRes].name,
                   keyList[RKEY_RESOURCENAME].val);

            if (keyList[RKEY_TYPE].val != NULL
                && keyList[RKEY_TYPE].val[0] != '\0') {
                int type;

                if ((type = validType (keyList[RKEY_TYPE].val)) >= 0)
                    lsinfo.resTable[lsinfo.nRes].valueType = type;
                else {
                    ls_syslog(LOG_ERR, ("%s: %s(%d: resource type <%s> for resource <%s> is not valid; ignoring resource <%s> in section resource"),
                              "do_Resources", fname, *lineNum, keyList[RKEY_TYPE].val, keyList[RKEY_RESOURCENAME].val, keyList[RKEY_RESOURCENAME].val);
                    freeKeyList (keyList);
                    continue;
                }
            }

            if (keyList[RKEY_INTERVAL].val != NULL && keyList[RKEY_INTERVAL].val[0] != '\0') {
                int interval;
                if ((interval = atoi (keyList[RKEY_INTERVAL].val)) > 0) {
                    lsinfo.resTable[lsinfo.nRes].interval = interval;
                    if (lsinfo.resTable[lsinfo.nRes].valueType == LS_NUMERIC)
                        lsinfo.resTable[lsinfo.nRes].flags |= RESF_DYNAMIC;
                } else {
                    ls_syslog(LOG_ERR, ("%s: %s(%d: INTERVAL <%s> for resource <%s> should be a integer greater than 0; ignoring resource <%s> in section resource"),
                              "do_Resources", fname, *lineNum, keyList[RKEY_INTERVAL].val, keyList[RKEY_RESOURCENAME].val, keyList[RKEY_RESOURCENAME].val);
                    freeKeyList (keyList);
                    continue;
                }
            }

            if (keyList[RKEY_INCREASING].val != NULL
                && keyList[RKEY_INCREASING].val[0] != '\0') {
                if (lsinfo.resTable[lsinfo.nRes].valueType == LS_NUMERIC) {
                    if (!strcasecmp (keyList[RKEY_INCREASING].val, "N"))
                        lsinfo.resTable[lsinfo.nRes].orderType = DECR;
                    else if (!strcasecmp(keyList[RKEY_INCREASING].val, "Y"))
                        lsinfo.resTable[lsinfo.nRes].orderType = INCR;
                    else {
                        ls_syslog(LOG_ERR, ("%s: %s(%d: INCREASING <%s> for resource <%s> is not valid; ignoring resource <%s> in section resource"),
                                  "do_Resources", fname, *lineNum, keyList[RKEY_INCREASING].val, keyList[RKEY_RESOURCENAME].val, keyList[RKEY_RESOURCENAME].val);
                        freeKeyList (keyList);
                        continue;
                    }
                } else
                    ls_syslog(LOG_ERR, ("%s: %s(%d: INCREASING <%s> is not used by the resource <%s> with type <%s>; ignoring INCREASING"),
                              "do_Resources", fname, *lineNum,
                              keyList[RKEY_INCREASING].val,
                              keyList[RKEY_RESOURCENAME].val,
                              /* Bug. Compare orderType with valueType
                               */
                              (lsinfo.resTable[lsinfo.nRes].orderType
                               == (enum orderType)LS_BOOLEAN) ? "BOOLEAN" : "STRING");
            } else {
                if (lsinfo.resTable[lsinfo.nRes].valueType
                    == LS_NUMERIC) {
                    ls_syslog(LOG_ERR, ("%s: %s(%d: No INCREASING specified for a numeric resource <%s>; ignoring resource <%s> in section resource"),
                              "do_Resources", fname, *lineNum, keyList[RKEY_RESOURCENAME].val, keyList[RKEY_RESOURCENAME].val);
                    freeKeyList (keyList);
                    continue;
                }
            }

            if (keyList[RKEY_RELEASE].val != NULL
                && keyList[RKEY_RELEASE].val[0] != '\0') {
                if (lsinfo.resTable[lsinfo.nRes].valueType == LS_NUMERIC) {
                    if (!strcasecmp(keyList[RKEY_RELEASE].val, "Y")) {
                        lsinfo.resTable[lsinfo.nRes].flags |= RESF_RELEASE;
                    } else if (strcasecmp(keyList[RKEY_RELEASE].val, "N")) {
                        ls_syslog(LOG_ERR, "doresources:%s(%d): RELEASE defined for resource <%s> should be 'Y', 'y', 'N' or 'n' not <%s>; ignoring resource <%s> in section resource",
                                  fname, *lineNum, keyList[RKEY_RESOURCENAME].val,
                                  keyList[RKEY_RELEASE].val,
                                  keyList[RKEY_RESOURCENAME].val);
                        freeKeyList (keyList);
                        continue;
                    }
                } else {
                    ls_syslog(LOG_ERR,"doresources:%s(%d): RELEASE cannot be defined for resource <%s> which isn't a numeric resource; ignoring resource <%s> in section resource",
                              fname, *lineNum, keyList[RKEY_RESOURCENAME].val,
                              keyList[RKEY_RESOURCENAME].val);
                    freeKeyList (keyList);
                    continue;
                }
            } else {
                if (lsinfo.resTable[lsinfo.nRes].valueType == LS_NUMERIC) {
                    lsinfo.resTable[lsinfo.nRes].flags |= RESF_RELEASE;
                }
            }

            strncpy(lsinfo.resTable[lsinfo.nRes].des,
                    keyList[RKEY_DESCRIPTION].val, MAXRESDESLEN);
            lsinfo.resTable[lsinfo.nRes].des[MAXRESDESLEN - 1] = 0;
            if (lsinfo.resTable[lsinfo.nRes].interval > 0
                && (lsinfo.resTable[lsinfo.nRes].valueType == LS_NUMERIC)) {
                lsinfo.numUsrIndx++;
                lsinfo.numIndx++;
            }
            lsinfo.nRes++;
            nres++;
            freeKeyList(keyList);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented", "do_Resources",
                  fname, *lineNum, "resource");
        return false;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_Resources", fname, *lineNum, "resource");
    return true;
}

static int
resNameDefined(char *name)
{
    int i;

    if (name == NULL)
        return -1;

    for(i = 0; i < lsinfo.nRes; i++) {
        if (strcmp(name, lsinfo.resTable[i].name) == 0)
            return i;
    }
    return -1;
}

struct clusterConf *
ls_readcluster_ex(char *fname, struct lsInfo *info, int lookupAdmins)
{
    struct lsInfo myinfo;
    char   *word;
    FILE   *fp;
    char   *cp;
    int lineNum = 0;
    int Error = false;
    int aorm = false;
    int count1, count2, i, j, k;

    lserrno = LSE_NO_ERR;
    if (fname== NULL) {
        ls_syslog(LOG_ERR, ("%s: filename is NULL"), "ls_readcluster");
        lserrno = LSE_NO_FILE;
        return NULL;
    }

    if (info == NULL) {
        ls_syslog(LOG_ERR, ("%s: LSF information is NULL"),   "ls_readcluster");
        lserrno = LSE_NO_FILE;
        return NULL;
    }

    if (cConf == NULL) {
        if ((cConf = (struct clusterConf *)
             malloc (sizeof (struct clusterConf))) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "ls_readcluster", "malloc")
;
            lserrno = LSE_MALLOC;
            return NULL;
        }
        cConf->clinfo = NULL;
        cConf->hosts = NULL;
        cConf->numHosts = 0;
        cConf->numShareRes = 0;
        cConf->shareRes = NULL;
    } else {

        for ( i = 0; i < cConf->numHosts; i ++ )
            freeHostInfo (&cConf->hosts[i]);
        FREEUP(cConf->hosts);
        cConf->numHosts = 0;

        for (i = 0; i < cConf->numShareRes; i++) {
            FREEUP(cConf->shareRes[i].resourceName);
            for (j = 0; j < cConf->shareRes[i].nInstances; j++) {
                FREEUP(cConf->shareRes[i].instances[j].value);
                for (k = 0; k < cConf->shareRes[i].instances[j].nHosts; k++) {
                    FREEUP(cConf->shareRes[i].instances[j].hostList[k]);
                }
                FREEUP(cConf->shareRes[i].instances[j].hostList);
            }
            FREEUP(cConf->shareRes[i].instances);
        }

        FREEUP(cConf->shareRes);
        cConf->shareRes = NULL;
        cConf->numShareRes = 0;
    }
    freeClusterInfo ( &clinfo );
    initClusterInfo ( &clinfo );
    cConf->clinfo = &clinfo;
    count1 = 0;
    count2 = 0;

    myinfo = *info;

    if (info->nRes && (myinfo.resTable = (struct resItem *) malloc
                       (info->nRes * sizeof(struct resItem))) == NULL ) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "ls_readcluster", "malloc")
;
        lserrno = LSE_MALLOC;
        return NULL;
    }

    for (i=0,j=0; i < info->nRes; i++) {
        if (info->resTable[i].flags & RESF_DYNAMIC) {
            memcpy(&myinfo.resTable[j], &info->resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }
    for (i=0; i < info->nRes; i++) {
        if (!(info->resTable[i].flags & RESF_DYNAMIC)) {
            memcpy(&myinfo.resTable[j], &info->resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }

    if ((fp = fopen(fname, "r")) == NULL) {
        FREEUP(myinfo.resTable);

        ls_syslog(LOG_INFO, "%s: %s(%s) failed: %m", "ls_readcluster", "fopen", fname);
        lserrno = LSE_NO_FILE;
        return NULL;
    }

    for (;;) {
        cp = getBeginLine(fp, &lineNum);
        if (!cp) {
            fclose(fp);
            if (cConf->numHosts) {
                FREEUP(myinfo.resTable);
                if (Error)
                    return NULL;
                else
                    return cConf;
            } else {
                FREEUP(myinfo.resTable);
                ls_syslog(LOG_ERR, ("%s: %s(%d: No hosts configured."),
                          "ls_readcluster", fname, lineNum);
                return cConf;
            }
        }

        word = getNextWord_(&cp);
        if (!word) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: Keyword expected after Begin. Ignoring section"),
                      "ls_readclusterfname", fname, lineNum);
            doSkipSection(fp, &lineNum, fname, "unknown");
        } else if (strcasecmp(word, "clustermanager") == 0) {
            count1++;
            if (count1 > 1)  {
                ls_syslog(LOG_ERR, ("%s: %s(%d: More than one %s section defined; ignored."),
                          "ls_readcluster", fname, lineNum, word);
                doSkipSection(fp, &lineNum, fname, word);
            } else {
                if (!do_Manager(fp, fname, &lineNum, "clustermanager", lookupAdmins) &&
                    aorm != true) {
                    Error = true;
                } else
                    aorm = true;
            }
            continue;
        } else if (strcasecmp(word, "clusteradmins") == 0) {
            count2++;
            if (count2 > 1)  {
                ls_syslog(LOG_ERR, ("%s: %s(%d: More than one %s section defined; ignored."),
                          "ls_readcluster", fname, lineNum, word);
                doSkipSection(fp, &lineNum, fname, word);
            } else {
                if (!do_Manager(fp, fname, &lineNum, "clusteradmins", lookupAdmins) &&
                    aorm != true) {
                    Error = true;
                } else
                    aorm = true;
            }
            continue;
        } else if (strcasecmp(word, "parameters") == 0) {
            if (!do_Clparams(fp, fname, &lineNum))
                Error = true;
            continue;
        } else if (strcasecmp(word, "host") == 0) {
            if (!do_Hosts(fp, fname, &lineNum, &myinfo))
                Error = true;
            continue;
        } else if (strcasecmp(word, "resourceMap") == 0) {
            if (doResourceMap(fp, fname, &lineNum) < 0)
                Error = true;
            continue;
        } else {
            ls_syslog(LOG_ERR, ("%s %s(%d: Invalid section name %s, ignoring section"),
                      "ls_readcluster", fname, lineNum, word);
            doSkipSection(fp, &lineNum, fname, word);
        }
    }
}

struct clusterConf *
ls_readcluster(char *fname, struct lsInfo *info)
{
    return ls_readcluster_ex(fname, info, true);
}

static void
freeClusterInfo (struct clusterInfo *cls)
{
    int         i;

    if (cls != NULL) {
        for ( i = 0; i < cls->nRes; i ++ )
            FREEUP(cls->resources[i]);
        for ( i = 0; i < cls->nTypes; i ++ )
            FREEUP(cls->hostTypes[i]);
        for ( i = 0; i < cls->nModels; i ++ )
            FREEUP(cls->hostModels[i]);
        for ( i = 0; i < cls->nAdmins; i ++ )
            FREEUP(cls->admins[i]);
        FREEUP(cls->admins);
        FREEUP(cls->adminIds);
    }
}

static void
initClusterInfo (struct clusterInfo *cls)
{
    if (cls != NULL) {
        strcpy ( cls->clusterName, "" );
        cls->status = 0;
        strcpy ( cls->masterName, "" );
        strcpy ( cls->managerName, "" );
        cls->managerId = 0;
        cls->numServers = 0;
        cls->numClients = 0;
        cls->nRes = 0;
        cls->resources = NULL;
        cls->nTypes = 0;
        cls->hostTypes = NULL;
        cls->nModels = 0;
        cls->hostModels = NULL;
        cls->nAdmins = 0;
        cls->adminIds = NULL;
        cls->admins = NULL;
    }
}

static void
freeHostInfo (struct hostInfo *host)
{
    int         i;

    if (host != NULL) {
        FREEUP(host->hostType);
        FREEUP(host->hostModel);
        for ( i = 0; i < host->nRes; i ++ )
            FREEUP(host->resources[i]);
        FREEUP(host->resources);
        FREEUP(host->windows);
        FREEUP(host->busyThreshold);
    }
}

static void
initHostInfo (struct hostInfo *host)
{
    if (host != NULL) {
        strcpy ( host->hostName, "" );
        host->hostType = NULL;
        host->hostModel = NULL;
        host->cpuFactor = 0;
        host->maxCpus = 0;
        host->maxMem = 0;
        host->maxSwap = 0;
        host->maxTmp = 0;
        host->nDisks = 0;
        host->nRes = 0;
        host->resources = NULL;
        host->windows = NULL;
        host->numIndx = 0;
        host->busyThreshold = NULL;
        host->isServer = 0;
        host->rexPriority = 0;
    }
}

static char
do_Manager (FILE *fp, char *fname, int *lineNum, char *secName, int lookupAdmins)
{
    char *linep;
    struct keymap keyList1[] = {
        {"MANAGER", NULL, 0},
        {NULL, NULL, 0}
    };
    struct keymap keyList2[] = {
        {"ADMINISTRATORS", NULL, 0},
        {NULL, NULL, 0}
    };
    struct keymap *keyList;

    linep = getNextLineC_(fp, lineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Manager", fname, *lineNum, secName);
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, secName))
        return false;

    if (strcmp (secName, "clustermanager") == 0)
        keyList = keyList1;
    else
        keyList = keyList2;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section %s, ignoring section"),
                      "do_Manager", fname, *lineNum, secName);
            doSkipSection(fp, lineNum, fname, secName);
            return false;
        }

        if ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, secName))
                return false;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section %s, ignoring section"),  "do_Manager",  fname, *lineNum, secName);
                doSkipSection(fp, lineNum, fname, secName);
                return false;
            }
            if (getClusAdmins (keyList[0].val, fname, lineNum, secName, lookupAdmins) < 0) {
                FREEUP(keyList[0].val);
                return false;
            } else {
                FREEUP(keyList[0].val);
                return true;
            }
        }
    } else {
        if (readHvalues(keyList, linep, fp, fname, lineNum, true, secName) <0)
            return false;
        if (getClusAdmins (keyList[0].val, fname, lineNum, secName, lookupAdmins) < 0) {
            FREEUP(keyList[0].val);
            return false;
        } else {
            FREEUP(keyList[0].val);
            return true;
        }
    }
    return true;
}

static int
getClusAdmins (char *line, char *fname, int *lineNum, char *secName, int lookupAdmins)
{
    struct admins *admins;
    static char lastSecName[40];

    admins = getAdmins (line, fname, lineNum, secName, lookupAdmins);
    if (admins->nAdmins <= 0) {

        ls_syslog(LOG_ERR, ("%s: %s(%d: No valid user for section %s: %s"),
                  "getClusAdmins", fname, *lineNum, secName, line);
        return -1;
    }
    if (strcmp (secName, "clustermanager") == 0 &&
        strcmp (lastSecName, "clusteradmins") == 0) {
        strcpy ( lastSecName, "" );
        if (setAdmins (admins, A_THEN_M) < 0)
            return -1;
    } else if (strcmp (lastSecName, "clustermanager") == 0 &&
               strcmp (secName, "clusteradmins") == 0) {
        strcpy ( lastSecName, "" );
        if (setAdmins (admins, M_THEN_A) < 0)
            return -1;
    } else {
        if (setAdmins (admins, M_OR_A) < 0)
            return -1;
    }
    strcpy (lastSecName, secName);
    return 0;
}

static struct admins *
getAdmins (char *line, char *fname, int *lineNum, char *secName, int lookupAdmins)
{
    static struct admins admins;
    static int first = true;
    int i, numAds = 0;
    char *sp, *word;
    struct passwd *pw;
    struct  group *unixGrp;

    if (first == false) {
        for (i = 0; i < admins.nAdmins; i ++)
            FREEUP (admins.adminNames[i]);
        FREEUP (admins.adminNames);

        FREEUP (admins.adminIds);
        FREEUP (admins.adminGIds);
    }
    first = false;
    admins.nAdmins = 0;
    sp = line;
    while ((word=getNextWord_(&sp)) != NULL)
        numAds++;
    if (numAds) {
        admins.adminIds = (int *) malloc (numAds * sizeof(int));
        admins.adminGIds = (int *) malloc (numAds * sizeof(int));
        admins.adminNames = (char **) malloc (numAds * sizeof (char *));
        if (admins.adminIds == NULL || admins.adminGIds == NULL ||
            admins.adminNames == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "getAdmins", "malloc")
;
            FREEUP (admins.adminIds);
            FREEUP (admins.adminGIds);
            FREEUP (admins.adminNames);
            admins.nAdmins = 0;
            lserrno = LSE_MALLOC;
            return (&admins);
        }
    } else
        return (&admins);

    sp = line;
    while ((word=getNextWord_(&sp)) != NULL) {
        if (lookupAdmins) {
            if ((pw = getpwnam2(word)) != NULL) {
                if (putInLists (word, &admins, &numAds, NULL) < 0)
                    return(&admins);
            } else if ((unixGrp = getgrnam(word)) != NULL) {
                i = 0;
                while (unixGrp->gr_mem[i] != NULL)
                    if (putInLists (unixGrp->gr_mem[i++], &admins, &numAds, NULL)
                        < 0)
                        return(&admins);

            } else {
                if (putInLists (word, &admins, &numAds, NULL) < 0)
                    return(&admins);
            }
        } else {
            if (putInLists (word, &admins, &numAds, NULL) < 0)
                return(&admins);
        }
    }
    return (&admins);
}

static int
setAdmins (struct admins *admins, int mOrA)
{
    int i, k, workNAdmins;
    int tempNAdmins, *tempAdminIds, *workAdminIds;
    char **tempAdminNames, **workAdminNames;

    tempNAdmins = admins->nAdmins + clinfo.nAdmins;
    if ( tempNAdmins ) {
        tempAdminIds = (int *) malloc (tempNAdmins *sizeof (int));
        tempAdminNames = (char **) malloc (tempNAdmins * sizeof (char *));
    } else {
        tempAdminIds = NULL;
        tempAdminNames = NULL;
    }
    if (!tempAdminIds || !tempAdminNames) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "setAdmins", "malloc")
;
        FREEUP (tempAdminIds);
        FREEUP (tempAdminNames);
        return -1;
    }
    if (mOrA == M_THEN_A) {
        workNAdmins = clinfo.nAdmins;
        workAdminIds = clinfo.adminIds;
        workAdminNames = clinfo.admins;
    } else {
        workNAdmins = admins->nAdmins;
        workAdminIds = admins->adminIds;
        workAdminNames = admins->adminNames;
    }
    for (i = 0; i < workNAdmins; i++) {
        tempAdminIds[i] = workAdminIds[i];
        if ((tempAdminNames[i] = putstr_(workAdminNames[i])) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "setAdmins", "malloc")
;

            for ( k = 0; k < i; k ++ )
                FREEUP(tempAdminNames[k]);
            FREEUP (tempAdminIds);
            FREEUP (tempAdminNames);
            return -1;
        }
    }
    tempNAdmins = workNAdmins;
    if (mOrA == M_THEN_A) {
        workNAdmins = admins->nAdmins;
        workAdminIds = admins->adminIds;
        workAdminNames = admins->adminNames;
    } else if (mOrA == A_THEN_M) {
        workNAdmins = clinfo.nAdmins;
        workAdminIds = clinfo.adminIds;
        workAdminNames = clinfo.admins;
    } else
        workNAdmins = 0;
    for (i = 0; i < workNAdmins; i++) {
        if (isInlist (tempAdminNames, workAdminNames[i], tempNAdmins))
            continue;
        tempAdminIds[tempNAdmins] = workAdminIds[i];
        if ((tempAdminNames[tempNAdmins] =
             putstr_ (workAdminNames[i])) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "setAdmins", "malloc")
;
            for ( k = 0; k < tempNAdmins; k ++ )
                FREEUP(tempAdminNames[k]);
            FREEUP (tempAdminIds);
            FREEUP (tempAdminNames);
            return -1;
        }
        tempNAdmins++;
    }
    if (clinfo.nAdmins > 0) {
        for (i = 0; i < clinfo.nAdmins; i++)
            FREEUP (clinfo.admins[i]);
        FREEUP (clinfo.adminIds);
        FREEUP (clinfo.admins);
    }
    clinfo.nAdmins = tempNAdmins;
    clinfo.adminIds = tempAdminIds;
    clinfo.admins = tempAdminNames;

    return 0;
}

static char
do_Hosts(FILE *fp, char *fname, int *lineNum, struct lsInfo *info)
{
    static struct keymap *keyList = NULL;
    struct hostInfo host;
    char   *sp;
    char   *word;
    char** resList;
    char *linep;
    int i, j, n;
    int ignoreR = false;
    int numAllocatedResources;

    FREEUP (keyList);
    if (!(keyList=(struct keymap *)
          malloc((info->numIndx+11)*sizeof(struct keymap)))) {
        return false;
    }

#define  HOSTNAME    info->numIndx
#define  MODEL       info->numIndx+1
#define  TYPE        info->numIndx+2
#define  ND          info->numIndx+3
#define  RESOURCES   info->numIndx+4
#define  RUNWINDOW   info->numIndx+5
#define  REXPRI0     info->numIndx+6
#define  SERVER0     info->numIndx+7
#define  R           info->numIndx+8
#define  S           info->numIndx+9
#define  NUM_ALLOCATED_RESOURCES 64

    initkeylist(keyList, HOSTNAME, S+1, info);
    keyList[HOSTNAME].key="HOSTNAME";
    keyList[MODEL].key="MODEL";
    keyList[TYPE].key="TYPE";
    keyList[ND].key="ND";
    keyList[RESOURCES].key="RESOURCES";
    keyList[RUNWINDOW].key="RUNWINDOW";
    keyList[REXPRI0].key="REXPRI";
    keyList[SERVER0].key="SERVER";
    keyList[R].key="R";
    keyList[S].key="S";
    keyList[S+1].key=NULL;

    initHostInfo ( &host );

    linep = getNextLineC_(fp, lineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Hosts", fname, *lineNum, "host");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "host")) {
        ls_syslog(LOG_ERR, ("%s: %s(%d: empty host section"),
                  "do_Hosts", fname, *lineNum);
        return false;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section host, ignoring section"),
                      "do_Hosts", fname, *lineNum);
            doSkipSection(fp, lineNum, fname, "host");
            return false;
        }

        i = 0;
        for (i=0; keyList[i].key != NULL; i++) {
            if (keyList[i].position != -1)
                continue;

            if ((strcasecmp("hostname", keyList[i].key) == 0) ||
                (strcasecmp("model", keyList[i].key) == 0) ||
                (strcasecmp("type", keyList[i].key) == 0) ||
                (strcasecmp("resources", keyList[i].key) == 0)) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line: key %s is missing in section host, ignoring section"),
                          "do_Hosts", fname, *lineNum, keyList[i].key);
                doSkipSection(fp, lineNum, fname, "host");
                for (j=0;keyList[j].key != NULL; j++)
                    if (keyList[j].position != -1)
                        FREEUP(keyList[j].val);
                return false;
            }
        }

        if (keyList[R].position != -1 && keyList[SERVER0].position != -1) {
            ls_syslog(LOG_WARNING, ("%s: %s(%d: keyword line: conflicting keyword definition: you cannot define both 'R' and 'SERVER'. 'R' ignored"),
                      "do_Hosts", fname, *lineNum);
            ignoreR = true;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {

            freekeyval (keyList);
            initHostInfo ( &host );

            if (isSectionEnd(linep, fname, lineNum, "host")) {
                return true;
            }

            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section host, ignoring line"),
                          "do_Hosts", fname, *lineNum);
                continue;
            }
            if (strlen(keyList[HOSTNAME].val)>MAXHOSTNAMELEN) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: too long host name, ignored."),
                          "do_Hosts", fname, *lineNum);
                continue;
            }

            struct ll_host hp;
            int cc = get_host_by_name(keyList[HOSTNAME].val, &hp);
            if (cc < 0) {
                ls_syslog(LOG_ERR, "%s: Invalid hostname %s in section host",
                          __func__, keyList[HOSTNAME].val);
                continue;
            }

            strcpy(host.hostName, hp.name);

            host.hostModel = strdup(keyList[MODEL].val);
            host.hostType = strdup(keyList[TYPE].val);
            if (keyList[ND].position != -1)
                host.nDisks = atoi(keyList[ND].val);
            else
                host.nDisks = INFINIT_INT;

            if ( info->numIndx && (host.busyThreshold = (float *) malloc
                                   (info->numIndx*sizeof(float *))) == NULL) {
                ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", "do_Hosts", "malloc");
                lserrno = LSE_MALLOC;
                freeHostInfo (&host);
                freekeyval (keyList);
                doSkipSection(fp, lineNum, fname, "host");
                return false;
            }

            putThreshold(R15S, &host, keyList[R15S].position,
                         keyList[R15S].val, INFINITY);
            putThreshold(R1M, &host, keyList[R1M].position,
                         keyList[R1M].val, INFINITY);
            putThreshold(R15M, &host, keyList[R15M].position,
                         keyList[R15M].val, INFINITY);
            putThreshold(UT, &host, keyList[UT].position,
                         keyList[UT].val, INFINITY);
            if (host.busyThreshold[UT] > 1.0
                && host.busyThreshold[UT] < INFINITY) {
                ls_syslog(LOG_INFO, ("%s: %s(%d: value for threshold ut <%2.2f> is greater than 1, assumming <%5.1f%%>"), "do_Hosts", fname, *lineNum, host.busyThreshold[UT], host.busyThreshold[UT]);
                host.busyThreshold[UT] /= 100.0;
            }
            putThreshold(PG, &host, keyList[PG].position,
                         keyList[PG].val, INFINITY);
            putThreshold(IO, &host, keyList[IO].position,
                         keyList[IO].val, INFINITY);
            putThreshold(LS, &host, keyList[LS].position,
                         keyList[LS].val, INFINITY);
            putThreshold(IT, &host, keyList[IT].position,
                         keyList[IT].val, -INFINITY);
            putThreshold(TMP, &host, keyList[TMP].position,
                         keyList[TMP].val, -INFINITY);
            putThreshold(SWP, &host, keyList[SWP].position,
                         keyList[SWP].val, -INFINITY);
            putThreshold(MEM, &host, keyList[MEM].position,
                         keyList[MEM].val, -INFINITY);

            for (i=NBUILTINDEX; i < NBUILTINDEX+info->numUsrIndx; i++) {
                if (info->resTable[i].orderType == INCR)
                    putThreshold(i, &host, keyList[i].position,
                                 keyList[i].val, INFINITY);
                else
                    putThreshold(i, &host, keyList[i].position,
                                 keyList[i].val, -INFINITY);
            }

            for (i = NBUILTINDEX+info->numUsrIndx; i < info->numIndx; i++)
                host.busyThreshold[i] = INFINITY;

            host.numIndx = info->numIndx;

            numAllocatedResources = NUM_ALLOCATED_RESOURCES;
            resList = (char **)calloc(numAllocatedResources,
                                      sizeof(char *));
            if (resList == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "calloc")
;
            }

            n = 0;
            sp = keyList[RESOURCES].val;
            while ((word = getNextWord_(&sp)) != NULL) {
                for ( i = 0; i < n; i ++ )
                    if (!strcmp(word, resList[i]))
                        break;
                if ( i < n ) {
                    ls_syslog(LOG_ERR, ("%s: %s(%d: Resource <%s> multiply specified for host %s in section host. Ignored."),
                              "do_Hosts", fname, *lineNum, word, host.hostName);
                    continue;
                } else {

                    if (n >= numAllocatedResources) {
                        numAllocatedResources = 2*numAllocatedResources;

                        resList =
                            (char **)realloc(resList,
                                             numAllocatedResources
                                             *(sizeof(char *)));
                        if (resList == NULL) {
                            lserrno = LSE_MALLOC;

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "calloc")
;
                            freeHostInfo (&host);
                            freekeyval (keyList);
                            doSkipSection(fp, lineNum, fname, "host");
                            return false;
                        }
                    }

                    if ((resList[n] = putstr_(word)) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "malloc")
;
                        lserrno = LSE_MALLOC;
                        for ( j = 0; j < n; j++ )
                            FREEUP(resList[j]);
                        FREEUP(resList);
                        freeHostInfo (&host);
                        freekeyval (keyList);
                        doSkipSection(fp, lineNum, fname, "host");
                        return false;
                    }
                    n++;
                }
            }
            resList[n] = NULL;
            host.nRes = n;
            if ( n && (host.resources = (char **) malloc
                       (n * sizeof(char *))) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "malloc")
;
                lserrno = LSE_MALLOC;
                for ( j = 0; j < n; j ++ )
                    FREEUP(resList[j]);
                FREEUP(resList);
                freeHostInfo (&host);
                freekeyval (keyList);
                doSkipSection(fp, lineNum, fname, "host");
                return false;
            }

            for (i=0; i<n; i++) {
                if ((host.resources[i] = putstr_ ( resList[i] )) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "malloc")
;
                    lserrno = LSE_MALLOC;
                    for ( j = 0; j < n; j ++ )
                        FREEUP(resList[j]);
                    FREEUP(resList);
                    freeHostInfo (&host);
                    freekeyval (keyList);
                    doSkipSection(fp, lineNum, fname, "host");
                    return false;
                }
            }

            for ( j = 0; j < n; j ++ )
                FREEUP(resList[j]);
            FREEUP(resList);

            host.rexPriority = DEF_REXPRIORITY;

            if (keyList[REXPRI0].position != -1) {
                host.rexPriority = atoi(keyList[REXPRI0].val);
            }

            host.isServer = 1;
            if (keyList[R].position != -1) {
                if (!ignoreR)
                    host.isServer = atoi(keyList[R].val);
            }

            if (keyList[SERVER0].position != -1) {
                host.isServer = atoi(keyList[SERVER0].val);
            }

            host.windows = NULL;
            if (keyList[RUNWINDOW].position != -1)  {
                if (strcmp(keyList[RUNWINDOW].val, "") == 0)
                    host.windows = NULL;
                else {
                    host.windows = parsewindow (keyList[RUNWINDOW].val,
                                                fname, lineNum, "Host" );
                    if (host.windows == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Hosts", "malloc")
;
                        lserrno = LSE_MALLOC;
                        freeHostInfo (&host);
                        freekeyval (keyList);
                        doSkipSection(fp, lineNum, fname, "host");
                        return false;
                    }
                }
            }

            addHost(&host, fname, lineNum);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented", "do_Hosts",
                  fname, *lineNum, "host");
        doSkipSection(fp, lineNum, fname, "host");
        return false;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_Hosts", fname, *lineNum, "host");
    return true;
}

static void
putThreshold(int indx, struct hostInfo *host,
             int position, char *val, float def)
{
    if (host == NULL)
        return;

    if (position != -1) {
        if (strcmp(val, "") == 0)
            host->busyThreshold[indx] = def;
        else
            host->busyThreshold[indx] = atof(val);
    } else
        host->busyThreshold[indx] = def;

}

static char
addHost(struct hostInfo *host, char *fname, int *lineNum)
{
    struct hostInfo *newlist;
    int i;

    if (host == NULL)
        return false;

    for (i = 0; i < cConf->numHosts; i++) {

        if (!equal_host(cConf->hosts[i].hostName, host->hostName))
            continue;

        ls_syslog(LOG_WARNING, ("%s: %s(%d: host <%s> redefined, using previous definition"),
                  "addHost", fname, *lineNum, host->hostName);
        freeHostInfo(host);
        return false;
    }

    cConf->numHosts++;
    newlist = calloc(cConf->numHosts, sizeof(struct hostInfo));
    if (newlist == NULL) {
        ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", "addHost", "calloc",
                  cConf->numHosts*sizeof(struct hostInfo));
        cConf->numHosts--;
        lserrno = LSE_MALLOC;
        /* Bug if we failed to calloc() at this early stage
         * just core dump as something is seriosly wrong....
         */
        abort();
        return false;
    }

    for (i = 0; i < cConf->numHosts - 1; i++)
        newlist[i] = cConf->hosts[i];
    newlist[cConf->numHosts-1] = *host;
    FREEUP(cConf->hosts);
    cConf->hosts = newlist;

    return true;
}

void
initkeylist(struct keymap keyList[], int m, int n, struct lsInfo *info)
{
    int i, index;

    for (i=0; i < m-1; i++)
        keyList[i].key = "";

    for (i=0; i < n; i++) {
        keyList[i].val = NULL;
        keyList[i].position = 0;
    }

    if (info == NULL) {
        i = 0;
        index = 0;
        while (builtInRes[i].name != NULL ) {
            if (builtInRes[i].flags & RESF_DYNAMIC)
                keyList[index++].key = builtInRes[i].name;
            i++;
        }
    } else {
        index = 0;
        for (i=0; i < info->nRes; i++) {
            if ( (info->resTable[i].flags & RESF_DYNAMIC)
                 && index < info->numIndx )
                keyList[index++].key = info->resTable[i].name;
        }
    }

    return;
}

void
freekeyval (struct keymap keylist[])
{
    int cc;
    for (cc = 0; keylist[cc].key != NULL; cc++) {
        if (keylist[cc].val != NULL) {
            FREEUP (keylist[cc].val);
        }
    }
}

char *
parsewindow(char *linep, char *fname, int *lineNum, char *section)
{
    char        *sp, *windows, *word, *save;

    if ( linep == NULL )
        return NULL;

    sp = linep;

    windows = putstr_ ( sp );
    if (windows == NULL)
        return NULL;

    *windows = '\0';
    while ((word = getNextWord_(&sp)) != NULL) {
        save = putstr_ (word);
        if (save == NULL) {
            FREEUP(windows);
            return NULL;
        }
        if ( validWindow(word, section) < 0 ) {
            ls_syslog(LOG_ERR, "File %s section %s at line %d: Bad time expression <%s>; ignored.", fname, section, *lineNum, save);
            lserrno = LSE_CONF_SYNTAX;
            FREEUP(save);
            continue;
        }
        if ( *windows != '\0')
            strcat ( windows, " " );
        strcat ( windows, save );
        FREEUP(save);
    }

    if (windows[0] == '\0') {
        FREEUP(windows);
    }
    return windows;

}

static int
validWindow (
    char *wordpair,
    char *context
    )
{
    int  oday, cday;
    float ohour, chour;
    char *sp;
    char *word;

    sp = strchr(wordpair, '-');
    if (!sp) {
        ls_syslog(LOG_ERR, ("Bad time expression in %s"), context);
        return -1;
    }

    *sp = '\0';
    sp++;
    word = sp;

    if (parse_time(word, &chour, &cday) < 0) {
        ls_syslog(LOG_ERR, ("Bad time expression in %s"), context);
        return -1;
    }

    word = wordpair;

    if (parse_time(word, &ohour, &oday) < 0) {
        ls_syslog(LOG_ERR, ("Bad time expression in %s"), context);
        return -1;
    }

    if (((oday && cday) == 0) && (oday != cday)) {
        ls_syslog(LOG_ERR, ("Ambiguous time in %s"), context);
        return -1;
    }

    return 0;

}

static int
parse_time (char *word, float *hour, int *day)
{
    float min;
    char *sp;

    *day = 0;
    *hour = 0.0;
    min  = 0.0;

    sp = strrchr(word, ':');
    if (!sp) {
        if (!isint_(word) || atoi (word) < 0)
            return -1;
        *hour = atof(word);
        if (*hour > 23)
            return -1;

    }
    else {
        *sp = '\0';
        sp++;

        if (!isint_(sp) || atoi (sp) < 0)
            return -1;

        min = atoi(sp);
        if (min > 59)
            return -1;

        sp = strrchr(word, ':');
        if (!sp) {
            if (!isint_(word) || atoi (word) < 0)
                return -1;
            *hour = atof(word);
            if (*hour > 23)
                return -1;
        }
        else {
            *sp = '\0';
            sp++;
            if (!isint_(sp) || atoi (sp) < 0)
                return -1;

            *hour = atof(sp);
            if (*hour > 23)
                return -1;

            if (!isint_(word) || atoi (word) < 0)
                return -1;

            *day  = atoi(word);
            if (*day == 0)
                *day = 7;
            if (*day < 1 || *day > 7)
                return -1;
        }
    }

    *hour = *hour + min/60.0;

    return 0;

}

static char
do_Cluster(FILE *fp, int *lineNum, char *fname)
{
    char *linep;
    struct keymap keyList[] = {
        {"CLUSTERNAME", NULL, 0},
        {"SERVERS", NULL, 0},
        {NULL, NULL, 0}
    };
    char *servers;
    bool_t found = false;

    linep = getNextLineC_(fp, lineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Cluster", fname, *lineNum, "cluster");
        return false;
    }

    if (isSectionEnd(linep, fname, lineNum, "cluster"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section cluster; ignoring section"),
                      "do_Cluster", fname, *lineNum);
            doSkipSection(fp, lineNum, fname, "cluster");
            return false;
        }

        if (keyList[0].position == -1) {
            ls_syslog(LOG_ERR,("%s: %s(%d: keyword line: key %s is missing in section cluster; ignoring section"),
                      "do_Cluster", fname, *lineNum, keyList[0].key);
            doSkipSection(fp, lineNum, fname, "cluster");
            return false;
        }

        while ((linep = getNextLineC_(fp, lineNum, true)) != NULL) {
            if (isSectionEnd(linep, fname, lineNum, "cluster"))
                return true;
            if (found )
                return true;

            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for section cluster, ignoring line"),
                          "do_Cluster", fname, *lineNum);
                continue;
            }

            if (keyList[1].position != -1)
                servers = keyList[1].val;
            else
                servers = NULL;

            if ((sConf->clusterName = putstr_(keyList[0].val))
                == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Cluster", "malloc")
;
                FREEUP(keyList[0].val);
                if (keyList[1].position != -1)
                    FREEUP(keyList[1].val);
                return false;
            }

            if ((sConf->servers = putstr_(servers))
                == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "do_Cluster", "malloc")
;
                FREEUP(keyList[0].val);
                if (keyList[1].position != -1)
                    FREEUP(keyList[1].val);
                return false;
            }

            found = true;
            FREEUP(keyList[0].val);
            if (keyList[1].position != -1)
                FREEUP(keyList[1].val);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented",
                  "do_Cluster", fname, *lineNum, "cluster");
        doSkipSection(fp, lineNum, fname, "cluster");
        return false;
    }

    ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
              "do_Cluster", fname, *lineNum, "cluster");
    return false;
}

static char
do_Clparams (FILE *clfp, char *lsfile, int *LineNum)
{
    char *linep;
    struct keymap keyList[] = {
#define EXINTERVAL              0
        {"EXINTERVAL", NULL, 0},
#define ELIMARGS                1
        {"ELIMARGS", NULL, 0},
#define PROBE_TIMEOUT           2
        {"PROBE_TIMEOUT", NULL, 0},
#define ELIM_POLL_INTERVAL      3
        {"ELIM_POLL_INTERVAL", NULL, 0},
#define HOST_INACTIVITY_LIMIT   4
        {"HOST_INACTIVITY_LIMIT", NULL, 0},
#define MASTER_INACTIVITY_LIMIT 5
        {"MASTER_INACTIVITY_LIMIT", NULL, 0},
#define RETRY_LIMIT             6
        {"RETRY_LIMIT", NULL, 0},
#define ADJUST_DURATION         7
        {"ADJUST_DURATION", NULL, 0},
#define LSF_ELIM_DEBUG 8
        {"LSF_ELIM_DEBUG", NULL, 0},
#define LSF_ELIM_BLOCKTIME 9
        {"LSF_ELIM_BLOCKTIME", NULL, 0},
#define LSF_ELIM_RESTARTS 10
        {"LSF_ELIM_RESTARTS", NULL, 0},
        {NULL, NULL, 0}
    };
    linep = getNextLineC_(clfp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  "do_Clparams", lsfile, *LineNum, "parameters");
        return false;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "parameters")) {
        return true;
    }

    if (strchr(linep, '=') == NULL) {
        ls_syslog(LOG_ERR, ("%s: %s(%d: vertical section not supported, ignoring section"),
                  "do_Clparams", lsfile, *LineNum);
        doSkipSection(clfp, LineNum, lsfile, "parameters");
        return false;
    } else {
        if (readHvalues(keyList, linep, clfp, lsfile, LineNum, false,
                        "parameters") <0)
            return false;
        return true;
    }
}

static void
freeKeyList(struct keymap *keyList)
{
    int i;

    for(i=0; keyList[i].key != NULL; i++)
        if (keyList[i].position != -1)
            FREEUP(keyList[i].val);
}

static int
validType (char *type)
{
    if (type == NULL)
        return -1;

    if (!strcasecmp (type, "Boolean"))
        return LS_BOOLEAN;

    if (!strcasecmp (type, "String"))
        return LS_STRING;

    if (!strcasecmp (type, "Numeric"))
        return LS_NUMERIC;

    if (!strcmp (type, "!"))
        return LS_EXTERNAL;

    return -1;
}

static int
doResourceMap(FILE *fp, char *lsfile, int *LineNum)
{
    static char fname[] = "doResourceMap";

    char *linep;
    struct keymap keyList[] = {
#define RKEY_RESOURCE_NAME  0
        {"RESOURCENAME", NULL, 0},
#define RKEY_LOCATION    1
        {"LOCATION", NULL, 0},
        {NULL, NULL, 0}
    };
    int resNo = 0;

    linep = getNextLineC_(fp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, "%s: premature EOF while reading %s",
                  fname, lsfile, *LineNum, "resourceMap");
        return -1;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "resourceMap")) {
        ls_syslog(LOG_WARNING, "%s: %s %d: Empty resourceMap, no keywords or resources defined.",
            fname, lsfile, *LineNum);
        return -1;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, ("%s: %s(%d: keyword line format error for section resource, ignoring section"), fname, lsfile, *LineNum);
            doSkipSection(fp, LineNum, lsfile, "resourceMap");
            return -1;
        }

        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "resourceMap"))
                return 0;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: values do not match keys for resourceMap section, ignoring line"), fname, lsfile, *LineNum);
                continue;
            }

            if ((resNo =resNameDefined(keyList[RKEY_RESOURCE_NAME].val)) < 0) {
                ls_syslog(LOG_ERR, ("%s: %s(%d: Resource name <%s> is  not defined; ignoring line"), fname, lsfile, *LineNum, keyList[RKEY_RESOURCE_NAME].val);
                freeKeyList (keyList);
                continue;
            }
            if (keyList[RKEY_LOCATION].val != NULL
                && keyList[RKEY_LOCATION].val[0] != '\0') {

                if (strstr(keyList[RKEY_LOCATION].val, "all ") &&
                    strchr(keyList[RKEY_LOCATION].val, '~')) {

                    struct HostsArray array;
                    int cnt;
                    int result;

                    array.size = 0;
                    array.hosts = malloc(cConf->numHosts * sizeof(char*));
                    if (!array.hosts) {
                        ls_syslog(LOG_ERR, "doresourcemap",  "malloc");
                        freeKeyList (keyList);
                        return -1;
                    }
                    for (cnt = 0; cnt < cConf->numHosts; cnt++) {
                        array.hosts[array.size] =
                            strdup(cConf->hosts[cnt].hostName);
                        if (!array.hosts[array.size]) {
                            freeSA_(array.hosts, array.size);
                            freeKeyList (keyList);
                            return -1;
                        }
                        array.size++;
                    }

                    result = convertNegNotation_(&(keyList[RKEY_LOCATION].val),
                                                   &array);
                    if (result == 0) {
                        ls_syslog(LOG_WARNING,
                                  "%s: %s(%d): convertNegNotation_: all the hosts are to be excluded %s !",
                                  fname, lsfile, *LineNum, keyList[RKEY_LOCATION].val);
                    } else if (result < 0) {
                        ls_syslog(LOG_WARNING, "%s: %s(%d): convertNegNotation_: Wrong syntax \'%s\'",
                                  fname, lsfile, *LineNum, keyList[RKEY_LOCATION].val);
                    }

                    freeSA_(array.hosts, array.size);
                }

                if (addResourceMap (keyList[RKEY_RESOURCE_NAME].val,
                                    keyList[RKEY_LOCATION].val, lsfile, *LineNum) < 0) {
                    ls_syslog(LOG_ERR, "%s: %s(%d): addResourceMap() failed for resource <%s>; ignoring line",
                              fname, lsfile, *LineNum,
                              keyList[RKEY_RESOURCE_NAME].val);
                    freeKeyList (keyList);
                    continue;
                }

                lsinfo.resTable[resNo].flags &= ~RESF_GLOBAL;
                lsinfo.resTable[resNo].flags |= RESF_SHARED;
                resNo = 0;
            } else {
                ls_syslog(LOG_ERR, ("%s: %s(%d: No LOCATION specified for resource <%s>; ignoring the line"), fname, lsfile, *LineNum, keyList[RKEY_RESOURCE_NAME].val);
                freeKeyList (keyList);
                continue;
            }
            freeKeyList (keyList);
        }
    } else {
        ls_syslog(LOG_ERR, "%s not implemented",  fname, lsfile, *LineNum, "resource");
        return -1;
    }
    return 0;

}

static int
addResourceMap (char *resName, char *location, char *lsfile, int LineNum)
{
    static char fname[] = "addResourceMap";
    struct lsSharedResourceInfo *resource;
    int i, j, numHosts = 0, first = true, error;
    char **hosts = NULL, initValue[MAXFILENAMELEN], *sp, *cp, ssp, *instance;
    char externalResourceFlag[]="!";
    char *tempHost;

    if (resName == NULL || location == NULL) {
        ls_syslog (LOG_ERR, ("%s: %s(%d: Resource name <%s> location <%s>"), fname, lsfile, LineNum, (resName?resName:"NULL"), (location?location:"NULL"));
        return -1;
    }

    if (!strcmp(location, "!")) {
        initValue[0] = '\0';
        tempHost = (char *)externalResourceFlag;
        hosts = &tempHost;
        if ((resource = addResource (resName, 1,
                                     hosts, initValue, lsfile, LineNum)) == NULL) {
            ls_syslog (LOG_ERR, "%s: %s(%d): %s() failed; ignoring the instance <%s>",
                       fname, lsfile, LineNum, "addResource", "!");
            return -1;
        }
        return 0;
    }

    resource = NULL;
    sp = location;

    i = 0;
    while (*sp != '\0') {
        if (*sp == '[')
            i++;
        else if (*sp == ']')
            i--;
        sp++;
    }
    sp = location;
    if (i != 0) {
        ls_syslog (LOG_ERR, ("%s: %s(%d: number of '[' is not match that of ']' in <%s> for resource <%s>; ignoring"), fname, lsfile, LineNum, location, resName);
        return -1;
    }

    while (sp != NULL && sp[0] != '\0') {
        for (j = 0; j < numHosts; j++)
            FREEUP (hosts[j]);
        FREEUP (hosts);
        numHosts = 0;
        error = false;
        instance = sp;
        initValue[0] = '\0';
        while (*sp == ' ' && *sp != '\0')
            sp++;
        if (*sp == '\0') {
            if (first == true)
                return -1;
            else
                return 0;
        }
        cp = sp;
        while (isalnum (*cp))
            cp++;
        if (cp != sp) {
            ssp = cp[0];
            cp[0] = '\0';
            strcpy (initValue, sp);
            cp[0] = ssp;
            if (isspace(*cp))
                cp++;
            if (*cp != '@')
                error = true;
            sp = cp + 1;
        }
        if (isspace(*sp))
            sp++;

        if (*sp != '[' && *sp != '\0') {
            ls_syslog (LOG_ERR, ("%s: %s(%d: Bad character <%c> in instance; ignoring"), fname, lsfile, LineNum, *sp);
            sp++;
        }
        if (isspace(*sp))
            sp++;
        if (*sp == '[') {
            sp++;
            cp = sp;
            while (*sp != ']' && *sp != '\0')
                sp++;
            if (*sp == '\0') {
                ls_syslog (LOG_ERR, ("%s: %s(%d: Bad format for instance <%s>; ignoring the instance"), fname, lsfile, LineNum, instance);
                return -1;
            }
            if (error == true) {
                sp++;
                ssp =  *sp;
                *sp = '\0';
                ls_syslog (LOG_ERR, ("%s: %s(%d: Bad format for instance <%s>; ignoringthe instance"), fname, lsfile, LineNum, instance);
                *sp = ssp;
                continue;
            }
            *sp = '\0';
            sp++;
            if ((numHosts = parseHostList (cp, lsfile, LineNum, &hosts)) <= 0) {
                ls_syslog (LOG_ERR, ("%s: %s(%d: %s(%s) failed; ignoring the instance <%s%s>"),
                           fname, lsfile, LineNum, "parseHostList", cp, instance, "]");
                continue;
            }

            if (resource == NULL) {
                if ((resource = addResource (resName, numHosts,
                                             hosts, initValue, lsfile, LineNum)) == NULL)
                    ls_syslog (LOG_ERR, ("%s: %s(%d: %s() failed; ignoring the instance <%s>"),
                               fname, lsfile, LineNum, "addResource", instance);
            } else {
                if (addHostInstance (resource, numHosts, hosts,
                                     initValue) < 0)
                    ls_syslog (LOG_ERR, ("%s: %s(%d: %s() failed; ignoring the instance <%s>"),
                               fname, lsfile, LineNum, "addHostInstance", instance);
            }
            continue;
        } else {
            ls_syslog (LOG_ERR, ("%s: %s(%d: No <[>  for instance in <%s>; ignoring"), fname, lsfile, LineNum, location);
            while (*sp != ']' && *sp != '\0')
                sp++;
            if (*sp == '\0')
                return -1;
            sp++;
        }
    }
    for (j = 0; j < numHosts; j++)
        FREEUP (hosts[j]);
    FREEUP (hosts);
    return 0;

}

static int
parseHostList (char *hostList, char *lsfile, int LineNum, char ***hosts)
{
    static char fname[] = "parseHostList";
    char *host, *sp, **hostTable;
    int numHosts = 0, i;

    if (hostList == NULL)
        return -1;

    sp = hostList;
    while ((host = getNextWord_(&sp)) != NULL)
        numHosts++;
    if ((hostTable = (char **) malloc (numHosts * sizeof(char *))) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc")
;
        return -1;
    }
    sp = hostList;
    numHosts = 0;
    while ((host = getNextWord_(&sp)) != NULL) {
        if ((hostTable[numHosts] = putstr_(host)) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "malloc")
;
            for (i = 0; i < numHosts; i++)
                FREEUP (hostTable[i]);
            FREEUP (hostTable);
            return -1;
        }
        numHosts++;
    }
    if (numHosts == 0) {
        FREEUP (hostTable);
        return -1;
    }
    *hosts = hostTable;
    return numHosts;

}

static struct lsSharedResourceInfo *
addResource (char *resName, int nHosts, char **hosts, char *value,
             char *fileName, int LineNum)
{
    int nRes;
    struct lsSharedResourceInfo *resInfo;

    if (resName == NULL || hosts == NULL)
        return NULL;

    if ((resInfo = (struct lsSharedResourceInfo *)
         myrealloc(cConf->shareRes, sizeof (struct lsSharedResourceInfo) * (cConf->numShareRes + 1))) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "addHostResource", "myrealloc")
;
        return NULL;
    }

    cConf->shareRes = resInfo;
    nRes = cConf->numShareRes;

    if ((resInfo[nRes].resourceName = putstr_(resName)) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "addHostResource", "malloc")
;
        return NULL;
    }

    resInfo[nRes].nInstances = 0;
    resInfo[nRes].instances = NULL;
    if (addHostInstance (resInfo + nRes, nHosts, hosts, value) < 0) {
        free(resInfo[nRes].resourceName);
        return NULL;
    }

    cConf->numShareRes++;

    return (resInfo+nRes);

}

static int
addHostInstance (struct lsSharedResourceInfo *sharedResource,  int nHosts,
                 char **hostNames, char *value)
{

    int i, inst;
    struct  lsSharedResourceInstance *instance;

    if (nHosts <= 0 || hostNames == NULL)
        return -1;

    instance = (struct lsSharedResourceInstance *) myrealloc(sharedResource->instances, sizeof(struct lsSharedResourceInstance) * (sharedResource->nInstances + 1));

    if (instance == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "addHostInstance", "myrealloc")
;
        return -1;
    }

    sharedResource->instances = instance;
    inst = sharedResource->nInstances;

    if ((instance[inst].value = putstr_(value)) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "addHostInstance", "putstr_")
;
        return -1;
    }

    instance[inst].nHosts = nHosts;
    if ((instance[inst].hostList = (char **) malloc(sizeof(char *) * nHosts))
        == NULL) {
        ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", "addHostInstance", "malloc");
        free(instance[inst].value);
        return -1;
    }

    for (i = 0; i < nHosts; i++) {
        if ((instance[inst].hostList[i] = putstr_(hostNames[i])) == NULL) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "addHostInstance", "putstr_")
;
            for (i--; i >= 0; i--)
                free(instance[inst].hostList[i]);
            free(instance[inst].hostList);
            free(instance[inst].value);
            return -1;
        }
    }

    sharedResource->nInstances++;

    return 0;

}

int convertNegNotation_(char** value, struct HostsArray* array)
{
    char* buffer = strdup(value[0]);
    char* sp1 = strstr(buffer, "all ");
    char* sp2 = sp1;
    char* ptr;
    int   cnt;
    char* save     = NULL;
    char* outHosts = NULL;
    int   result   = -1;

    if (!buffer) {
        lserrno = LSE_MALLOC;
        goto clean_up;
    }

    for (cnt = 0; (sp2 > buffer) && sp2[0] != '['; cnt++) {
        sp2--;
    }

    if (!sp2 || sp2 < buffer) {
        goto clean_up;
    }

    if (cnt > 1) {
        memmove(sp2 + 1, sp1, strlen(sp1) + 1);
    }

    sp1 = sp2;
    while (sp2 && sp2[0] != ']') {
        sp2++;
    }

    if (!sp1 || !sp2) {
        goto clean_up;
    }

    ls_syslog(LOG_DEBUG, "convertNegNotation_: the original string is \'%s\'",
              value[0]);

    ptr = sp1;
    save = getNextValueQ_(&sp1, '[', ']');
    if (!save) {
        goto clean_up;
    }

    result = resolveBaseNegHosts(save, &outHosts, array);
    if (result >= 0) {
        char* new_value;

        *ptr = 0;
        new_value = malloc(strlen(buffer) + strlen(outHosts) + strlen(sp2) + 2);
        if (!new_value) {
            lserrno = LSE_MALLOC;
            goto clean_up;
        }
        strcpy(new_value, buffer);
        strcat(new_value, "[");
        strcat(new_value, outHosts);
        strcat(new_value, sp2);

        FREEUP(value[0]);
        value[0] = new_value;
    }

clean_up:

    if (lserrno == LSE_MALLOC) {

  ls_syslog(LOG_ERR, "%s: %s failed: %m", "convertNegNotation_", "malloc")
;
    }

    FREEUP(buffer);
    FREEUP(outHosts);

    return result;

}

void
freeSA_(char **list, int num)
{
    int i;

    if (list == NULL || num <= 0)
        return;

    for (i =0; i < num; i++)
        FREEUP (list[i]);
    FREEUP (list);
}

static int resolveBaseNegHosts(char* inHosts, char** outHosts, struct HostsArray* array)
{
    char*  buffer = strdup(inHosts);
    char*  save   = buffer;
    char** inTable  = NULL;
    char** outTable = NULL;
    int    in_num  = 0;
    int    neg_num = 0;
    int    j, k;
    char*  word;
    int    size = 0;

    inTable  = array->hosts;
    in_num   = array->size;
    outTable = malloc(array->size * sizeof(char*));
    if (!buffer || !inTable || !outTable) {
        lserrno = LSE_MALLOC;
        goto error_clean_up;
    }

    if ((word = getNextWord_(&buffer)) != NULL) {
        if (strcmp(word, "all")) {
            goto error_clean_up;
        }
        for (j = 0; j < in_num; j++) {
            size += strlen(inTable[j]);
        }
    } else {
        goto error_clean_up;
    }

    while ((word = getNextWord_(&buffer)) != NULL) {
        if (word[0] == '~') {
            word++;
            if (!isalnum(word[0])) {
                goto error_clean_up;
            }
        } else {
            continue;
        }

        outTable[neg_num] = strdup(word);
        if (!outTable[neg_num]) {
            lserrno = LSE_MALLOC;
            goto error_clean_up;
        }
        neg_num++;
        if (((neg_num - array->size) % array->size) == 0) {
            outTable = realloc(outTable, (array->size + neg_num) * sizeof(char*));
            if (!outTable) {
                lserrno = LSE_MALLOC;
                goto error_clean_up;
            }
        }
    }

    for (j = 0; j < neg_num; j++) {
        for (k = 0; k < in_num; k++) {
            if (inTable[k] && equal_host(inTable[k], outTable[j])) {
                size -= strlen(inTable[k]);
                free(inTable[k]);
                inTable[k] = NULL;
            }
        }
        FREEUP(outTable[j]);
    }

    outHosts[0] = malloc(size + in_num);
    if (!outHosts[0]) {
        lserrno = LSE_MALLOC;
        goto error_clean_up;
    }

    outHosts[0][0] = 0;
    for (j = 0, k = 0; j < in_num; j++) {
        if (inTable[j]) {
            strcat(outHosts[0], (const char*)inTable[j]);
            FREEUP(inTable[j]);
            strcat(outHosts[0], " ");
            k++;
        }
    }
    if (outHosts[0][0]) {
        outHosts[0][strlen(outHosts[0]) - 1] = '\0';
    }

    FREEUP(outTable);
    FREEUP(save);

    return k;

error_clean_up:

    freeSA_(outTable, neg_num);
    FREEUP(buffer);
    FREEUP(outHosts[0]);
    FREEUP(save);

    return -1;

}
