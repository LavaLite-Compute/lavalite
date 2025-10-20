/* $Id: lim.conf.c,v 1.8 2007/08/15 22:18:53 tmizan Exp $
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

#include "lsf/lim/lim.h"

#define NL_SETN 24

struct hostEntry {
    char    hostName[MAXHOSTNAMELEN];
    char    hostModel[MAXLSFNAMELEN];
    char    hostType[MAXLSFNAMELEN];
    int     rcv;
    int     nDisks;
    float   cpuFactor;
    float   *busyThreshold;
    int     nRes;
    char    **resList;
    int     rexPriority;
};

struct sharedResourceInstance *sharedResourceHead = NULL ;
struct lsInfo allInfo;
struct shortLsInfo shortInfo;

hTab hostModelTbl;
int sizeOfResTable =0;
static int numofhosts = 0;
char mcServersSet = false;

extern int ELIMdebug, ELIMrestarts, ELIMblocktime;

#define M_THEN_A  1
#define A_THEN_M  2
#define M_OR_A    3

#define ILLEGAL_CHARS     ".!-=+*/[]@:&|{}'`\""

static struct hostNode *addHost(struct clusterNode *, struct hostEntry *, char *, char *, int *);
static char addHostType(char *);
static char dotypelist(FILE *fp, int *LineNum, char *lsfile);
static char addHostModel(char *, char *, float);
static struct clusterNode *addCluster(char *, char *);
static char doclist(FILE *, int *, char *);
static int doclparams (FILE *, char *, int *);
static int dohosts(FILE *, struct clusterNode *, char *, int *);
static char dohostmodel(FILE *, int *, char *);
static char doresources(FILE *, int *, char *);
static int doresourcemap(FILE *, char *, int *);
static char  doindex(FILE *fp, int *LineNum, char *lsfile);
static int readCluster2(struct clusterNode *clPtr);
static int domanager (FILE *clfp, char *lsfile, int *LineNum, char *secName);
static char setIndex (struct keymap *keyList, char *lsfile, int linenum );
static void putThreshold(int, struct hostEntry *, int, char *, float);
static int modelNameToNo(char *);
static int configCheckSum(char *, u_short *);
static int reCheckClusterClass(struct clusterNode *);

static void initResTable(void);
static char * findClusterServers(char *);
static int getClusAdmins (char *, char *, int *, char *);
static int setAdmins (struct admins *, int);
static struct admins * getAdmins (char *, char *, int *, char *);
static void freeKeyList(struct keymap *);

static void addMapBits (int, int *, int *);
static int validType (char *);
static void initResItem(struct resItem *);
static struct sharedResource *addResource (char *, int, char **, char *, char *,
                                           int, int);
static void freeSharedRes (struct sharedResource *);
static int addHostInstance (struct sharedResource *,  int, char **, char *, int);
static struct resourceInstance * addInstance (struct sharedResource *,  int, char **, char *);
static struct resourceInstance * initInstance (void);
static void freeInstance (struct resourceInstance *);
static int addHostList (struct resourceInstance *, int, char **);
static int doresourcemap(FILE *, char *, int *);
static int addResourceMap (char *, char *, char *, int, int *);
static int parseHostList (char *, char *, int, char ***, int *);
static int addHostNodeIns (struct resourceInstance *, int, char **);
static struct  resourceInstance * isInHostNodeIns(char *, int,
                                                  struct  resourceInstance **);
static char * validLocationHost (char *);
static char ** getValidHosts (char *, int *, struct sharedResource *);
static void adjIndx (void);
static int doubleResTable (char *, int);
static int adjHostListOrder();
static int saveHostIPAddr(struct hostNode *, struct hostent *);

extern int  convertNegNotation_(char**, struct HostsArray*);

FILE * confOpen(char *filename, char *type);
float mykey(void);

static void setExtResourcesDefDefault(char *);
static int setExtResourcesDef(char *);
static int setExtResourcesLoc(char *, int);
void *ExtResDLHandle;
extern struct extResInfo *getExtResourcesDef(char *);
extern char *getExtResourcesLoc(char *);
static char *getExtResourcesValDefault(char *);
extern char *getExtResourcesVal(char *);

#define VCL_VERSION     2


int
readShared(void)
{
    static char fname[] = "readShared()";
    FILE   *fp;
    char   *cp;
    char lsfile[MAXFILENAMELEN];
    char *word;
    char modelok, resok, clsok, indxok, typeok;
    int LineNum = 0;

    modelok = false;
    resok = false;
    clsok = false;
    indxok = true;
    typeok = false;

    initResTable();

    sprintf(lsfile, "%s/lsf.shared", limParams[LSF_CONFDIR].paramValue);


    if (configCheckSum(lsfile, &lsfSharedCkSum) < 0) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "configCheckSum");
        return -1;
    }
    fp = confOpen(lsfile, "r");
    if (!fp) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5200,
                                         "%s: Can't open configuration file <%s>: %m"), /* catgets 5200 */
                  fname, lsfile);
        return -1;
    }

    for (;;) {
        if ((cp = getBeginLine(fp, &LineNum)) == NULL) {
            FCLOSEUP(&fp);
            if (! modelok) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5201,
                                                 "%s: HostModel section missing or invalid in %s"), /* catgets 5201 */
                          fname, lsfile);
            }
            if (!resok) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5202,
                                                 "%s: Warning: Resource section missing or invalid in %s"), /* catgets 5202 */
                          fname, lsfile);
            }
            if (!typeok) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5203,
                                                 "%s: HostType section missing or invalid"), /* catgets 5203 */
                          lsfile);
            }
            if (!indxok) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5204,
                                                 "%s: Warning: attempt to define too many new indices"), /* catgets 5204 */
                          lsfile);
            }
            if (!clsok) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5205,
                                                 "%s: Cluster section missing or invalid"), /* catgets 5205 */
                          lsfile);
            }
            if (modelok && resok && clsok && typeok)
                return 0;
            else
                return -1;
        }


        word = getNextWord_(&cp);
        if (!word) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5206,
                                             "%s: %s(%d): Section name expected after Begin; ignoring section"), /* catgets 5206 */
                      fname, lsfile, LineNum);
            lim_CheckError = WARNING_ERR;
            doSkipSection(fp, &LineNum, lsfile, "unknown");
            continue;
        } else {
            if (strcasecmp(word, "host") == 0) {
                ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd, NL_SETN, 5380, "%s: %s(%d): section %s no longer needed in this version, ignored")), /* catgets 5380 */ "readShared", lsfile, LineNum, word);
                continue;
            }

            if (strcasecmp(word, "hosttype") == 0) {
                if (dotypelist(fp, &LineNum, lsfile))
                    typeok = true;
                continue;
            }

            if (strcasecmp(word, "hostmodel") == 0) {
                if (dohostmodel(fp, &LineNum, lsfile))
                    modelok = true;
                continue;
            }

            if (strcasecmp(word, "resource") == 0) {
                if (doresources(fp, &LineNum, lsfile))
                    resok = true;
                continue;
            }

            if (strcasecmp(word, "cluster") == 0)  {
                if (doclist(fp, &LineNum, lsfile))
                    clsok = true;
                continue;
            }

            if (strcasecmp(word, "newindex") == 0) {
                if (! doindex(fp, &LineNum, lsfile))
                    indxok = false;
                continue;
            }

            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5207,
                                             "%s: %s(%d): Invalid section name %s; ignoring section"), /* catgets 5207 */
                      fname, lsfile, LineNum, word);
            lim_CheckError = WARNING_ERR;
            doSkipSection(fp, &LineNum, lsfile, word);
        }
    }

}

static char
doindex (FILE *fp, int *LineNum, char *lsfile)
{
    static char fname[] = "doindex()";
    char *linep;
    struct keymap keyList[] = {
        {"INTERVAL", NULL, 0},
        {"INCREASING", NULL, 0},
        {"DESCRIPTION", NULL, 0},
        {"NAME", NULL, 0},
        { NULL, NULL, 0}
    };

    linep = getNextLineC_(fp, LineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5208,
                                         "%s: %s(%d): Premature EOF"), /* catgets 5208 */
                  fname, lsfile, *LineNum);
        lim_CheckError = WARNING_ERR;
        return true;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "newindex")) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5209,
                                             "%s: %s(%d): empty section"), /* catgets 5209 */
                  fname, lsfile, *LineNum);
        return true;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5210,
                                             "%s: %s(%d): keyword line format error for section newindex; ignoring section"), /* catgets 5210 */
                      fname,  lsfile, *LineNum);
            lim_CheckError = WARNING_ERR;
            doSkipSection(fp, LineNum, lsfile, "newindex");
            return true;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "newindex"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5211,
                                                 "%s: %s(%d): values do not match keys for section newindex; ignoring line"), fname, lsfile, *LineNum); /* catgets 5211 */
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if (!setIndex(keyList, lsfile, *LineNum)) {
                doSkipSection(fp, LineNum, lsfile, "newindex");
                freeKeyList (keyList);
                return false;
            }
            freeKeyList (keyList);
        }
    } else {
        if (readHvalues(keyList, linep, fp, lsfile,
                        LineNum, true, "newindex") <0)
            return true;
        if (! setIndex(keyList, lsfile, *LineNum)) {
            return false;
        }
        return true;
    }

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5212,
                                     "%s: %s(%d): Premature EOF"), /* catgets 5212 */
              fname, lsfile, *LineNum);
    lim_CheckError = WARNING_ERR;
    return true;

}

static char
setIndex (struct keymap *keyList, char *lsfile, int linenum)
{
    static char fname[] = "setIndex()";
    int resIdx, i;


    if (strlen(keyList[3].val) >= MAXLSFNAMELEN) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5213,
                                         "%s: %s(%d): Name %s is too long (maximum is %d chars); ignoring index"), /* catgets 5213 */
                  fname, lsfile, linenum, keyList[3].val, MAXLSFNAMELEN-1);
        lim_CheckError = WARNING_ERR;
        return true;
    }

    if (strpbrk(keyList[3].val, ILLEGAL_CHARS) != NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5214,
                                         "%s: %s(%d): illegal character (one of %s)"), /* catgets 5214 */
                  fname, lsfile, linenum, ILLEGAL_CHARS);
        lim_CheckError = WARNING_ERR;
        return true;
    }
    if (IS_DIGIT (keyList[3].val[0])) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5215,
                                         "%s: %s(%d): Index name <%s> begun with a digit is illegal; ignored"), /* catgets 5215 */
                  fname, lsfile, linenum, keyList[3].val);
        lim_CheckError = WARNING_ERR;
        return true;
    }

    if ((resIdx = resNameDefined(keyList[3].val)) >= 0) {
        if (!(allInfo.resTable[resIdx].flags & RESF_DYNAMIC)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5216,
                                             "%s: %s(%d): Name <%s> is not a dynamic resource; ignored"), /* catgets 5216 */
                      fname, lsfile, linenum, keyList[3].val);
            return true;
        }
        if ((allInfo.resTable[resIdx].flags & RESF_BUILTIN))
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5217,
                                             "%s: %s(%d): Name <%s> reserved or previously defined;"), /* catgets 5217 */
                      fname, lsfile, linenum, keyList[3].val);
        else {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5219,
                                             "%s: %s(%d): Name <%s> reserved or previously defined;ignoring"), /* catgets 5219 */
                      fname, lsfile, linenum, keyList[3].val);
            lim_CheckError = WARNING_ERR;
            return true;
        }
    } else {
        resIdx = allInfo.nRes;
    }
    if (resIdx >= sizeOfResTable && doubleResTable(lsfile, linenum) < 0)
        return false;

    initResItem(&allInfo.resTable[resIdx]);
    allInfo.resTable[resIdx].interval = atoi(keyList[0].val);
    allInfo.resTable[resIdx].orderType =
        (strcasecmp(keyList[1].val, "y") == 0) ? INCR: DECR;

    strcpy(allInfo.resTable[resIdx].des, keyList[2].val);
    strcpy(allInfo.resTable[resIdx].name, keyList[3].val);
    allInfo.resTable[resIdx].valueType = LS_NUMERIC;
    allInfo.resTable[resIdx].flags = RESF_DYNAMIC | RESF_GLOBAL;


    if (allInfo.numUsrIndx+NBUILTINDEX >=li_len-1) {
        li_len <<= 1;
        if (!(li=(struct liStruct *)
              realloc(li, li_len*sizeof(struct liStruct)))) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return true;
        }
    }

    if (resIdx == allInfo.nRes) {

        li[NBUILTINDEX + allInfo.numUsrIndx].increasing =
            (strcasecmp(keyList[1].val, "y") == 0);
        if ((li[NBUILTINDEX + allInfo.numUsrIndx].name =
             putstr_(keyList[3].val)) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            return true;
        }
        allInfo.numUsrIndx++;
        allInfo.numIndx++;
        allInfo.nRes++;
    } else {

        for (i = 0; i < NBUILTINDEX + allInfo.numUsrIndx; i++) {
            if (!strcasecmp(keyList[3].val, li[i].name)) {
                li[i].increasing = (strcasecmp(keyList[1].val, "y") == 0);
                break;
            }
        }
    }

    defaultRunElim = true;


    return true;

}

static char
doclist(FILE *fp, int *LineNum, char *lsfile)
{
    static char fname[] = "doclist()";
    char *linep;
    struct keymap keyList[] = {
        {"CLUSTERNAME", NULL, 0},
        {"SERVERS", NULL, 0},
        {NULL, NULL, 0}
    };
    char *servers;
    bool_t clusterAdded = false;

    linep = getNextLineC_(fp, LineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5222,
                                         "%s: %s(%d): section cluster: Premature EOF"), /* catgets 5222 */
                  fname, lsfile, *LineNum);
        return false;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "cluster"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5223,
                                             "%s: %s(%d): keyword line format error for section cluster; ignoring section"), fname, lsfile, *LineNum); /* catgets 5223 */
            doSkipSection(fp, LineNum, lsfile, "cluster");
            return false;
        }

        if (keyList[0].position == -1) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5224,
                                             "%s: %s(%d): keyword line: key %s is missing in section cluster; ignoring section"), /* catgets 5224 */
                      fname, lsfile, *LineNum, keyList[0].key);
            doSkipSection(fp, LineNum, lsfile, "cluster");
            return false;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "cluster"))
                return true;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5225,
                                                 "%s: %s(%d): values do not match keys for section cluster, ignoring line"), /* catgets 5225 */
                          fname, lsfile, *LineNum);
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if (keyList[1].position != -1) {
                servers = keyList[1].val;
                mcServersSet = true;
            }
            else
                servers = NULL;


            if (!clusterAdded && !addCluster(keyList[0].val, servers)) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5226,
                                                 "%s: Ignoring cluster %s"), /* catgets 5226 */
                          fname, keyList[0].val);
                lim_CheckError = WARNING_ERR;
            } else if (clusterAdded ) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5226,
                                                 "%s: Ignoring cluster %s"), /* catgets 5226 */
                          fname, keyList[0].val);
                lim_CheckError = WARNING_ERR;
            }

            FREEUP(keyList[0].val);
            if (keyList[1].position != -1)
                FREEUP(keyList[1].val);
            clusterAdded = true;
        }
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5227,
                                         "%s: %s(%d): horizontal cluster section not implemented yet, ignoring section"), fname, lsfile, *LineNum); /* catgets 5227 */
        doSkipSection(fp, LineNum, lsfile, "cluster");
        return false;
    }

    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF, fname, lsfile, *LineNum,
              "cluster");
    return false;
}

static char
dotypelist(FILE *fp, int *LineNum, char *lsfile)
{
    static char fname[] = "dotypelist()";
    struct keymap keyList[] = {
        {"TYPENAME", NULL, 0},
        {NULL, NULL, 0}
    };
    char *linep;


    linep = getNextLineC_(fp, LineNum, true);
    if (!linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF, fname, lsfile, *LineNum,
                  "HostType");
        return false;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "HostType"))
        return false;

    if (allInfo.nTypes <= 0) {
        allInfo.nTypes = 2;
    }

    if (shortInfo.nTypes <= 0) {
        shortInfo.nTypes = 2;
    }

    strcpy(allInfo.hostTypes[0], "UNKNOWN_AUTO_DETECT");
    shortInfo.hostTypes[0] = allInfo.hostTypes[0];
    strcpy(allInfo.hostTypes[1], "DEFAULT");
    shortInfo.hostTypes[1] = allInfo.hostTypes[1];

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5230,
                                             "%s: %s(%d): keyword line format error for section HostType, ignoring section"), fname, lsfile, *LineNum); /* catgets 5230 */
            doSkipSection(fp, LineNum, lsfile, "HostType");
            return false;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "HostType"))
            {
                return true;
            }
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5231,
                                                 "%s: %s(%d): values do not match keys for section cluster, ignoring line"), fname, lsfile, *LineNum); /* catgets 5231 */
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if (strpbrk(keyList[0].val, ILLEGAL_CHARS) != NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5232,
                                                 "%s: %s(%d): illegal character (one of %s), ignoring type %s"), /* catgets 5232 */
                          fname, lsfile, *LineNum, ILLEGAL_CHARS, keyList[0].val);
                lim_CheckError = WARNING_ERR;
                FREEUP(keyList[0].val);
                continue;
            }
            if (IS_DIGIT (keyList[0].val[0])) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5233,
                                                 "%s: %s(%d): Type name <%s> begun with a digit is illegal; ignored"), /* catgets 5233 */
                          fname, lsfile, *LineNum, keyList[0].val);
                lim_CheckError = WARNING_ERR;
                FREEUP(keyList[0].val);
                continue;
            }
            if (!addHostType(keyList[0].val))
                lim_CheckError = WARNING_ERR;

            FREEUP(keyList[0].val);
        }
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5234,
                                         "%s: %s(%d): horizontal HostType section not implemented yet, ignoring section"), fname, lsfile, *LineNum); /* catgets 5234 */
        doSkipSection(fp, LineNum, lsfile, "HostType");
        return false;
    }

    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF, fname, lsfile, *LineNum,
              "HostType");
    return false;

}

static char
dohostmodel(FILE *fp, int *LineNum, char *lsfile)
{
    static char fname[] = "dohostmodel()";
    static char first = true;
    char  *linep;
    int new;
    hEnt *hashEntPtr;
    float *floatp;
    struct keymap keyList[] = {
        {"MODELNAME", NULL, 0},
        {"CPUFACTOR", NULL, 0},
        {"ARCHITECTURE", NULL, 0},
        {NULL, NULL, 0}
    };
    char *sp, *word;


    if (first) {
        int i;
        for(i = 0; i < MAXMODELS; ++i) {
            allInfo.cpuFactor[i] = 1.0;
            allInfo.modelRefs[i] = 0;
        }

        h_initTab_(&hostModelTbl, 0);
        first = false;
    }


    linep = getNextLineC_(fp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF, fname, lsfile, *LineNum,
                  "hostmodel");
        return false;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "hostmodel"))
        return false;

    if (allInfo.nModels <= 0) {
        memset(allInfo.modelRefs, 0, sizeof(int) * MAXMODELS);
        allInfo.nModels = 2;
    }
    if (shortInfo.nModels <= 0) {
        shortInfo.nModels = 2;
    }

    strcpy(allInfo.hostModels[0], "UNKNOWN_AUTO_DETECT");
    strcpy(allInfo.hostArchs[0], "UNKNOWN_AUTO_DETECT");
    allInfo.cpuFactor[0] = 1;
    shortInfo.hostModels[0] = allInfo.hostModels[0];
    strcpy(allInfo.hostModels[1], "DEFAULT");
    strcpy(allInfo.hostArchs[1], "");
    allInfo.cpuFactor[1] = 1;
    shortInfo.hostModels[1] = allInfo.hostModels[1];

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5237,
                                             "%s: %s(%d): keyword line format error for section hostmodel, ignoring section"), fname, lsfile, *LineNum); /* catgets 5237 */
            doSkipSection(fp, LineNum, lsfile, "dohostmodel");
            return false;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "hostmodel"))
            {
                return true;
            }
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5238,
                                                 "%s: %s(%d): values do not match keys for section hostmodel, ignoring line"), fname, lsfile, *LineNum); /* catgets 5238 */
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if (! isanumber_(keyList[1].val) || atof(keyList[1].val) <= 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5239,
                                                 "%s: %s(%d): Bad cpuFactor for host model %s, ignoring line"), /* catgets 5239 */
                          fname, lsfile, *LineNum, keyList[0].val);
                lim_CheckError = WARNING_ERR;
                FREEUP(keyList[0].val);
                FREEUP(keyList[1].val);
                FREEUP(keyList[2].val);
                continue;
            }

            if (strpbrk(keyList[0].val, ILLEGAL_CHARS) != NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5240,
                                                 "%s: %s(%d): illegal character (one of %s), ignoring model %s"), /* catgets 5240 */
                          fname, lsfile, *LineNum, ILLEGAL_CHARS, keyList[0].val);
                lim_CheckError = WARNING_ERR;
                FREEUP(keyList[0].val);
                FREEUP(keyList[1].val);
                FREEUP(keyList[2].val);
                continue;
            }
            if (IS_DIGIT (keyList[0].val[0])) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5241,
                                                 "%s: %s(%d): Model name <%s> begun with a digit is illegal; ignored"), fname, lsfile, *LineNum, keyList[0].val); /* catgets 5241 */
                lim_CheckError = WARNING_ERR;
                FREEUP(keyList[0].val);
                FREEUP(keyList[1].val);
                FREEUP(keyList[2].val);
                continue;
            }

            sp = keyList[2].val;
            if (sp && sp[0]) {
                while ((word = getNextWord_(&sp)) != NULL) {
                    if (!addHostModel(keyList[0].val, word,
                                      atof(keyList[1].val))) {
                        ls_syslog(LOG_ERR,
                                  _i18n_msg_get(ls_catd , NL_SETN, 5242,
                                                "%s: %s(%d): Too many host models, ignoring model %s"), fname, lsfile, *LineNum, keyList[0].val); /* catgets 5242 */
                        lim_CheckError = WARNING_ERR;
                        goto next_value;
                    }
                }
            } else {
                if (!addHostModel(keyList[0].val, NULL,
                                  atof(keyList[1].val))) {
                    ls_syslog(LOG_ERR,
                              _i18n_msg_get(ls_catd , NL_SETN, 5242,
                                            "%s: %s(%d): Too many host models, ignoring model %s"), fname, lsfile, *LineNum, keyList[0].val); /* catgets 5242 */
                    lim_CheckError = WARNING_ERR;
                    goto next_value;
                }

            }


            hashEntPtr = h_addEnt_(&hostModelTbl, keyList[0].val, &new);

            if (new) {
                floatp  = (float *) malloc(sizeof(float));
                if (floatp == NULL) {
                    ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M,
                              fname, "malloc", sizeof(float));
                    doSkipSection(fp, LineNum, lsfile, "HostModel");
                    return false;
                }
                *floatp = atof(keyList[1].val);
                hashEntPtr->hData = (int *) floatp;
            } else {
                floatp = (float *) hashEntPtr->hData;
                *floatp = atof(keyList[1].val);
                hashEntPtr->hData = (int *) floatp;
            }

        next_value:
            FREEUP(keyList[0].val);
            FREEUP(keyList[1].val);
            FREEUP(keyList[2].val);
        }
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5244,
                                         "%s: %s(%d): horizontal HostModel section not implemented yet, ignoring section"),  fname, lsfile, *LineNum); /* catgets 5244 */
        doSkipSection(fp, LineNum, lsfile, "HostModel");
        return false;
    }

    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
              fname, lsfile, *LineNum, "HostModel");
    return false;
}



static void
initResTable(void)
{
    static char fname[] = "initResTable()";
    struct resItem *resTable;
    int   i;


    if ((resTable = (struct resItem *)
         malloc(300 * sizeof(struct resItem))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        lim_Exit(fname);
    }
    sizeOfResTable = 300;
    i=0;
    allInfo.numIndx = 0;
    allInfo.numUsrIndx = 0;
    while(builtInRes[i].name != NULL) {
        strcpy(resTable[i].name, builtInRes[i].name);
        strcpy(resTable[i].des,
               _i18n_msg_get(ls_catd, MOD_LSF_LIB,
                             builtInRes_ID[i], builtInRes[i].des));
        resTable[i].valueType = builtInRes[i].valueType;
        resTable[i].orderType = builtInRes[i].orderType;
        resTable[i].interval  = builtInRes[i].interval;
        resTable[i].flags     = builtInRes[i].flags;
        if ((resTable[i].flags & RESF_DYNAMIC) &&
            (resTable[i].valueType == LS_NUMERIC))
            allInfo.numIndx++;
        i++;
    }
    allInfo.nRes = i;
    allInfo.resTable = resTable;
    return;

}

int
resNameDefined (char *name)
{
    int i;

    for(i=0; i < allInfo.nRes; i++) {
        if (strcmp(name, allInfo.resTable[i].name) == 0)
            return i;
    }
    return -1;
}

static char
doresources(FILE *fp, int *LineNum, char *lsfile)
{

    static char fname[] = "doresources()";
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
#define RKEY_DESCRIPTION 5
        {"DESCRIPTION", NULL, 0},
        {NULL, NULL, 0}
    };
    int nres=0;
    int resIdx;


    linep = getNextLineC_(fp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                  fname, lsfile, *LineNum,  "resource");
        return false;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "resource"))
        return false;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5248,
                                             "%s: %s(%d): keyword line format error for section resource, ignoring section"), fname, lsfile, *LineNum); /* catgets 5248 */
            doSkipSection(fp, LineNum, lsfile, "resource");
            return false;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "resource")) {
                return true;
            }

            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5249,
                                                 "%s: %s(%d): values do not match keys for section resource, ignoring line"), fname, lsfile, *LineNum); /* catgets 5249 */
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if (strlen(keyList[RKEY_RESOURCENAME].val) >= MAXLSFNAMELEN-1) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5250,
                                                 "%s: %s(%d): Resource name %s too long in section resource. Should be less than %d characters. Ignoring line"), /* catgets 5250 */
                          fname, lsfile, *LineNum, keyList[0].val, MAXLSFNAMELEN-1);
                lim_CheckError = WARNING_ERR;
                freeKeyList (keyList);
                continue;
            }

            if ((resIdx = resNameDefined(keyList[RKEY_RESOURCENAME].val)) >= 0){

                if ((allInfo.resTable[resIdx].flags & RESF_BUILTIN)
                    && (allInfo.resTable[resIdx].flags & RESF_DYNAMIC)) {

                    if ( keyList[RKEY_TYPE].val && *keyList[RKEY_TYPE].val
                         && allInfo.resTable[resIdx].valueType ==
                         validType (keyList[RKEY_TYPE].val)
                         && allInfo.resTable[resIdx].orderType ==
                         !strcasecmp (keyList[RKEY_INCREASING].val, "N")){

                        allInfo.resTable[resIdx].flags &= ~RESF_BUILTIN;
                    }
                    else{

                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5251,
                                                         "%s: %s(%d): Built-in resource %s can't be overrided with different type or increasing. Ignoring line"),
                                  fname, lsfile, *LineNum, keyList[0].val); /* catgets 5251 */
                        lim_CheckError = WARNING_ERR;
                    }
                }
                else{

                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5251,
                                                     "%s: %s(%d): Resource name %s reserved or previously defined. Ignoring line"),
                              fname, lsfile, *LineNum, keyList[0].val); /* catgets 5251 */
                    lim_CheckError = WARNING_ERR;
                }
                freeKeyList (keyList);
                continue;
            }

            if (strpbrk(keyList[RKEY_RESOURCENAME].val, ILLEGAL_CHARS)
                != NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5252,
                                                 "%s: %s(%d): illegal character (one of %s): in resource name:%s, section resource, ignoring line"),  /* catgets 5252 */
                          fname, lsfile, *LineNum, ILLEGAL_CHARS, keyList[0].val);
                lim_CheckError = WARNING_ERR;
                freeKeyList (keyList);
                continue;
            }
            if (IS_DIGIT (keyList[RKEY_RESOURCENAME].val[0])) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5253,
                                                 "%s: %s(%d): Resource name <%s> begun with a digit is illegal; ignored"), fname, lsfile, *LineNum, keyList[0].val); /* catgets 5253 */
                lim_CheckError = WARNING_ERR;
                freeKeyList (keyList);
                continue;
            }
            if (allInfo.nRes >= sizeOfResTable
                && doubleResTable(lsfile, *LineNum) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "doubleResTable");
                return false;
            }
            initResItem(&allInfo.resTable[allInfo.nRes]);
            strcpy(allInfo.resTable[allInfo.nRes].name,
                   keyList[RKEY_RESOURCENAME].val);


            if (keyList[RKEY_TYPE].val != NULL
                && keyList[RKEY_TYPE].val[0] != '\0') {
                int type;


                if (strcmp(keyList[RKEY_TYPE].val, LSF_LIM_ERES_TYPE) == 0)
                {
                    if (setExtResourcesDef(keyList[RKEY_RESOURCENAME].val) != 0)
                    {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5256,
                                                         "%s: Ignoring the external resource <%s>(%d) in section resource of file %s"),  /* catgets 5256 */
                                  fname, keyList[RKEY_RESOURCENAME].val, *LineNum, lsfile);
                        lim_CheckError = WARNING_ERR;
                        freeKeyList (keyList);
                        continue;
                    }
                    allInfo.nRes++;
                    nres++;
                    freeKeyList (keyList);
                    continue;
                }
                if ((type = validType (keyList[RKEY_TYPE].val)) >= 0)
                    allInfo.resTable[allInfo.nRes].valueType = type;
                else {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5257,
                                                     "%s: %s(%d): resource type <%s> for resource <%s> is not valid; ignoring resource <%s> in section resource"),  /* catgets 5257 */
                              fname, lsfile, *LineNum, keyList[RKEY_TYPE].val, keyList[RKEY_RESOURCENAME].val, keyList[RKEY_RESOURCENAME].val);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }
            } else {
                if (logclass & LC_TRACE)
                    ls_syslog(LOG_DEBUG3, "doresources: %s(%d): Resource type is not defined for resource <%s>; The resource will be assigned type <boolean>",
                              lsfile, *LineNum, keyList[RKEY_RESOURCENAME].val);
                allInfo.resTable[allInfo.nRes].valueType = LS_BOOLEAN;
            }


            if (keyList[RKEY_INTERVAL].val != NULL && keyList[RKEY_INTERVAL].val[0] != '\0') {
                int interval;
                if ((interval = atoi (keyList[RKEY_INTERVAL].val)) > 0) {
                    allInfo.resTable[allInfo.nRes].interval = interval;
                    allInfo.resTable[allInfo.nRes].flags |= RESF_DYNAMIC;
                } else {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5258,
                                                     "%s: %s(%d): INTERVAL <%s> for resource <%s> should be a integer greater than 0; ignoring resource <%s> in section resource"),  /* catgets 5258 */
                              fname,
                              lsfile,
                              *LineNum,
                              keyList[RKEY_INTERVAL].val,
                              keyList[RKEY_RESOURCENAME].val,
                              keyList[RKEY_RESOURCENAME].val);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }
            }

            if (keyList[RKEY_INCREASING].val != NULL
                && keyList[RKEY_INCREASING].val[0] != '\0') {
                if (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC) {
                    if (!strcasecmp (keyList[RKEY_INCREASING].val, "N"))
                        allInfo.resTable[allInfo.nRes].orderType = DECR;
                    else if (!strcasecmp(keyList[RKEY_INCREASING].val, "Y"))
                        allInfo.resTable[allInfo.nRes].orderType = INCR;
                    else {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5259,
                                                         "%s: %s(%d): INCREASING <%s> for resource <%s> is not valid; ignoring resource <%s> in section resource"),  /* catgets 5259 */
                                  fname,
                                  lsfile,
                                  *LineNum,
                                  keyList[RKEY_INCREASING].val,
                                  keyList[RKEY_RESOURCENAME].val,
                                  keyList[RKEY_RESOURCENAME].val);
                        lim_CheckError = WARNING_ERR;
                        freeKeyList (keyList);
                        continue;
                    }
                } else
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5260,
                                                     "%s: %s(%d): INCREASING <%s> is not used by the resource <%s> with type <%s>; ignoring INCREASING"),    /* catgets 5260 */
                              fname,
                              lsfile,
                              *LineNum,
                              keyList[RKEY_INCREASING].val,
                              keyList[RKEY_RESOURCENAME].val,
                              (allInfo.resTable[allInfo.nRes].orderType == LS_BOOLEAN)?"BOOLEAN":"STRING");
            } else {
                if (allInfo.resTable[allInfo.nRes].valueType
                    == LS_NUMERIC) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5261,
                                                     "%s: %s(%d): No INCREASING specified for a numeric resource <%s>; ignoring resource <%s> in section resource"), /* catgets 5261 */
                              fname,
                              lsfile,
                              *LineNum,
                              keyList[RKEY_RESOURCENAME].val,
                              keyList[RKEY_RESOURCENAME].val);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }
            }



            if (keyList[RKEY_RELEASE].val != NULL
                && keyList[RKEY_RELEASE].val[0] != '\0') {
                if (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC) {
                    if (!strcasecmp(keyList[RKEY_RELEASE].val, "Y")) {
                        allInfo.resTable[allInfo.nRes].flags |= RESF_RELEASE;
                    } else if (strcasecmp(keyList[RKEY_RELEASE].val, "N")) {
                        ls_syslog(LOG_ERR, I18N(5474,
                                                "doresources:%s(%d): RELEASE defined for resource <%s> should be 'Y', 'y', 'N' or 'n' not <%s>; ignoring resource <%s> in section resource"), /*catgets 5474*/
                                  lsfile, *LineNum, keyList[RKEY_RESOURCENAME].val,
                                  keyList[RKEY_RELEASE].val,
                                  keyList[RKEY_RESOURCENAME].val);
                        lim_CheckError = WARNING_ERR;
                        freeKeyList (keyList);
                        continue;
                    }
                } else {

                    ls_syslog(LOG_ERR, I18N(5475,
                                            "doresources:%s(%d): RELEASE cannot be defined for resource <%s> which isn't a numeric resource; ignoring resource <%s> in section resource"), /*catgets 5475 */
                              lsfile, *LineNum, keyList[RKEY_RESOURCENAME].val,
                              keyList[RKEY_RESOURCENAME].val);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }
            } else {

                if (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC) {
                    allInfo.resTable[allInfo.nRes].flags |= RESF_RELEASE;
                }
            }

            strncpy(allInfo.resTable[allInfo.nRes].des,
                    keyList[RKEY_DESCRIPTION].val, MAXRESDESLEN);

            if (allInfo.resTable[allInfo.nRes].interval > 0
                && (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC)) {

                if (allInfo.numUsrIndx+NBUILTINDEX >=li_len-1) {
                    li_len *= 2;
                    if (!(li=(struct liStruct *)
                          realloc(li, li_len*sizeof(struct liStruct)))) {
                        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "malloc",
                                  li_len*sizeof(struct liStruct));
                        return false;
                    }
                }
                if ((li[NBUILTINDEX + allInfo.numUsrIndx].name =
                     putstr_(allInfo.resTable[allInfo.nRes].name)) == NULL) {
                    ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "malloc",
                              sizeof(allInfo.resTable[allInfo.nRes].name));
                    return false;
                }

                li[NBUILTINDEX + allInfo.numUsrIndx].increasing =
                    (allInfo.resTable[allInfo.nRes].orderType == INCR)?1:0;
                allInfo.numUsrIndx++;
                allInfo.numIndx++;
            }
            allInfo.nRes++;
            nres++;
            freeKeyList (keyList);
        }
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5264,
                                         "%s: %s(%d): horizontal resource section not implemented yet"), fname, lsfile, *LineNum); /* catgets 5264 */
        return false;
    }


    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
              fname, lsfile, *LineNum, "resource");
    return false;

}

static void
chkUIdxAndSetDfltRunElim(void)
{
    if (defaultRunElim == false && allInfo.numUsrIndx > 0) {
        int i;

        for (i = NBUILTINDEX; i < allInfo.numIndx; i++) {
            if (allInfo.resTable[i].flags & (RESF_DYNAMIC | RESF_GLOBAL)) {
                if( allInfo.resTable[i].flags & RESF_DEFINED_IN_RESOURCEMAP ) {
                    defaultRunElim = true;
                    break;
                }
            }
        }
    }
}

static void
setupHostNodeResBitArrays(void)
{
    struct sharedResourceInstance *tmp;
    int i, j, bitPos, newMax;
    struct hostNode *hostPtr;

    for (tmp=sharedResourceHead, bitPos=0; tmp; tmp=tmp->nextPtr, bitPos++);

    newMax = GET_INTNUM(bitPos);

    for ( tmp=sharedResourceHead; tmp; tmp=tmp->nextPtr ){
        for ( i=0; i < tmp->nHosts; i++ ){
            hostPtr = tmp->hosts[i];
            if (!(hostPtr->resBitArray)){
                hostPtr->resBitArray = (int *)malloc((newMax+1)*sizeof(int));
                for (j=0; j < newMax+1; j++)
                    hostPtr->resBitArray[j] = 0;
            }
        }
    }

}

static int
doresourcemap(FILE *fp, char *lsfile, int *LineNum)
{
    static char fname[] = "doresourcemap()";
    int isDefault;
    char *linep;
    int resNo = 0;
    struct keymap keyList[] = {
#define RKEY_RESOURCE_NAME  0
        {"RESOURCENAME", NULL, 0},
#define RKEY_LOCATION    1
        {"LOCATION", NULL, 0},
        {NULL, NULL, 0}
    };

    linep = getNextLineC_(fp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                  fname, lsfile, *LineNum, "resourceMap");
        return -1;
    }


    if (isSectionEnd(linep, lsfile, LineNum, "resourceMap")) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5477,
                                             "%s: %s(%d): Empty resourceMap, no keywords or resources defined."), /* catgets 5477 */
                  fname, lsfile, *LineNum);
        return -1;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5267,
                                             "%s: %s(%d): keyword line format error for section resource, ignoring section"), fname, lsfile, *LineNum); /* catgets 5267 */
            doSkipSection(fp, LineNum, lsfile, "resourceMap");
            return -1;
        }


        while ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, "resourceMap")) {
                return 0;
            }

            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5268,
                                                 "%s: %s(%d): values do not match keys for resourceMap section, ignoring line"), fname, lsfile, *LineNum); /* catgets 5268 */
                lim_CheckError = WARNING_ERR;
                continue;
            }

            if ((resNo =resNameDefined(keyList[RKEY_RESOURCE_NAME].val)) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5269,
                                                 "%s: %s(%d): Resource name <%s> is  not defined; ignoring line"), /* catgets 5269 */
                          fname, lsfile, *LineNum,
                          keyList[RKEY_RESOURCE_NAME].val);
                lim_CheckError = WARNING_ERR;
                freeKeyList (keyList);
                continue;
            }
            else {

                if (resNo < NBUILTINDEX)
                {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5296,
                                                     "%s: %s(%d): Built-in resource %s can't be redefined as shared resource here. Ignoring line"), /* catgets 5296 */
                              fname, lsfile, *LineNum, keyList[0].val);
                    continue;
                }
            }

            if (keyList[RKEY_LOCATION].val != NULL
                && strcmp(keyList[RKEY_LOCATION].val, LSF_LIM_ERES_TYPE) == 0)
            {
                if (setExtResourcesLoc(keyList[RKEY_RESOURCENAME].val, resNo) != 0)
                {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5270,
                                                     "%s: Ignoring the external resource location <%s>(%d) in section resourceMap of file %s"),  /* catgets 5270 */
                              fname, keyList[RKEY_RESOURCENAME].val,
                              *LineNum, lsfile);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }
                freeKeyList (keyList);
                continue;
            }

            if (keyList[RKEY_LOCATION].val != NULL
                && keyList[RKEY_LOCATION].val[0] != '\0') {

                if (strstr(keyList[RKEY_LOCATION].val, "all ") &&
                    strchr(keyList[RKEY_LOCATION].val, '~')) {

                    struct HostsArray array;
                    struct hostNode* hPtr;
                    int result;

                    array.size = 0;
                    array.hosts = malloc(numofhosts*sizeof(char*));
                    if (!array.hosts) {
                        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, "doresourcemap",  "malloc");
                        return -1;
                    }
                    for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
                        array.hosts[array.size] = strdup(hPtr->hostName);
                        if (!array.hosts[array.size]) {
                            freeSA_(array.hosts, array.size);
                            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, "doresourcemap",  "malloc");
                            return -1;
                        }
                        array.size++;
                    }

                    result = convertNegNotation_(&(keyList[RKEY_LOCATION].val), &array);
                    if (result == 0) {
                        ls_syslog(LOG_WARNING,
                                  I18N(5397,"%s: %s(%d): convertNegNotation_: all the hosts are to be excluded %s !"), /* catgets 5397 */
                                  fname, lsfile, *LineNum, keyList[RKEY_LOCATION].val);
                    } else if (result < 0) {
                        ls_syslog(LOG_WARNING, I18N(5398, "%s: %s(%d): convertNegNotation_: Wrong syntax \'%s\'"), /* catgets 5398 */
                                  fname, lsfile, *LineNum, keyList[RKEY_LOCATION].val);
                    }
                    freeSA_(array.hosts, array.size);
                }

                if (addResourceMap (keyList[RKEY_RESOURCE_NAME].val,
                                    keyList[RKEY_LOCATION].val, lsfile,
                                    *LineNum, &isDefault) < 0) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5271,
                                                     "%s: %s(%d): addResourceMap() failed for resource <%s>; ignoring line"), /* catgets 5271 */
                              fname, lsfile, *LineNum,
                              keyList[RKEY_RESOURCE_NAME].val);
                    lim_CheckError = WARNING_ERR;
                    freeKeyList (keyList);
                    continue;
                }

                if( !(allInfo.resTable[resNo].flags & RESF_BUILTIN) ) {
                    allInfo.resTable[resNo].flags
                        |= RESF_DEFINED_IN_RESOURCEMAP;
                }

                if (!(isDefault &&
                      (allInfo.resTable[resNo].flags & RESF_DYNAMIC) &&
                      (allInfo.resTable[resNo].valueType == LS_NUMERIC))) {

                    allInfo.resTable[resNo].flags &= ~RESF_GLOBAL;
                    allInfo.resTable[resNo].flags |= RESF_SHARED;
                }

                resNo = 0;
            } else {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5272,
                                                 "%s: %s(%d): No LOCATION specified for resource <%s>; ignoring the line"), /* catgets 5272 */
                          fname, lsfile, *LineNum,
                          keyList[RKEY_RESOURCE_NAME].val);
                lim_CheckError = WARNING_ERR;
                freeKeyList (keyList);
                continue;
            }
            freeKeyList (keyList);
        }
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5273,
                                         "%s: %s(%d): horizontal resource section not implemented yet"), fname, lsfile, *LineNum); /* catgets 5273 */
        return -1;
    }


    return 0;

}


static int
addSharedResourceInstance(int nHosts, char **hosts, char *resName)
{
    char fname[] = "addSharedResourceInstance" ;
    struct sharedResourceInstance *tmp;
    struct hostNode *hPtr ;
    int i, cnt;
    static int firstFlag = 1;
    int resNo;

    if ((resNo = resNameDefined(resName)) < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5274,
                                         "%s: Resource name <%s> not defined"), /* catgets 5274 */
                  fname, resName);
        return -1;
    }



    if (!(allInfo.resTable[resNo].flags & RESF_DYNAMIC)) {
        return 0;
    }

    tmp=
        (sharedResourceInstance *)malloc(sizeof(struct sharedResourceInstance));
    if (tmp == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL, fname, "malloc",
                  sizeof(struct sharedResourceInstance));
        return -1;
    } else {
        tmp->nextPtr = NULL;
        tmp->resName = putstr_(resName) ;
        tmp->hosts = (struct hostNode **)malloc(nHosts*sizeof(struct hostNode *));
        if (tmp->hosts == NULL){
            ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL, fname, "malloc",
                      nHosts*sizeof(struct hostNode) );
            return -1;
        } else {
            cnt = 0;
            for ( i=0; i<nHosts; i++ ){
                if ((hPtr=findHostbyList(myClusterPtr->hostList, hosts[i]))
                    != NULL)
                    tmp->hosts[cnt++] = hPtr ;
            }
            tmp->nHosts = cnt ;
        }
        if (firstFlag) {
            firstFlag = 0;
            sharedResourceHead = tmp ;
        } else {
            tmp->nextPtr = sharedResourceHead ;
            sharedResourceHead = tmp ;
        }
    }
    if (logclass & LC_ELIM){
        char str[SBUF_SIZ];
        cnt = 0;
        for (tmp = sharedResourceHead; tmp ; tmp = tmp->nextPtr) {
            sprintf(str, "%d %s: ", cnt++, tmp->resName);
            for (i = 0; i < tmp->nHosts;i++)
                sprintf(str + strlen(str), "%s ", tmp->hosts[i]->hostName);
            ls_syslog(LOG_DEBUG,"%s", str);
        }
    }
    return 1;
}

static int
addResourceMap (char *resName, char *location, char *lsfile, int LineNum,
                int *isDefault)
{
    static char fname[] = "addResourceMap";
    struct sharedResource *resource;
    int i, j, numHosts = 0, first = true, error;
    char **hosts = NULL, *sp, *cp, ssp, *instance;
    char *initValue;
    int  defaultWord = false, numCycle;
    struct hostNode *hPtr;
    int resNo, dynamic;

    *isDefault = false;

    if (resName == NULL || location == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5382,
                                         "%s: %s(%d): Resource name <%s> location <%s>"), /* catgets 5382 */
                  fname, lsfile, LineNum,
                  (resName?resName:"NULL"), (location?location:"NULL"));
        return -1;
    }

    if ((resNo = resNameDefined(resName)) < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5275,
                                         "%s: %s(%d): Resource name <%s> not defined"), /* catgets 5275 */
                  fname, lsfile, LineNum, resName);
        return -1;
    }

    dynamic = (allInfo.resTable[resNo].flags & RESF_DYNAMIC);

    resource = inHostResourcs (resName);
    sp = location;


    i = 0;
    while (*sp != '\0') {
        if (*sp == '[')
            i++;
        else if (*sp == ']')
            i--;
        sp++;
    }
    if (i != 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5383,
                                         "%s: %s(%d): number of '[' is not match that of ']' in <%s> for resource <%s>; ignoring"), /* catgets 5383 */
                  fname, lsfile, LineNum, location, resName);
        return -1;
    }

    if ((initValue = (char *)malloc(4*sizeof(char))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
        return -1;
    }
    sp = location;

    while (sp != NULL && sp[0] != '\0') {
        for (j = 0; j < numHosts; j++)
            FREEUP (hosts[j]);
        FREEUP (hosts);
        numHosts = 0;
        error = false;
        instance = sp;
        initValue[0] = '\0';
        defaultWord = false;
        while (*sp == ' ' && *sp != '\0')
            sp++;
        if (*sp == '\0') {
            FREEUP(initValue);
            if (first == true)
                return -1;
            else
                return 0;
        }
        cp = sp;
        if ( *cp != '[' && *cp != '\0') {
            while (*cp && *cp != '@'
                   && !(!iscntrl(*cp) && isspace(*cp)))
                cp++;
        }

        if (cp != sp) {
            int lsize;
            ssp = cp[0];
            cp[0] = '\0';
            lsize = (strlen(sp)+1)*sizeof(char);
            if ((initValue
                 = (char *)realloc((void *)initValue, lsize)) == NULL){
                ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "realloc",
                          lsize);
            }
            strcpy (initValue, sp);
            if (!isdigitstr_(initValue) && allInfo.resTable[resNo].valueType == LS_NUMERIC) {

                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5386,
                                                 "%s: %s(%d): Invalid characters (%s) used as NUMERIC resource value; ignoring"), /* catgets 5386 */
                          fname, lsfile, LineNum, initValue);
                FREEUP(initValue);
                return -1;
            }
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
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5384,
                                             "%s: %s(%d): Bad character <%c> in instance; ignoring"), /* catgets 5384 */
                      fname, lsfile, LineNum, *sp);
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
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5385,
                                                 "%s: %s(%d): Bad format for instance <%s>; ignoring the instance"), /* catgets 5385 */
                          fname, lsfile, LineNum, instance);
                FREEUP(initValue);
                return -1;
            }
            if (error == true) {
                sp++;
                ssp =  *sp;
                *sp = '\0';
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5385,
                                                 "%s: %s(%d): Bad format for instance <%s>; ignoringthe instance"),
                          fname, lsfile, LineNum, instance);
                *sp = ssp;
                continue;
            }
            *sp = '\0';
            sp++;

            if (initValue[0] == '\0' && !dynamic) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5276,
                                                 "%s: %s(%d): Value must be defined for static resource; ignoring resource <%s>, instance <%s>"), /* catgets 5276 */
                          fname, lsfile, LineNum, resName, instance);
                continue;
            }

            if (initValue[0] != '\0' && dynamic) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5277,
                                                 "%s: %s(%d): Value <%s> ignored for dynamic resource <%s>, instance <%s>"), /* catgets 5277 */
                          fname, lsfile, LineNum, initValue, resName,
                          instance);
                initValue[0] = '\0';
            }

            if ((numHosts = parseHostList (cp, lsfile, LineNum, &hosts, &defaultWord)) <= 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5387,
                                                 "%s: %s(%d): getHostList(%s) failed; ignoring the instance <%s>"), /* catgets 5387 */
                          fname, lsfile, LineNum, cp, instance);
                lim_CheckError = WARNING_ERR;
                continue;
            }
            if (defaultWord == true) {
                *isDefault = true;

                if (numHosts > 1)
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5388,
                                                     "%s: %s(%d):  Other host is specified with reserved word <default> in the instance <%s> for resource <%s>;ignoring other hosts"), /* catgets 5388 */
                              fname, lsfile, LineNum, instance, resName);

                if (resource && resource->numInstances > 1)  {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5389,
                                                     "%s: %s(%d):  Other instances are specified with the instance <%s> for resource <%s>; ignoring the instance"), /* catgets 5389 */
                              fname, lsfile, LineNum, instance, resName);
                    break;
                }
            }
            if (defaultWord == true) {
                numCycle = numofhosts;
                FREEUP(hosts[0]);
            }
            else
                numCycle = 1;

            for (j = 0; j < numCycle; j++) {
                if (defaultWord == true) {

                    if (dynamic)
                        defaultRunElim = true;

                    if (j == 0)
                        hPtr = myClusterPtr->hostList;
                    else
                        hPtr = hPtr->nextPtr;
                    if (hPtr == NULL)
                        break;
                    FREEUP(hosts[0]);
                    hosts[0] = putstr_(hPtr->hostName);
                    numHosts = 1;
                }
                if (resource == NULL) {

                    if (!(defaultWord && dynamic &&
                          allInfo.resTable[resNo].valueType == LS_NUMERIC) &&
                        (resource = addResource (resName, numHosts,
                                                 hosts, initValue, lsfile,
                                                 LineNum, true)) == NULL)
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5390,
                                                         "%s: %s(%d): addResource() failed; ignoring the instance <%s>"), /* catgets 5390 */
                                  fname, lsfile, LineNum, instance);
                } else {
                    if (addHostInstance (resource, numHosts, hosts,
                                         initValue, true) < 0)
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5391,
                                                         "%s: %s(%d): addHostInstance() failed; ignoring the instance <%s>"), /* catgets 5391 */
                                  fname, lsfile, LineNum, instance);
                }
            }
            defaultWord = false;
            continue;
        } else {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5392,
                                             "%s: %s(%d): No <[>  for instance in <%s>; ignoring"), fname, lsfile, LineNum, location);  /* catgets 5392 */
            while (*sp != ']' && *sp != '\0')
                sp++;
            if (*sp == '\0')
            {
                FREEUP(initValue);
                return -1;
            }
            sp++;
        }
    }
    for (j = 0; j < numHosts; j++)
        FREEUP (hosts[j]);
    FREEUP (hosts);
    FREEUP(initValue);
    return 0;

}


static int
parseHostList (char *hostList, char *lsfile, int LineNum, char ***hosts, int *hasDefault)
{
    static char fname[] = "parseHostList";
    char *host, *sp, **hostTable, *hostName;
    int numHosts = 0, i;

    if (hostList == NULL)
        return -1;

    sp = hostList;
    while ((host = getNextWord_(&sp)) != NULL)
        numHosts++;
    if ((hostTable = (char **) calloc (numHosts, sizeof(char *))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
        return -1;
    }
    sp = hostList;
    numHosts = 0;
    while ((host = getNextWord_(&sp)) != NULL) {

        if ((hostName = validLocationHost (host)) == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5278,
                                             "%s: %s(%d): Invalid hostname <%s>;ignoring the host"), fname, lsfile, LineNum, host); /* catgets 5278 */
            lim_CheckError = WARNING_ERR;
            continue;
        }
        if ((hostTable[numHosts] = putstr_(hostName)) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
            for (i = 0; i < numHosts; i++)
                FREEUP (hostTable[i]);
            FREEUP (hostTable);
            return -1;
        }
        if (!strcmp (hostName, "default"))
            *hasDefault = true;
        numHosts++;
    }
    if (numHosts == 0) {
        FREEUP (hostTable);
        return -1;
    }
    *hosts = hostTable;
    return numHosts;

}

static char *
validLocationHost (char *hostName)
{
    static char fname[] = "validLocationHost()";
    const char *officialName;
    int num;

    if (!strcmp (hostName, "default") || !strcmp (hostName, "others")
        || !strcmp (hostName, "all"))

        return hostName;

    if ((officialName = getHostOfficialByName_(hostName)) != NULL) {
        if (findHostbyList(myClusterPtr->hostList, hostName) != NULL)
            return (char*) officialName;
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5395,
                                         "%s: Host <%s> is not used by cluster <%s>;ignoring"), /* catgets 5395 */
                  fname, hostName, myClusterName);
        return NULL;
    }

    if ((num = typeNameToNo(hostName)) > 0)

        return hostName;

    if ((num = modelNameToNo (hostName)) > 0)

        return hostName;

    return NULL;

}

static void
initResItem(struct resItem *resTable)
{
    if (resTable == NULL)
        return;

    resTable->name[0] = '\0';
    resTable->des[0] = '\0';
    resTable->valueType = -1;
    resTable->orderType = NA;
    resTable->flags = RESF_GLOBAL;
    resTable->interval = 0;

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

    return -1;
}

int
readCluster(int checkMode)
{
    static char fname[] = "readCluster()";
    char *hname;
    int i;

    if (!myClusterPtr) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5279,
                                         "My cluster name %s is not configured in lsf.shared"), /* catgets 5279 */
                  myClusterName);
        lim_Exit("readCluster");
    }

    if (readCluster2(myClusterPtr) < 0)
        lim_Exit("readCluster");


    myClusterPtr->loadIndxNames = (char **)calloc(allInfo.numIndx,
                                                  sizeof(char *));
    if (myClusterPtr->loadIndxNames == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "calloc");
        lim_Exit("calloc");
    }

    for (i = 0; i < allInfo.numIndx; i++)
        myClusterPtr->loadIndxNames[i] = putstr_(li[i].name);

    setupHostNodeResBitArrays();

    if ((hname = ls_getmyhostname()) == NULL)
        lim_Exit("readCluster/ls_getmyhostname");

    myHostPtr = findHostbyList(myClusterPtr->hostList, hname);
    if (!myHostPtr) {
        myHostPtr = findHostbyList(myClusterPtr->clientList, hname);
        if (!myHostPtr) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5281,
                                             "%s: Local host %s not configured in Host section of file lsf.cluster.%s"), fname, hname, myClusterName); /* catgets 5281 */
            if (checkMode)
                return -1;
            else
                lim_Exit("readCluster");
        } else {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5282,
                                             "%s: Local host %s is configured as client-only in file lsf.cluster.%s; LIM will not run on a client-only host"),  /* catgets 5282 */
                      fname, hname, myClusterName);
            if (!checkMode)
                lim_Exit("readCluster");
        }
    }

    for (i=1; i<8; i++)
        if (myHostPtr->week[i] != NULL)
            break;

    if (i==8)   {
        for (i=1;i<8;i++)
            insertW(&(myHostPtr->week[i]), -1.0, 25.0);
    }

    for (i = 0; i < GET_INTNUM(allInfo.numIndx) + 1; i++)
        myHostPtr->status[i] = 0;
    checkHostWd();

    if (nClusAdmins == 0) {
        struct passwd *pwd;
        char rootName[MAXLSFNAMELEN];

        ls_syslog(LOG_ERR, "%s: No LavaLite managers specified in file"
                  "lsf.cluster.%s, default cluster manager is root.",
                  __func__, myClusterName);

        clusAdminIds = calloc(1, sizeof(int));
        clusAdminIds[0] = 0;
        nClusAdmins = 1;
        clusAdminNames = calloc(1, sizeof (char *));
        clusAdminNames[0] = strdup("root");
    }

    myClusterPtr->status = CLUST_STAT_OK | CLUST_ACTIVE | CLUST_INFO_AVAIL;
    myClusterPtr->managerName = clusAdminNames[0];
    myClusterPtr->managerId = clusAdminIds[0];

    myClusterPtr->nAdmins = nClusAdmins;
    myClusterPtr->adminIds = clusAdminIds;
    myClusterPtr->admins   = clusAdminNames;

    return 0;

}

static int
readCluster2(struct clusterNode *clPtr)
{
    static char fname[] = "readCluster2()";
    char   fileName[MAXFILENAMELEN];
    char   *word;
    FILE   *clfp;
    char   *cp;
    int LineNum = 0;
    int Error = false;
    int aorm = false;

#define ADMIN_IGNORE(word)                                              \
    {                                                                   \
        if (aorm == true) {                                             \
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5283,   \
                                             "%s: section <clustermanager> and <clusteradmins> cannot be configured in the same time, ignoring the second section <%s>"), fname , word); /* catgets 5283 */ \
            continue;                                                   \
        }                                                               \
    }

    sprintf(fileName, "%s/lsf.cluster.%s",
            limParams[LSF_CONFDIR].paramValue, clPtr->clName);


    if (configCheckSum(fileName, &clPtr->checkSum) < 0) {
        return -1;
    }
    if ((clfp = confOpen(fileName, "r")) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M,
                  fname, "confOpen", fileName);
        return -1;
    }

    for (;;) {
        cp = getBeginLine(clfp, &LineNum);
        if (!cp) {
            FCLOSEUP(&clfp);
            if (clPtr->hostList) {
                if (Error)
                    return -1;
                else {

                    adjIndx();

                    chkUIdxAndSetDfltRunElim();


                    return 0;
                }
            } else if (!(clPtr->hostList)) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5285,
                                                 "%s: %s(%d): No hosts configured for cluster %s"),/* catgets 5285 */
                          fname, fileName, LineNum, clPtr->clName);
                return -1;
            }
        }

        word = getNextWord_(&cp);
        if (!word) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5286,
                                             "%s: %s(%d): Keyword expected after Begin. Ignoring section"), /* catgets 5286 */
                      fname, fileName, LineNum);
            lim_CheckError = WARNING_ERR;
            doSkipSection(clfp, &LineNum, fileName, "unknown");
        } else if (strcasecmp(word, "clustermanager") == 0) {
            if (clPtr != myClusterPtr) {
                doSkipSection(clfp, &LineNum, fileName, "clustermanager");
                continue;
            }
            ADMIN_IGNORE(word);
            if (domanager(clfp, fileName, &LineNum, "clustermanager")<0 &&
                aorm != true) {
                Error = true;
            } else
                aorm = true;
            continue;
        } else if (strcasecmp(word, "clusteradmins") == 0) {
            if (clPtr != myClusterPtr) {
                doSkipSection(clfp, &LineNum, fileName, "clusteradmins");
                continue;
            }
            ADMIN_IGNORE(word);
            if (domanager(clfp, fileName, &LineNum, "clusteradmins")<0 &&
                aorm != true) {
                Error = true;
            } else
                aorm = true;
            continue;
        } else if (strcasecmp(word, "parameters") == 0) {
            if (doclparams(clfp, fileName, &LineNum) < 0)
                lim_CheckError= WARNING_ERR;
            continue;
        } else if (strcasecmp(word, "host") == 0) {
            if (dohosts(clfp, clPtr, fileName, &LineNum) < 0)
                Error = true;


            if (adjHostListOrder() < 0 ) {
                return -1;
            }

            continue;
        } else if (strcasecmp(word, "resourceMap") == 0) {
            if (doresourcemap(clfp, fileName, &LineNum) < 0)
                Error = true;
            continue;
        } else {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5287,
                                             "%s %s(%d): Invalid section name %s, ignoring section"), /* catgets 5287 */
                      fname, fileName, LineNum, word);
            lim_CheckError = WARNING_ERR;
            doSkipSection(clfp, &LineNum, fileName, word);
        }
    }

}

static void
adjIndx (void)
{
    static char fname[] = "adjIndx()";
    int i, resNo, j, k;
    char **temp;
    struct resItem tmpTable;
    struct hostNode *hPtr;

    if (numHostResources <= 0)
        return;

    for (i = 0; i < numHostResources; i++) {
        if ((resNo = resNameDefined (hostResources[i]->resourceName)) < 0)
            continue;

        if ((allInfo.resTable[resNo].valueType != LS_NUMERIC)
            || !(allInfo.resTable[resNo].flags & RESF_SHARED))
            continue;


        memcpy((char *)&tmpTable, (char *)&allInfo.resTable[resNo],
               sizeof(struct resItem));
        for (j = resNo; j < allInfo.nRes - 1; j++)
            memcpy((char *)&allInfo.resTable[j], (char *)&allInfo.resTable[j+1],
                   sizeof(struct resItem));
        memcpy((char *)&allInfo.resTable[allInfo.nRes - 1],
               (char *)&tmpTable, sizeof(struct resItem));


        if ((temp = realloc (shortInfo.resName,
                             (shortInfo.nRes + 1)*sizeof (char *))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            lim_Exit(fname);
        }
        shortInfo.resName = temp;
        if ((shortInfo.resName[shortInfo.nRes]
             = putstr_(tmpTable.name)) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            lim_Exit(fname);
        }
        SET_BIT (shortInfo.nRes, shortInfo.numericResBitMaps);
        shortInfo.nRes++;

        if (tmpTable.flags & RESF_DYNAMIC) {



            for (k = NBUILTINDEX; k < allInfo.numIndx; k++) {
                if (strcasecmp(li[k].name, tmpTable.name) != 0)
                    continue;
                FREEUP (li[k].name);
                for (j = k; j < allInfo.numIndx - 1; j++) {
                    memcpy((char *)&li[j], (char *)&li[j+1],
                           sizeof(struct liStruct));
                }
                break;
            }



            for (hPtr = myClusterPtr->hostList;
                 hPtr != NULL; hPtr = hPtr->nextPtr) {
                for (j = resNo; j < allInfo.numIndx -1; j++)
                    hPtr->busyThreshold[j] = hPtr->busyThreshold[j+1];
            }
            allInfo.numUsrIndx--;
            allInfo.numIndx--;
        }
    }

}

static int
domanager (FILE *clfp, char *lsfile, int *LineNum, char *secName)
{
    static char fname[] = "domanager()";
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

    if (lim_debug > 0 && lim_debug < 3) {
        char lsfUserName[MAXLSFNAMELEN];

        nClusAdmins = 1;
        clusAdminIds = (int *) malloc (sizeof (int));
        clusAdminGids = (int *) malloc (sizeof (int));
        if (getpwnam(lsfUserName) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "getLSFUser_");
            return -1;
        }
        clusAdminIds[0] = getuid();
        clusAdminGids[0] = getgid();
        clusAdminNames = (char **) malloc (sizeof (char *));
        clusAdminNames[0] = putstr_(lsfUserName);
        doSkipSection(clfp, LineNum, lsfile, secName);
        if (lim_CheckMode > 0)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5289,
                                             "%s: %s(%d): The cluster manager is the invoker <%s> in debug mode"), fname, lsfile, *LineNum,
                      lsfUserName); /* catgets 5289 */
        return 0;
    }

    linep = getNextLineC_(clfp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                  fname, lsfile, *LineNum, secName);
        return -1;
    }

    if (isSectionEnd(linep, lsfile, LineNum, secName))
        return 0;

    if (strcmp (secName, "clustermanager") == 0)
        keyList = keyList1;
    else
        keyList = keyList2;

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, true)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5291,
                                             "%s: %s(%d): keyword line format error for section %s, ignoring section"), fname, lsfile, *LineNum, secName); /* catgets 5291 */
            doSkipSection(clfp, LineNum, lsfile, secName);
            return -1;
        }

        if ((linep = getNextLineC_(clfp, LineNum, true)) != NULL) {
            if (isSectionEnd(linep, lsfile, LineNum, secName))
                return 0;
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5292,
                                                 "%s: %s(%d): values do not match keys for section %s, ignoring section"), fname, lsfile, *LineNum, secName); /* catgets 5292 */
                doSkipSection(clfp, LineNum, lsfile, secName);
                return -1;
            }
            if (getClusAdmins (keyList[0].val, lsfile, LineNum, secName) < 0) {
                FREEUP(keyList[0].val);
                return -1;
            } else {
                FREEUP(keyList[0].val);
                return 0;
            }
        }
    } else {
        if (readHvalues(keyList, linep, clfp, lsfile, LineNum, true, secName) <0)
            return -1;
        if (getClusAdmins (keyList[0].val, lsfile, LineNum, secName) < 0) {
            FREEUP(keyList[0].val);
            return -1;
        } else {
            FREEUP(keyList[0].val);
            return 0;
        }
    }
    return 0;

}

static int
getClusAdmins (char *line, char *lsfile, int *LineNum, char *secName)
{
    static char fname[] = "getClusAdmins()";
    struct admins *admins;
    static char lastSecName[40];
    static int count = 0;

    if (count > 1)  {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5293,
                                         "%s: %s(%d): More than one %s section defined; ignored."), fname, lsfile, *LineNum, secName); /* catgets 5293 */
        return -1;
    }
    count++;
    if (strcmp (lastSecName, secName) == 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5294,
                                         "%s: %s(%d): section <%s> is multiply specified; ignoring the section"), fname, lsfile, *LineNum, secName); /* catgets 5294 */
        return -1;
    }
    lserrno = LSE_NO_ERR;
    admins = getAdmins (line, lsfile, LineNum, secName);
    if (admins->nAdmins <= 0) {

        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5295,
                                         "%s: %s(%d): No valid user for section %s: %s"), fname, lsfile, *LineNum, secName, line); /* catgets 5295 */
        return -1;
    }
    if (strcmp (secName, "clustermanager") == 0 &&
        strcmp (lastSecName, "clusteradmins") == 0) {
        if (setAdmins (admins, A_THEN_M) < 0)
            return -1;
    } else if (strcmp (lastSecName, "clustermanager") == 0 &&
               strcmp (secName, "clusteradmins") == 0) {
        if (setAdmins (admins, M_THEN_A) < 0)
            return -1;
    } else {
        if (setAdmins (admins, M_OR_A) < 0)
            return -1;
    }
    strcpy (lastSecName, secName);
    return 0;

}

static int
setAdmins (struct admins *admins, int mOrA)
{
    static char fname[] = "setAdmins()";
    int i, workNAdmins;
    int tempNAdmins, *tempAdminIds, *tempAdminGids, *workAdminIds, *workAdminGids;
    char **tempAdminNames, **workAdminNames;

    tempNAdmins = admins->nAdmins + nClusAdmins;
    tempAdminIds = (int *) malloc (tempNAdmins *sizeof (int));
    tempAdminGids = (int *) malloc (tempNAdmins * sizeof (int));
    tempAdminNames = (char **) malloc (tempNAdmins * sizeof (char *));
    if (!tempAdminIds || !tempAdminGids || !tempAdminNames) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        FREEUP (tempAdminIds);
        FREEUP (tempAdminGids);
        FREEUP (tempAdminNames);
        return -1;
    }
    if (mOrA == M_THEN_A) {
        workNAdmins = nClusAdmins;
        workAdminIds = clusAdminIds;
        workAdminGids = clusAdminGids;
        workAdminNames = clusAdminNames;
    } else {
        workNAdmins = admins->nAdmins;
        workAdminIds = admins->adminIds;
        workAdminGids = admins->adminGIds;
        workAdminNames = admins->adminNames;
    }
    for (i = 0; i < workNAdmins; i++) {
        tempAdminIds[i] = workAdminIds[i];
        tempAdminGids[i] = workAdminGids[i];
        tempAdminNames[i] = putstr_(workAdminNames[i]);
    }
    tempNAdmins = workNAdmins;
    if (mOrA == M_THEN_A) {
        workNAdmins = admins->nAdmins;
        workAdminIds = admins->adminIds;
        workAdminGids = admins->adminGIds;
        workAdminNames = admins->adminNames;
    } else if (mOrA == A_THEN_M) {
        workNAdmins = nClusAdmins;
        workAdminIds = clusAdminIds;
        workAdminGids = clusAdminGids;
        workAdminNames = clusAdminNames;
    } else
        workNAdmins = 0;
    for (i = 0; i < workNAdmins; i++) {
        if (isInlist (tempAdminNames, workAdminNames[i], tempNAdmins))
            continue;
        tempAdminIds[tempNAdmins] = workAdminIds[i];
        tempAdminGids[tempNAdmins] = workAdminGids[i];
        tempAdminNames[tempNAdmins] = putstr_ (workAdminNames[i]);
        tempNAdmins++;
    }
    if (nClusAdmins > 0) {
        for (i = 0; i < nClusAdmins; i++)
            FREEUP (clusAdminNames[i]);
        FREEUP (clusAdminIds);
        FREEUP (clusAdminGids);
        FREEUP (clusAdminNames);
    }
    nClusAdmins = tempNAdmins;
    clusAdminIds = tempAdminIds;
    clusAdminGids = tempAdminGids;
    clusAdminNames = tempAdminNames;


    return 0;
}
static int
doclparams (FILE *clfp, char *lsfile, int *LineNum)
{
    static char fname[] = "doclparams()";
    char *linep;
    int warning = false;
    struct keymap keyList[] = {
#define EXINTERVAL      0
        {"EXINTERVAL", NULL, 0},
#define ELIMARGS        1
        {"ELIMARGS", NULL, 0},
#define PROBE_TIMEOUT       2
        {"PROBE_TIMEOUT", NULL, 0},
#define ELIM_POLL_INTERVAL  3
        {"ELIM_POLL_INTERVAL", NULL, 0},
#define HOST_INACTIVITY_LIMIT   4
        {"HOST_INACTIVITY_LIMIT", NULL, 0},
#define MASTER_INACTIVITY_LIMIT 5
        {"MASTER_INACTIVITY_LIMIT", NULL, 0},
#define RETRY_LIMIT     6
        {"RETRY_LIMIT", NULL, 0},
#define ADJUST_DURATION     7
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
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                  fname, lsfile, *LineNum, "parameters");
        return -1;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "parameters"))
        return 0;

    if (strchr(linep, '=') == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5298,
                                         "%s: %s(%d): vertical section not supported, ignoring section"), fname, lsfile, *LineNum); /* catgets 5298 */
        doSkipSection(clfp, LineNum, lsfile, "parameters");
        return -1;
    } else {
        if (readHvalues(keyList, linep, clfp, lsfile, LineNum, false,
                        "parameters") <0)
            return -1;
        if (keyList[EXINTERVAL].val) {
            if (!isanumber_(keyList[0].val) || atof(keyList[0].val) < 0.001) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5299,
                                                 "%s: %s(%d): Invalid exchange interval in section parameters: %s. Ignoring."), /* catgets 5299 */
                          fname, lsfile, *LineNum, keyList[EXINTERVAL].val);
                FREEUP(keyList[EXINTERVAL].val);
                warning = true;
            } else
                exchIntvl = atof(keyList[EXINTERVAL].val);
            FREEUP(keyList[EXINTERVAL].val);

            if (exchIntvl < 15)
                resInactivityLimit = 180.0/exchIntvl;
        }

        if (keyList[ELIMARGS].val) {
            myClusterPtr->eLimArgs = putstr_(keyList[1].val);

            if (!myClusterPtr->eLimArgs) {
                ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "malloc",
                          keyList[ELIMARGS].val);
                return -1;
            }
            FREEUP(keyList[ELIMARGS].val);
        }

        if (keyList[PROBE_TIMEOUT].val) {
            if (!isint_(keyList[PROBE_TIMEOUT].val) || atoi(keyList[PROBE_TIMEOUT].val) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5301,
                                                 "%s: %s(%d): Invalid probe timeout value in section parameters: %s. Ignoring."), /* catgets 5301 */
                          fname, lsfile, *LineNum, keyList[PROBE_TIMEOUT].val);
                warning = true;
                FREEUP(keyList[PROBE_TIMEOUT].val);
            } else
                probeTimeout = atoi(keyList[PROBE_TIMEOUT].val);

            FREEUP(keyList[PROBE_TIMEOUT].val);
        }

        if (keyList[ELIM_POLL_INTERVAL].val) {
            if (!isanumber_(keyList[ELIM_POLL_INTERVAL].val) ||
                atof(keyList[ELIM_POLL_INTERVAL].val) < 0.001 ||
                atof(keyList[ELIM_POLL_INTERVAL].val) > 5) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5302,
                                                 "%s: %s(%d): Invalid sample interval in section parameters: %s. Must be between 0.001 and 5. Ignoring."), /* catgets 5302 */
                          fname, lsfile, *LineNum, keyList[ELIM_POLL_INTERVAL].val);
                warning = true;
                FREEUP(keyList[ELIM_POLL_INTERVAL].val);
            } else
                sampleIntvl = atof(keyList[ELIM_POLL_INTERVAL].val);
            FREEUP(keyList[ELIM_POLL_INTERVAL].val);
        }

        if (keyList[HOST_INACTIVITY_LIMIT].val) {
            if (!isint_(keyList[HOST_INACTIVITY_LIMIT].val) ||
                atoi(keyList[HOST_INACTIVITY_LIMIT].val) < 2) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5303,
                                                 "%s: %s(%d): Invalid host inactivity limit in section parameters: %s. Ignoring."), fname, lsfile, *LineNum, keyList[HOST_INACTIVITY_LIMIT].val); /* catgets 5303 */
                FREEUP(keyList[HOST_INACTIVITY_LIMIT].val);
                warning = true;
            } else
                hostInactivityLimit = atoi(keyList[HOST_INACTIVITY_LIMIT].val);
            FREEUP(keyList[HOST_INACTIVITY_LIMIT].val);
        }

        if (keyList[MASTER_INACTIVITY_LIMIT].val) {
            if (!isint_(keyList[MASTER_INACTIVITY_LIMIT].val) ||
                atoi(keyList[MASTER_INACTIVITY_LIMIT].val) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5304,
                                                 "%s: %s(%d): Invalid master inactivity limit in section parameters: %s. Ignoring."), fname, lsfile, *LineNum, keyList[MASTER_INACTIVITY_LIMIT].val); /* catgets 5304 */
                FREEUP(keyList[MASTER_INACTIVITY_LIMIT].val);
                warning = true;
            } else
                masterInactivityLimit = atoi(keyList[MASTER_INACTIVITY_LIMIT].val);
            FREEUP(keyList[MASTER_INACTIVITY_LIMIT].val);
        }

        if (keyList[RETRY_LIMIT].val) {
            if (!isint_(keyList[RETRY_LIMIT].val) ||
                atoi(keyList[RETRY_LIMIT].val) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5305,
                                                 "%s: %s(%d): Invalid host inactivity limit in section parameters: %s. Ignoring."), fname, lsfile, *LineNum, keyList[RETRY_LIMIT].val); /* catgets 5305 */
                FREEUP(keyList[RETRY_LIMIT].val);
                warning = true;
            } else
                retryLimit = atoi(keyList[RETRY_LIMIT].val);
            FREEUP(keyList[RETRY_LIMIT].val);
        }

        if (keyList[ADJUST_DURATION].val) {
            if (!isint_(keyList[ADJUST_DURATION].val) ||
                atoi(keyList[ADJUST_DURATION].val) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5306,
                                                 "%s: %s(%d): Invalid load adjust duration in section parameters: %s. Ignoring."), /* catgets 5306 */
                          fname, lsfile, *LineNum, keyList[ADJUST_DURATION].val);
                FREEUP(keyList[ADJUST_DURATION].val);
                warning = true;
            } else
                keepTime= atoi(keyList[ADJUST_DURATION].val);
            FREEUP(keyList[ADJUST_DURATION].val);
        }


        if (keyList[LSF_ELIM_DEBUG].val) {


            if (strcasecmp(keyList[LSF_ELIM_DEBUG].val,"y") != 0 &&
                strcasecmp(keyList[LSF_ELIM_DEBUG].val,"n") != 0) {

                ls_syslog(LOG_WARNING, I18N(5316,
                                            "LSF_ELIM_DEBUG invalid: %s, not debuging ELIM."), /* catgets 5316 */
                          keyList[LSF_ELIM_DEBUG].val);
                warning = true;
            } else {
                if (strcasecmp(keyList[LSF_ELIM_DEBUG].val, "y") == 0) {
                    ELIMdebug = 1;
                }
            }

            FREEUP(keyList[LSF_ELIM_DEBUG].val);
        }


        if (keyList[LSF_ELIM_BLOCKTIME].val) {

            ELIMblocktime = atoi(keyList[LSF_ELIM_BLOCKTIME].val);


            if (!isint_(keyList[LSF_ELIM_BLOCKTIME].val) ||
                ELIMblocktime < 0) {

                ls_syslog(LOG_WARNING, I18N(5318,
                                            "LSF_ELIM_BLOCKTIME invalid: %s, blocking communication with ELIM."), /* catgets 5318 */
                          keyList[LSF_ELIM_BLOCKTIME].val);
                warning = true;
                ELIMblocktime = -1;
            }

            FREEUP(keyList[LSF_ELIM_BLOCKTIME].val);
        }


        if (ELIMdebug && ELIMblocktime == -1) {

            ls_syslog(LOG_WARNING, I18N(5340,
                                        "LSF_ELIM_DEBUG=y but LSF_ELIM_BLOCKTIME is not set/valid; LSF_ELIM_BLOCKTIME will be set to 2 seconds ")); /* catgets 5340 */

            warning = true;

            ELIMblocktime = 2;
        }


        if (keyList[LSF_ELIM_RESTARTS].val) {
            ELIMrestarts = atoi(keyList[LSF_ELIM_RESTARTS].val);


            if (!isint_(keyList[LSF_ELIM_RESTARTS].val) ||
                ELIMrestarts < 0) {

                ls_syslog(LOG_WARNING, I18N(5366,
                                            "LSF_ELIM_RESTARTS invalid: %s, unlimited ELIM restarts."), /* catgets 5366 */
                          keyList[LSF_ELIM_RESTARTS].val);
                warning = true;

                ELIMrestarts = -1;
            } else {
                ELIMrestarts += 1;
            }
            FREEUP(keyList[LSF_ELIM_RESTARTS].val);
        }


        if (exchIntvl < sampleIntvl) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5308,
                                             "%s: Exchange interval must be greater than or equal to sampling interval. Setting exchange and sample interval to %f."), /* catgets 5308 */
                      fname, sampleIntvl);
            exchIntvl = sampleIntvl;
            warning = true;
        }

        if (warning==true)
            return -1;

        return 0;
    }
}

static struct keymap *
initKeyList(void)
{
    int i;

    static struct keymap *hostKeyList = NULL;

#define  HOSTNAME_    allInfo.numIndx
#define  MODEL        allInfo.numIndx+1
#define  TYPE         allInfo.numIndx+2
#define  ND           allInfo.numIndx+3
#define  RESOURCES    allInfo.numIndx+4
#define  RUNWINDOW    allInfo.numIndx+5
#define  REXPRI_      allInfo.numIndx+6
#define  SERVER_      allInfo.numIndx+7
#define  R            allInfo.numIndx+8
#define  S            allInfo.numIndx+9

    if (hostKeyList == NULL)
        if (!(hostKeyList=(struct keymap *)
              malloc((allInfo.numIndx+11)*sizeof(struct keymap)))) {
            return NULL;
        }




    for (i=0; i < S+1; i++) {
        hostKeyList[i].key="";
        hostKeyList[i].val = NULL;
        hostKeyList[i].position = 0;
    }
    hostKeyList[HOSTNAME_].key="HOSTNAME";
    hostKeyList[MODEL].key="MODEL";
    hostKeyList[TYPE].key="TYPE";
    hostKeyList[ND].key="ND";
    hostKeyList[RESOURCES].key="RESOURCES";
    hostKeyList[RUNWINDOW].key="RUNWINDOW";
    hostKeyList[REXPRI_].key="REXPRI";
    hostKeyList[SERVER_].key="SERVER";
    hostKeyList[R].key="R";
    hostKeyList[S].key="S";
    hostKeyList[S+1].key=NULL;



    for (i=0; i < allInfo.numIndx; i++)
        hostKeyList[i].key  = allInfo.resTable[i].name;

    return hostKeyList;

}

void
setMyClusterName(void)
{
    static char fname[] = "setMyClusterName()";
    struct keymap *keyList;
    FILE   *fp;
    char clusterFile[MAXFILENAMELEN];
    char *cluster;
    int LineNum;
    char found = false;
    char *lp, *cp;
    char *hname;

    if ((hname = ls_getmyhostname()) == NULL)
        lim_Exit("setMyClusterName/ls_getmyhostname failed");

    {
        ls_syslog(LOG_DEBUG, "setMyClusterName: searching cluster files ...");
        cluster = myClusterPtr->clName;
        sprintf(clusterFile, "%s/lsf.cluster.%s",
                limParams[LSF_CONFDIR].paramValue, cluster);
        fp = confOpen(clusterFile, "r");

        if (!fp) {
            if (!found && !mcServersSet) {
                ls_syslog(LOG_ERR, I18N_CANNOT_OPEN, fname, clusterFile);
            }
            goto endfile;
        }

        LineNum = 0;

        for (;;) {
            if ((lp = getBeginLine(fp, &LineNum)) == NULL) {
                if (! found) {
                    ls_syslog(LOG_DEBUG, "setMyClusterName: Local host %s not defined in cluster file %s", hname, clusterFile);
                }
                break;
            }


            cp = getNextWord_(&lp);
            if (!cp) {
                if (! found) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5317,
                                                     "%s: %s(%d): Section name expected after Begin; section ignored."), fname, clusterFile, LineNum); /* catgets 5317 */
                    lim_CheckError = WARNING_ERR;
                }
                continue;
            } else {

                if (strcasecmp(cp, "host") != 0)
                    continue;
            }


            lp = getNextLineC_(fp, &LineNum, true);
            if (! lp) {
                if (! found) {
                    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                              fname, clusterFile, LineNum, "host");
                    lim_CheckError = WARNING_ERR;
                }
                break;
            }
            if (isSectionEnd(lp, clusterFile, &LineNum, "Host")) {

                continue;
            }
            if (strchr(lp, '=') != NULL) {
                if (! found) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5319,
                                                     "%s: %s(%d): horizontal host section not implemented yet, use vertical format: section ignored"),  /* catgets 5319 */
                              fname, clusterFile, LineNum);
                    lim_CheckError = WARNING_ERR;
                }
                continue;
            }

            keyList = initKeyList();

            if (! keyMatch(keyList, lp, false)) {
                if (! found) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5320,
                                                     "%s: %s(%d): keyword line format error for section Host, section ignored"), fname, clusterFile, LineNum); /* catgets 5320 */
                    lim_CheckError = WARNING_ERR;
                }
                continue;
            }
            if (keyList[HOSTNAME_].position == -1) {
                if (! found) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5321,
                                                     "%s: %s(%d): key HOSTNAME_ is missing in section host, section ignored"), fname, clusterFile, LineNum); /* catgets 5321 */
                    lim_CheckError = WARNING_ERR;
                }
                continue;
            }


            while ((lp = getNextLineC_(fp, &LineNum, true)) != NULL) {
                const char *officialName;
                if (isSectionEnd(lp, clusterFile, &LineNum, "host"))
                    break;
                if (mapValues(keyList, lp) < 0) {
                    if (! found) {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5322,
                                                         "%s: %s(%d): values do not match keys for section Host, record ignored"), fname, clusterFile, LineNum); /* catgets 5322 */
                        lim_CheckError = WARNING_ERR;
                    }
                    continue;
                }
                if ((officialName = getHostOfficialByName_(keyList[HOSTNAME_].val)) == NULL) {
                    if (! found) {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5323,
                                                         "%s: %s(%d): Invalid hostname %s in section host, host ignored"), /* catgets 5323 */
                                  fname, clusterFile, LineNum, keyList[HOSTNAME_].val);
                        lim_CheckError = WARNING_ERR;
                    }
                    freeKeyList(keyList);
                    continue;
                }

                if (strcasecmp(officialName, hname) == 0) {
                    if (! found) {
                        ls_syslog(LOG_DEBUG, "setMyClusterName: local host %s belongs to cluster %s", hname, cluster);
                        found = true;
                        strcpy(myClusterName, cluster);
                        freeKeyList(keyList);
                        break;
                    } else {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5324,
                                                         "%s: %s(%d): local host %s defined in more than one cluster file. Previous definition was in lsf.cluster.%s, ignoring current definition"), fname, clusterFile, LineNum, hname, myClusterName); /* catgets 5324 */
                        lim_CheckError = WARNING_ERR;
                        freeKeyList(keyList);
                        continue;
                    }
                }
                freeKeyList(keyList);
            }
        }
        FCLOSEUP(&fp);
    }

endfile:

    if (! found) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5325,
                                         "%s: unable to find the cluster file containing local host %s"), fname, hname); /* catgets 5325 */
        lim_Exit("setMyClusterName");
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
dohosts(FILE *clfp, struct clusterNode *clPtr, char *lsfile, int *LineNum)
{
    static char fname[] = "dohosts()";
    static struct hostEntry hostEntry;
    char   *sp, *cp;
    char   *word, *window;
    char *linep;
    int i, n;
    int ignoreR = false;
    struct keymap *keyList;

    if (!(hostEntry.busyThreshold=
          (float *)malloc(allInfo.numIndx*sizeof(float)))) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
        return -1;
    }
    if ((hostEntry.resList = (char **)
         malloc (allInfo.nRes *sizeof(char *))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
        return -1;
    }
    hostEntry.nRes = 0;

    keyList = initKeyList();

    linep = getNextLineC_(clfp, LineNum, true);
    if (! linep) {
        ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
                  fname, lsfile, *LineNum, "host");
        return -1;
    }

    if (isSectionEnd(linep, lsfile, LineNum, "host")) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5329,
                                         "%s: %s(%d): empty host section"),  /* catgets 5329 */
                  fname, lsfile, *LineNum);
        return -1;
    }

    if (strchr(linep, '=') == NULL) {
        if (! keyMatch(keyList, linep, false)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5330,
                                             "%s: %s(%d): keyword line format error for section host, ignoring section"), fname, lsfile, *LineNum); /* catgets 5330 */
            doSkipSection(clfp, LineNum, lsfile, "host");
            return -1;
        }


        i = 0;
        for (i=0; keyList[i].key != NULL; i++) {
            if (keyList[i].position != -1)
                continue;

            if ((strcasecmp("hostname", keyList[i].key) == 0) ||
                (strcasecmp("model", keyList[i].key) == 0) ||
                (strcasecmp("type", keyList[i].key) == 0) ||
                (strcasecmp("resources", keyList[i].key) == 0)) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5331,
                                                 "%s: %s(%d): keyword line: key %s is missing in section host, ignoring section"), fname, lsfile, *LineNum, keyList[i].key); /* catgets 5331 */
                doSkipSection(clfp, LineNum, lsfile, "host");
                freeKeyList (keyList);
                return -1;
            }
        }

        if (keyList[R].position != -1 && keyList[SERVER_].position != -1) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5332,
                                                 "%s: %s(%d): keyword line: conflicting keyword definition: you cannot define both 'R' and 'SERVER_'. 'R' ignored"), /* catgets 5332 */
                      fname, lsfile, *LineNum);
            lim_CheckError = WARNING_ERR;
            ignoreR = true;
        }

        while ((linep = getNextLineC_(clfp, LineNum, true)) != NULL) {

            freeKeyList (keyList);
            for (i = 0; i < hostEntry.nRes; i++)
                FREEUP (hostEntry.resList[i]);
            hostEntry.nRes = 0;

            if (isSectionEnd(linep, lsfile, LineNum, "host")) {

                struct hostNode *hPtr, *tPtr;
                for (hPtr = clPtr->hostList, clPtr->hostList = NULL;
                     hPtr; hPtr = tPtr) {
                    tPtr = hPtr->nextPtr;
                    hPtr->nextPtr = clPtr->hostList;
                    clPtr->hostList = hPtr;
                }
                for (hPtr = clPtr->clientList, clPtr->clientList = NULL;
                     hPtr; hPtr = tPtr) {
                    tPtr = hPtr->nextPtr;
                    hPtr->nextPtr = clPtr->clientList;
                    clPtr->clientList = hPtr;
                }
                return 0;
            }
            if (mapValues(keyList, linep) < 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5333,
                                                 "%s: %s(%d): values do not match keys for section host, ignoring line"), fname, lsfile, *LineNum); /* catgets 5333 */
                lim_CheckError = WARNING_ERR;
                continue;
            }


            if (keyList[TYPE].val[0] == '!')
                hostEntry.hostType[0] = '\0';
            else
                strcpy(hostEntry.hostType, keyList[TYPE].val);


            if (keyList[MODEL].val[0] == '!')
                hostEntry.hostModel[0] = '\0';
            else
                strcpy(hostEntry.hostModel, keyList[MODEL].val);

            strcpy(hostEntry.hostName, keyList[HOSTNAME_].val);
            if (keyList[ND].position != -1) {
                hostEntry.nDisks = atoi(keyList[ND].val);
            } else
                hostEntry.nDisks = 0;

            putThreshold(R15S, &hostEntry, keyList[R15S].position,
                         keyList[R15S].val, INFINIT_LOAD);
            putThreshold(R1M, &hostEntry, keyList[R1M].position,
                         keyList[R1M].val, INFINIT_LOAD);
            putThreshold(R15M, &hostEntry, keyList[R15M].position,
                         keyList[R15M].val, INFINIT_LOAD);
            if (keyList[UT].val
                && (cp = strchr (keyList[UT].val, '%')) != NULL)
                *cp = '\0';
            putThreshold(UT, &hostEntry, keyList[UT].position,
                         keyList[UT].val, INFINIT_LOAD);
            if (hostEntry.busyThreshold[UT] > 1.0
                && hostEntry.busyThreshold[UT] < INFINIT_LOAD) {
                ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd, NL_SETN, 5476, "%s: %s(%d): value for threshold ut <%2.2f> is greater than 1, assumming <%5.1f%%>")), /* catgets 5476 */ "dohosts", lsfile,
                          *LineNum, hostEntry.busyThreshold[UT],
                          hostEntry.busyThreshold[UT]);
                hostEntry.busyThreshold[UT] /= 100.0;
            }
            putThreshold(PG, &hostEntry, keyList[PG].position,
                         keyList[PG].val, INFINIT_LOAD);
            putThreshold(IO, &hostEntry, keyList[IO].position,
                         keyList[IO].val, INFINIT_LOAD);
            putThreshold(LS, &hostEntry, keyList[LS].position,
                         keyList[LS].val, INFINIT_LOAD);
            putThreshold(IT, &hostEntry, keyList[IT].position,
                         keyList[IT].val, -INFINIT_LOAD);
            putThreshold(TMP, &hostEntry, keyList[TMP].position,
                         keyList[TMP].val, -INFINIT_LOAD);
            putThreshold(SWP, &hostEntry, keyList[SWP].position,
                         keyList[SWP].val, -INFINIT_LOAD);
            putThreshold(MEM, &hostEntry, keyList[MEM].position,
                         keyList[MEM].val, -INFINIT_LOAD);

            for (i=NBUILTINDEX; i < allInfo.numIndx; i++) {
                if (keyList[i].key == NULL)
                    continue;

                if (allInfo.resTable[i].orderType == INCR)
                    putThreshold(i, &hostEntry, keyList[i].position,
                                 keyList[i].val, INFINIT_LOAD);
                else
                    putThreshold(i, &hostEntry, keyList[i].position,
                                 keyList[i].val, -INFINIT_LOAD);
            }

            for (i = NBUILTINDEX+allInfo.numUsrIndx; i < allInfo.numIndx; i++)
                hostEntry.busyThreshold[i] = INFINIT_LOAD;

            for (i=0; i < allInfo.numIndx; i++)
                if (keyList[i].position != -1)
                    FREEUP(keyList[i].val);
            n = 0;
            sp = keyList[RESOURCES].val;
            while ((word = getNextWord_(&sp)) != NULL) {
                hostEntry.resList[n] = putstr_(word);
                n++;
            }
            hostEntry.resList[n] = NULL;
            hostEntry.nRes = n;

            hostEntry.rexPriority = DEF_REXPRIORITY;
            if (keyList[REXPRI_].position != -1) {
                hostEntry.rexPriority = atoi(keyList[REXPRI_].val);
            }

            hostEntry.rcv = 1;
            if (keyList[R].position != -1) {
                if (!ignoreR)
                    hostEntry.rcv = atoi(keyList[R].val);
            }

            if (keyList[SERVER_].position != -1) {
                hostEntry.rcv = atoi(keyList[SERVER_].val);
            }

            window = NULL;
            if (keyList[RUNWINDOW].position != -1)  {
                if (strcmp(keyList[RUNWINDOW].val, "") == 0)
                    window = NULL;
                else
                    window = keyList[RUNWINDOW].val;
            }

            if (!addHost(clPtr, &hostEntry, window, lsfile, LineNum)) {
                clPtr->checkSum += hostEntry.hostName[0];
                lim_CheckError = WARNING_ERR;
            }

        }
        freeKeyList (keyList);
    } else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5335,
                                         "%s: %s(%d): horizontal host section not implemented yet, use vertical format: section ignored"), fname, lsfile, *LineNum); /* catgets 5335 */
        doSkipSection(clfp, LineNum, lsfile, "host");
        return -1;
    }


    ls_syslog(LOG_ERR, I18N_PREMATURE_EOF,
              fname, lsfile, *LineNum, "host");
    return -1;

}

static void
putThreshold(int indx, struct hostEntry *hostEntryPtr,
             int position, char *val, float def)
{
    if (position != -1) {
        if (strcmp(val, "") == 0)
            hostEntryPtr->busyThreshold[indx] = def;
        else
            hostEntryPtr->busyThreshold[indx] = atof(val);
    } else
        hostEntryPtr->busyThreshold[indx] = def;

}

int
typeNameToNo(const char *typeName)
{
    int i;

    for (i=0; i < allInfo.nTypes; i++) {
        if (strcmp(allInfo.hostTypes[i], typeName) == 0)
            return i;
    }
    return -1;
}

int
archNameToNo(const char *archName)
{
    int i, len, arch_speed, curr_speed, best_speed, best_pos;
    char *p;

    for (i = 0; i < allInfo.nModels; ++i) {
        if (strcmp(allInfo.hostArchs[i], archName) == 0) {
            return i;
        }
    }
    if ((p = strchr(archName, '_')) != NULL) {
        len = p - archName;
        arch_speed = atoi(++p);
    } else {
        len = strlen(archName);
        arch_speed = 0;
    }
    best_pos = 0;
    best_speed = 0;
    for (i = 0; i < allInfo.nModels; ++i) {
        if (strncmp(archName, allInfo.hostArchs[i], len))
            continue;
        p = strchr(allInfo.hostArchs[i], '_');
        curr_speed = p ? atoi(++p) : 0;
        if (arch_speed) {
            if ((arch_speed - curr_speed)*(arch_speed - curr_speed) <=
                (arch_speed - best_speed)*(arch_speed - best_speed)) {
                best_speed = curr_speed;
                best_pos = i;
            }
        } else {
            if (best_speed <= curr_speed) {
                best_speed = curr_speed;
                best_pos = i;
            }
        }
    }
    if (best_pos) {
        return best_pos;
    }
    return -1;
}

static
int modelNameToNo(char *modelName)
{
    int i;

    for (i=0; i < allInfo.nModels; i++) {
        if (strcmp(allInfo.hostModels[i], modelName) == 0)
            return i;
    }

    return -1;
}
static struct hostNode *
addHost(struct clusterNode *clPtr, struct hostEntry *hEntPtr, char *window, char *fileName, int *LineNumPtr)
{
    static char fname[] = "addHost()";
    struct hostNode *hPtr;
    struct hostent *hp;
    char *word;
    int i, resNo;


    if ((hp = (struct hostent *)getHostEntryByName_(hEntPtr->hostName)) == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5337,
                                         "%s: %s(%d): Invalid hostname %s in section host. Ignoring host"), /* catgets 5337 */
                  fname, fileName, *LineNumPtr, hEntPtr->hostName);
        return NULL;
    }


    if ((hPtr = findHostbyList(clPtr->hostList, hEntPtr->hostName)) != NULL) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5338,
                                             "%s: %s(%d): host <%s> redefined, using previous definition"), fname, fileName, *LineNumPtr, hEntPtr->hostName); /* catgets 5338 */
        return ((struct hostNode *)hPtr);
    }

    hPtr = findHostbyList(clPtr->clientList, hEntPtr->hostName);
    if (hPtr != NULL) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5339,
                                             "%s: %s(%d): host <%s> redefined, using previous definition"), fname, fileName, *LineNumPtr, hEntPtr->hostName); /* catgets 5339 */
        return ((struct hostNode *)hPtr);
    }
    if ((hPtr = initHostNode()) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }

    for (i = 0; i<hEntPtr->nRes; i++) {
        char *resStr;
        char  dedicated = false;
        int   resNo;


        if (hEntPtr->resList[i][0] == '!') {
            dedicated = true;
            resStr = hEntPtr->resList[i]+1;
        } else
            resStr = hEntPtr->resList[i];

        if ((resNo = validResource(resStr)) >= 0)  {
            if (resNo < INTEGER_BITS)
                hPtr->resClass |= (1<<resNo);
            SET_BIT (resNo, hPtr->resBitMaps);
            if (dedicated) {
                if (resNo < INTEGER_BITS)
                    hPtr->DResClass |= (1<<resNo);
                SET_BIT(resNo, hPtr->DResBitMaps);
            }

        } else {
            lim_CheckError = WARNING_ERR;
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5341,
                                             "%s: %s(%d): Invalid resource name <%s> for host %s in section %s; ignoring %s"), /* catgets 5341 */
                      fname, fileName, *LineNumPtr, resStr, hEntPtr->hostName, "Host", resStr);
        }
    }


    if (!hEntPtr->hostModel[0]) {
        hPtr->hModelNo = DETECTMODELTYPE;

    } else if ( (hPtr->hModelNo = modelNameToNo(hEntPtr->hostModel)) < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5342,
                                         "%s: %s(%d): Unknown host model %s. Ignoring host"), /* catgets 5342 */
                  fname, fileName, *LineNumPtr, hEntPtr->hostModel);
        freeHostNodes (hPtr, false);
        return NULL;
    }


    if (!hEntPtr->hostType[0]) {
        hPtr->hTypeNo = DETECTMODELTYPE;

    } else if ( (hPtr->hTypeNo = typeNameToNo(hEntPtr->hostType)) < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5343,
                                         "%s: %s(%d): Unknown host type %s. Ignoring host"), /* catgets 5343 */
                  fname, fileName, *LineNumPtr, hEntPtr->hostType);
        freeHostNodes (hPtr, false);
        return NULL;
    }

    hPtr->hostName = putstr_(hp->h_name);

    if (hEntPtr->rcv)
        hPtr->hostNo = clPtr->hostList ? clPtr->hostList->hostNo + 1 : 0;
    else
        hPtr->hostNo = clPtr->clientList ? clPtr->clientList->hostNo + 1 : 0;


    if (saveHostIPAddr(hPtr, hp) < 0 ) {
        ls_syslog(LOG_ERR, I18N(5473, "Can not save internet address of host %s"), hp->h_name); /* catgets 5473 */
        freeHostNodes (hPtr, false);
        return NULL;
    }

    hPtr->statInfo.nDisks = hEntPtr->nDisks;
    hPtr->rexPriority = hEntPtr->rexPriority;
    for (i=0; i < allInfo.numIndx; i++)
        hPtr->busyThreshold[i] = hEntPtr->busyThreshold[i];

    for (i=0; i<8; i++)
        hPtr->week[i] = NULL;
    if (window && hEntPtr->rcv) {
        hPtr->windows = putstr_(window);
        while ((word = getNextWord_(&window)) != NULL) {
            if (addWindow(word, hPtr->week, hPtr->hostName) <0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5345,
                                                 "%s: %s(%d): Bad time expression %s; ignored"), /* catgets 5345 */
                          fname, fileName, *LineNumPtr, word);
                lim_CheckError = WARNING_ERR;
                free(hPtr->windows);
                hPtr->windows = "-";
                hPtr->wind_edge = 0;
                break;
            }
            hPtr->wind_edge = time(0);
        }
    } else {
        hPtr->windows = "-";
        hPtr->wind_edge = 0;
    }

    if (hEntPtr->rcv) {
        hPtr->nextPtr = clPtr->hostList;
        clPtr->hostList = hPtr;
        hPtr->hostInactivityCount = 0;
    } else {
        hPtr->nextPtr = clPtr->clientList;
        clPtr->clientList = hPtr;
        hPtr->hostInactivityCount = -1;
    }

    if (hEntPtr->rcv)
        for (resNo = 0; resNo < allInfo.nRes; resNo++) {
            int isSet, j;
            char *value, *name;;

            TEST_BIT(resNo, hPtr->resBitMaps, isSet);
            if (isSet == 0)
                continue;

            name = shortInfo.resName[resNo];
            j = resNameDefined (shortInfo.resName[resNo]);
            if (allInfo.resTable[j].valueType == LS_BOOLEAN)
                value = "1";
            else
                value = "";
        }
    numofhosts++;
    return hPtr;
}

struct hostNode *
addFloatClientHost(struct hostent *hp)
{
    static char fname[] = "addFloatClientHost()";
    struct hostNode *hPtr, *lastHPtr;
    int i;


    if (hp == NULL) {
        ls_syslog(LOG_ERR, I18N(5406,
                                "%s: Invalid hostAddr. Ignoring host"), /* catgets 5406 */
                  fname);
        return NULL;
    }


    if (findHostInCluster(hp->h_name)) {
        ls_syslog(LOG_ERR, I18N(5407,
                                "%s: <%s> already defined in this cluster"), /* catgets 5407 */
                  fname, hp->h_name);

        return NULL;
    }


    hPtr = initHostNode();
    if (hPtr == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }


    hPtr->hTypeNo = DETECTMODELTYPE;


    hPtr->hModelNo = DETECTMODELTYPE;


    hPtr->hostName = putstr_(hp->h_name);
    if (!hPtr->hostName) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        freeHostNodes (hPtr, false);
        return NULL;
    }


    hPtr->hostNo = -1;


    for (hPtr->naddr=0;hp->h_addr_list && hp->h_addr_list[hPtr->naddr] != NULL; hPtr->naddr++);
    if (hPtr->naddr){
        hPtr->addr = (u_int *)malloc(hPtr->naddr*sizeof(u_int));
        if (!hPtr->addr) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            freeHostNodes (hPtr, false);
            return NULL;
        }
    } else
        hPtr->addr = 0;

    for (i=0; i < hPtr->naddr; i++)
        memcpy((char *)&hPtr->addr[i], hp->h_addr_list[i], hp->h_length);

    for (i=0; i<8; i++)
        hPtr->week[i] = NULL;

    hPtr->windows = putstr_("-");
    if (!hPtr->windows) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        freeHostNodes (hPtr, false);
        return NULL;
    }
    hPtr->wind_edge = 0;


    if (myClusterPtr->clientList == NULL) {

        myClusterPtr->clientList = hPtr;
        hPtr->nextPtr = NULL;
    } else {

        for (lastHPtr = myClusterPtr->clientList; lastHPtr->nextPtr != NULL;
             lastHPtr = lastHPtr->nextPtr);
        lastHPtr->nextPtr = hPtr;
        hPtr->nextPtr = NULL;
    }

    hPtr->hostInactivityCount = -1;
    numofhosts++;
    return hPtr;
}

int
removeFloatClientHost(struct hostNode *hPtr)
{
    static char fname[] = "removeFloatClientHost()";
    struct hostNode *tempPtr = NULL;

    if (logclass & LC_TRACE) {
        ls_syslog(LOG_DEBUG, "%s Entering ... ", fname);
    }

    if (hPtr == NULL) {
        ls_syslog(LOG_ERR, I18N(5408,
                                "%s: hostNode is invalid"), /* catgets 5408 */
                  fname);
        return -1;
    }

    if (myClusterPtr->clientList == hPtr) {

        myClusterPtr->clientList = hPtr->nextPtr;
    } else {

        for (tempPtr = myClusterPtr->clientList;
             tempPtr && tempPtr->nextPtr != hPtr; tempPtr = tempPtr->nextPtr);
        if (tempPtr == NULL) {
            ls_syslog(LOG_ERR, I18N(5409,
                                    "%s: host <%s> not found in client list"), /* catgets 5409 */
                      fname,  hPtr->hostName);
            return -1;
        }
        tempPtr->nextPtr = hPtr->nextPtr;
    }
    hPtr->nextPtr = NULL;


    numofhosts--;
    freeHostNodes(hPtr, false);

    return 0;
}


struct hostNode *
initHostNode (void)
{
    static char fname[] = "initHostNode()";
    struct hostNode *hPtr;
    int i;

    if ((hPtr = (struct hostNode *) malloc(sizeof(struct hostNode))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    hPtr->resBitMaps = NULL;
    hPtr->DResBitMaps = NULL;
    hPtr->status = NULL;

    if ((hPtr->resBitMaps = (int *)
         malloc(GET_INTNUM(allInfo.nRes) * sizeof (int))) == NULL
        || (hPtr->DResBitMaps = (int *)
            malloc(GET_INTNUM(allInfo.nRes) * sizeof (int))) == NULL
        || (hPtr->status = (int *)
            malloc((1+GET_INTNUM(allInfo.numIndx)) * sizeof (int))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        FREEUP (hPtr->resBitMaps);
        FREEUP (hPtr->DResBitMaps);
        FREEUP (hPtr->status);
        FREEUP (hPtr);
        return NULL;
    }
    for (i =0; i< GET_INTNUM(allInfo.nRes); i++) {
        hPtr->resBitMaps[i] = 0;
        hPtr->DResBitMaps[i] = 0;
    }
    hPtr->loadIndex = NULL;
    hPtr->uloadIndex = NULL;
    hPtr->busyThreshold = NULL;

    if ((hPtr->loadIndex = (float *) malloc (allInfo.numIndx * sizeof (float))) == NULL
        || (hPtr->uloadIndex = (float *)
            malloc (allInfo.numIndx * sizeof (float))) == NULL
        || (hPtr->busyThreshold = (float *)
            malloc (allInfo.numIndx * sizeof (float))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        FREEUP (hPtr->resBitMaps);
        FREEUP (hPtr->DResBitMaps);
        FREEUP (hPtr->status);
        FREEUP (hPtr->loadIndex);
        FREEUP (hPtr->uloadIndex);
        FREEUP (hPtr->busyThreshold);
        FREEUP (hPtr);
        return NULL;
    }

    for (i =0; i < allInfo.numIndx; i++) {
        hPtr->loadIndex[i]  = INFINIT_LOAD;
        hPtr->uloadIndex[i] = INFINIT_LOAD;
    }

    for (i = NBUILTINDEX; i < allInfo.numIndx; i++)
        hPtr->busyThreshold[i] = (allInfo.resTable[i].orderType == INCR) ?
            INFINIT_LOAD : -INFINIT_LOAD;

    hPtr->hostName = NULL;
    hPtr->hModelNo = 0;
    hPtr->hTypeNo = 0;
    hPtr->hostNo = 0;
    hPtr->naddr = 0;
    hPtr->addr = NULL;
    hPtr->infoValid = false;
    hPtr->protoVersion = 0;
    hPtr->availHigh = 0;
    hPtr->availLow = 0;
    hPtr->use = -1;
    hPtr->resClass = 0;
    hPtr->DResClass = 0;
    hPtr-> nRes = 0;
    hPtr->windows = NULL;


    hPtr->statInfo.maxMem = 0;
    hPtr->statInfo.maxSwap = 0;
    hPtr->statInfo.maxTmp = 0;

    for (i = 0; i < 8; i++)
        hPtr->week[i] = NULL;

    hPtr->wind_edge = 0;
    hPtr->lastJackTime= (time_t) 0;
    hPtr->hostInactivityCount = 0;

    for (i = 0; i < 1+GET_INTNUM(allInfo.numIndx); i++)
        hPtr->status[i] = 0;
    hPtr->status[0] = LIM_UNAVAIL;

    hPtr-> conStatus = 0;
    hPtr->lastSeqNo = 0;
    hPtr->rexPriority = 0;
    hPtr->infoMask = 0;
    hPtr->loadMask = 0;
    hPtr->numInstances = 0;
    hPtr->instances = NULL;

    hPtr->callElim = false ;
    hPtr->maxResIndex = 0;
    hPtr->resBitArray = NULL;

    hPtr->nextPtr = NULL;
    hPtr->expireTime = -1;

    return hPtr;

}

void
freeHostNodes (struct hostNode *hPtr, int allList)
{

    int i;
    struct hostNode *next;

    while (hPtr) {
        FREEUP (hPtr->hostName);
        FREEUP (hPtr->addr);
        FREEUP (hPtr->windows);

        if (allList == false) {
            for (i = 0; i < 8; i++)
                FREEUP (hPtr->week[i]);
        }
        FREEUP (hPtr->busyThreshold);
        FREEUP (hPtr->loadIndex);
        FREEUP (hPtr->uloadIndex);
        FREEUP (hPtr->resBitMaps);
        FREEUP (hPtr->DResBitMaps);
        FREEUP (hPtr->status);
        FREEUP (hPtr->instances);

        next = hPtr->nextPtr;


        FREEUP (hPtr);

        if (allList == true)
            hPtr = next;
    }
}


static struct sharedResource *
addResource (char *resName, int nHosts, char **hosts, char *value,
             char *fileName, int LineNum, int resourceMap)
{
    static char fname[] = "addResource()";
    int i;

    struct sharedResource *temp, **temp1;

    if (resName == NULL || hosts == NULL)
        return NULL;

    if ((temp = (struct sharedResource *)
         malloc (sizeof (struct sharedResource))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    if ((temp->resourceName = putstr_(resName)) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    temp->numInstances = 0;
    temp->instances = NULL;
    if (addHostInstance (temp, nHosts, hosts, value, resourceMap) < 0)
        return NULL;

    if (numHostResources == 0)
        temp1 = (struct sharedResource **) malloc (sizeof (struct sharedResource *));
    else
        temp1 = (struct sharedResource **) realloc (hostResources,
                                                    (numHostResources + 1) * sizeof (struct sharedResource *));
    if (temp1 == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        freeSharedRes (temp);
        for (i = 0; i < numHostResources; i++)
            freeSharedRes (hostResources[i]);
        FREEUP (hostResources);
        return NULL;
    }
    hostResources = temp1;
    hostResources[numHostResources] = temp;
    numHostResources++;
    return temp;

}

static void
freeSharedRes (struct sharedResource *sharedRes)
{

    int i;

    if (sharedRes == NULL)
        return;
    FREEUP (sharedRes->resourceName);

    for (i = 0; i < sharedRes->numInstances; i++)
        freeInstance (sharedRes->instances[i]);
    FREEUP (sharedRes);

}

static int
addHostInstance (struct sharedResource *sharedResource,  int nHosts, char **hostNames, char *value, int resourceMap)
{

    static char fname[] = "addHostInstance()";
    int i, j, numList = 0;
    static char **temp = NULL;
    static int numHosts = 0;
    char **hostList;
    struct  resourceInstance *instance;

    if (nHosts <= 0 || hostNames == NULL)
        return -1;

    if (resourceMap == false) {

        if (sharedResource->numInstances > 1) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5405,
                                             "%s: More than one instatnce defined for the resource <%s> on host <%s> in host section; ignoring"), fname, sharedResource->resourceName, hostNames[0]); /* catgets 5405 */
            return -1;
        }
        if (sharedResource->numInstances == 0) {

            if (addInstance (sharedResource, nHosts, hostNames, value) == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "addInstance");
                return -1;
            }
        } else {

            if (addHostList (sharedResource->instances[0], nHosts, hostNames) < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "addHostList");
                return -1;
            }
        }
        instance = sharedResource->instances[0];

        if (addHostNodeIns(instance, nHosts, hostNames) < 0)
            return -1;
    } else {

        if (numHosts == 0 && temp == NULL) {
            if ((temp = (char **)
                 malloc (numofhosts * sizeof (char *))) == NULL) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
                return -1;
            }
        } else {
            for (i = 0; i < numHosts; i++)
                FREEUP (temp[i]);
        }
        numHosts = 0;
        for (i = 0; i < nHosts; i++) {
            if ((hostList = getValidHosts (hostNames[i], &numList, sharedResource)) == NULL)
                continue;
            for (j = 0; j < numList; j++) {

                int k;
                int duplicated=0;
                for (k = 0; k < numHosts; k++) {
                    if (!strcmp(temp[k], hostList[j])) {
                        duplicated = 1;
                        break;
                    }
                }
                if (duplicated) {
                    ls_syslog(LOG_WARNING, I18N(5478,
                                                "%s: Host %s is duplicated in resource %s mapping."), /* catgets 5478 */
                              fname, hostList[j], sharedResource->resourceName);
                    continue;
                }
                temp[numHosts] = putstr_(hostList[j]);
                if (temp[numHosts] == NULL) {
                    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
                    return -1;
                }
                numHosts++;
            }
        }
        if ((instance = addInstance (sharedResource, numHosts, temp, value)) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "addInstance");
            return -1;
        }

        if (addHostNodeIns(instance, numHosts, temp) < 0)
            return -1;




        if(addSharedResourceInstance(numHosts, temp, sharedResource->resourceName) < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname,
                      "addSharedResourceInstance");
            return -1 ;
        }
    }
    return 0;

}

static char **
getValidHosts (char *hostName, int *numHost, struct sharedResource *resource)
{

    static char fname[] = "getValidHosts";
    static char **temp = NULL;
    int num;
    struct hostNode *hPtr;
    const char *officialName;

    *numHost = 0;
    if (temp == NULL) {
        if ((temp = (char **) malloc (numofhosts * sizeof (char *))) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return NULL;
        }
    }

    if (!strcmp (hostName, "all") || !strcmp (hostName, "others")) {
        if (resource->numInstances > 0 && !strcmp (hostName, "all")) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5349,
                                             "%s: Shared resource <%s> has more than one instance, reserved word <all> can not be specified;ignoring"), fname, resource->resourceName);  /* catgets 5349 */
            return NULL;
        }
        for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
            if (isInHostList (resource, hPtr->hostName) == NULL) {
                temp[*numHost] = hPtr->hostName;
                (*numHost)++;
            }
        }
        return temp;
    }

    if ((officialName = getHostOfficialByName_(hostName)) != NULL) {
        if ((hPtr = findHostbyList(myClusterPtr->hostList, (char*)officialName))
            == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5413,
                                             "%s: Host <%s> is not used by cluster <%s>;ignoring"), fname, hostName, myClusterName); /* catgets 5413 */
            return NULL;
        }
        if (isInHostList (resource, (char *)officialName) != NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5414,
                                             "%s: Host <%s> is defined in more than one instance for resource <%s>; ignoring"), fname, hostName, resource->resourceName); /* catgets 5414 */
            return NULL;
        }
        *numHost = 1;
        temp[0] = hPtr->hostName;
        return temp;
    }


    if ((num = typeNameToNo (hostName)) > 0) {
        for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
            if (hPtr->hTypeNo == num
                && isInHostList (resource, hPtr->hostName) == NULL) {
                temp[*numHost] = hPtr->hostName;
                (*numHost)++;
            }
        }
        return temp;
    }

    if ((num = modelNameToNo (hostName)) > 0) {
        for (hPtr = myClusterPtr->hostList; hPtr; hPtr = hPtr->nextPtr) {
            if (hPtr->hModelNo == num
                && isInHostList (resource, hPtr->hostName) == NULL) {
                temp[*numHost] = hPtr->hostName;
                (*numHost)++;
            }
        }
        return temp;
    }
    return NULL;
}

static int
addHostNodeIns (struct resourceInstance *instance, int nHosts,
                char **hostNames)
{

    static char fname[] = "addHostNodeIns";
    int i, resNo;
    struct hostNode *hPtr;
    const char *officialName;
    struct  resourceInstance **temp;

    if ((resNo = resNameDefined (instance->resName)) < 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5415,
                                         "%s: Resource name <%s> is not defined in resource section in lsf.shared"), fname, instance->resName); /* catgets 5415 */
        return 0;
    }
    for (i = 0; i < nHosts; i++) {
        if (hostNames[i] == NULL)
            continue;

        if ((officialName = getHostOfficialByName_(hostNames[i])) == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5350,
                                             "%s: Invalid hostname <%s> "), fname, hostNames[i]); /* catgets 5350 */
            continue;
        }


        if ((hPtr = findHostbyList(myClusterPtr->hostList, (char*)officialName))
            == NULL) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 5351,
                                                 "%s: Host <%s> is not defined in host sectionin lsf.cluster"), fname, hostNames[i]); /* catgets 5351 */
            continue;
        }
        if (hPtr->numInstances > 0 && isInHostNodeIns
            (instance->resName, hPtr->numInstances, hPtr->instances) != NULL)
            continue;

        if (hPtr->numInstances > 0)
            temp = (struct resourceInstance **) realloc (hPtr->instances,
                                                         (hPtr->numInstances + 1) * sizeof(struct resourceInstance *));
        else
            temp = (struct resourceInstance **) malloc (sizeof(struct resourceInstance *));

        if (temp == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }
        temp[hPtr->numInstances] = instance;
        hPtr->instances = temp;
        hPtr->numInstances++;
    }
    return 0;
}

static struct  resourceInstance *
isInHostNodeIns (char *resName, int numInstances, struct resourceInstance **instances)
{
    int i;

    if (numInstances <= 0 || instances == NULL)
        return NULL;
    for ( i = 0; i < numInstances; i ++)
        if (!strcmp (resName, instances[i]->resName))
            return (instances[i]);
    return NULL;
}

static int
addHostList (struct resourceInstance *resourceInstance, int nHosts, char **hostNames)
{
    static char fname[] = "addHostList()";
    struct hostNode **temp;
    int i;
    struct hostNode *hostPtr;

    if (resourceInstance == NULL || nHosts <= 0 || hostNames == NULL)
        return -1;

    if (resourceInstance->nHosts == 0)
        temp = (struct hostNode **)
            malloc (nHosts * sizeof (struct hostNode *));
    else
        temp = (struct hostNode **) realloc (resourceInstance->hosts,
                                             (resourceInstance->nHosts + 1) * sizeof (struct hostNode *));

    if (temp == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }
    resourceInstance->hosts = temp;

    for (i = 0; i < nHosts; i++) {
        if ((hostPtr = findHostbyList(myClusterPtr->hostList,
                                      hostNames[i])) == NULL) {
            ls_syslog (LOG_DEBUG3, "addHostList: Host <%s> is not used by cluster <%s> as a server:ignoring", hostNames[i], myClusterName);
            continue;
        }
        resourceInstance->hosts[resourceInstance->nHosts] = hostPtr;
        resourceInstance->nHosts++;
    }
    return 0;

}
static struct resourceInstance *
addInstance (struct sharedResource *sharedResource,  int nHosts, char **hostNames, char *value)
{

    static char fname[] = "addInstance()";
    int i, resNo;
    struct resourceInstance **insPtr, *temp;
    struct hostNode *hPtr;

    if (nHosts <= 0 || hostNames == NULL)
        return NULL;

    if ((temp = initInstance()) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    if ((temp->hosts = (struct hostNode **) malloc (nHosts * sizeof (struct hostNode *))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    temp->resName = sharedResource->resourceName;
    resNo = validResource(temp->resName);
    for (i = 0; i < nHosts; i++) {
        if (hostNames[i] == NULL)
            continue;

        if ((hPtr = findHostbyList(myClusterPtr->hostList, hostNames[i])) == NULL) {
            ls_syslog (LOG_DEBUG3, "addInstance: Host <%s> is not used by cluster <%s> as server;ignoring", hostNames[i], myClusterName);
            continue;
        }
        temp->hosts[temp->nHosts] = hPtr;
        temp->nHosts++;
    }
    if (value[0] == '\0')
        strcpy (value, "-");
    if ((temp->value = putstr_(value)) == NULL
        || (temp->orignalValue = putstr_(value)) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        freeInstance (temp);
        return NULL;
    }
    if ((insPtr = (struct resourceInstance **) myrealloc (sharedResource->instances,
                                                          (sharedResource->numInstances + 1) * sizeof (char *))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "myrealloc");
        freeInstance (temp);
        return NULL;
    }
    sharedResource->instances = insPtr;
    sharedResource->instances[sharedResource->numInstances] = temp;
    sharedResource->numInstances++;

    return temp;
}


static struct resourceInstance *
initInstance (void)
{

    static char fname[] = "initInstance()";
    struct resourceInstance *temp;

    if ((temp = (struct resourceInstance *)
         malloc (sizeof (struct resourceInstance))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }
    temp->nHosts = 0;
    temp->resName = NULL;
    temp->hosts = NULL;
    temp->orignalValue = NULL;
    temp->value = NULL;
    temp->updateTime = 0;
    temp->updHost = NULL;

    return temp;

}

static void
freeInstance (struct resourceInstance *instance)
{

    if (instance == NULL)
        return;
    FREEUP (instance->hosts);
    FREEUP (instance->orignalValue);
    FREEUP (instance->value);
    FREEUP (instance);

}

struct resourceInstance *
isInHostList (struct sharedResource  *sharedResource,  char *hostName)
{

    int i, j;

    if (sharedResource->numInstances <= 0)
        return NULL;

    for (i=0; i <sharedResource->numInstances; i++) {
        if (sharedResource->instances[i]->nHosts <= 0
            || sharedResource->instances[i]->hosts == NULL)
            continue;
        for (j = 0; j < sharedResource->instances[i]->nHosts; j++) {
            if (strcmp(sharedResource->instances[i]->hosts[j]->hostName, hostName) == 0)
                return (sharedResource->instances[i]);
        }
    }
    return NULL;
}

struct sharedResource *
inHostResourcs (char *resName)
{

    int i;

    if (numHostResources <= 0)
        return NULL;

    for (i=0; i < numHostResources; i++) {
        if (strcmp(hostResources[i]->resourceName, resName) == 0)
            return (hostResources[i]);
    }
    return NULL;

}

int
validResource(const char *resName)
{
    int i;

    for (i = 0; i < shortInfo.nRes; i++) {
        if (strcmp(shortInfo.resName[i], resName) == 0)
            break;
    }


    if (i == shortInfo.nRes)
        return -1;

    return i;

}

int
validLoadIndex(const char *resName)
{
    int i;

    for (i = 0; i < allInfo.numIndx; i++) {
        if (strcmp(li[i].name, resName) == 0)
            return i;
    }

    return -1;

}


bool_t
validHostType(const char *hType)
{
    int i;

    for (i = 0;i < shortInfo.nTypes; i++) {
        if (strcmp(shortInfo.hostTypes[i], hType) == 0)
            return i;
    }

    return -1;

}
int
validHostModel(const char *hModel)
{
    int i;

    for (i = 0; i < allInfo.nModels; i++) {
        if (strcmp(allInfo.hostModels[i], hModel) == 0)
            return i;
    }

    return -1;

}


static int cntofdefault=0;
static char
addHostType(char *type)
{
    static char fname[] = "addHostType()";
    int i;

    if (allInfo.nTypes == MAXTYPES) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5353,
                                         "%s: Too many host types defined in section HostType. You can only define up to %d host types; host type %s ignored"),  /* catgets 5353 */
                  fname, MAXTYPES, type);
        return false;
    }


    for (i=0;i<allInfo.nTypes;i++) {
        if (strcmp(allInfo.hostTypes[i], type) != 0)
            continue;
        if (strcmp(type,"DEFAULT")==0)
        {
            cntofdefault++;
            if (cntofdefault<=1) break;
        }
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5354,
                                         "%s: host type %s multiply defined"), /* catgets 5354 */
                  fname, type);
        return false;
    }

    strcpy(allInfo.hostTypes[allInfo.nTypes], type);
    shortInfo.hostTypes[shortInfo.nTypes] =
        allInfo.hostTypes[allInfo.nTypes];
    allInfo.nTypes++;
    shortInfo.nTypes++;
    return true;

}

static char
addHostModel(char *model, char *arch, float factor)
{
    static char fname[] = "addHostModel()";
    int i;

    if (allInfo.nModels == MAXMODELS) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5355,
                                         "%s: Too many host models defined in section HostModel. You can only define up to %d host models; host model %s ignored"),  /* catgets 5355 */
                  fname, MAXMODELS, model);
        return false;
    }

    if (!strcmp(model,"DEFAULT"))
    {
        strcpy(allInfo.hostArchs[1], arch? arch: "");
        allInfo.cpuFactor[1] = factor;
        return true;
    }

    for (i = 0; i < allInfo.nModels; ++i) {

        if (!arch && strcmp(allInfo.hostModels[i], model) == 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5357,
                                             "%s: host model %s multiply defined"), /* catgets 5357 */
                      fname, model);
            return true;
        }

        if (!arch || strcmp(allInfo.hostArchs[i], arch) != 0)
            continue;

        ls_syslog(LOG_ERR, I18N(5479,
                                "%s: host architecture %s defined multiple times"),/*catgets 5479*/
                  fname, arch);

        return true;
    }

    strcpy(allInfo.hostModels[allInfo.nModels], model);
    strcpy(allInfo.hostArchs[allInfo.nModels], arch? arch: "");
    allInfo.cpuFactor[allInfo.nModels] = factor;
    shortInfo.hostModels[shortInfo.nModels] =
        allInfo.hostModels[allInfo.nModels];
    allInfo.nModels++;
    shortInfo.nModels++;
    return true;

}

static struct clusterNode *
addCluster(char *clName, char *candlist)
{
    static char fname[] = "addCluster()";
    char *sp, *word;
    int i;
    struct hostent *hp;
    static int nextClNo = 0;

    if (myClusterPtr != NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5356,
                                         "%s: Ignoring duplicate cluster %s"), /* catgets 5356 */
                  fname, clName);
        return NULL;
    }

    myClusterPtr = (struct clusterNode *)malloc(sizeof(struct clusterNode));
    if (myClusterPtr == (struct clusterNode *)NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return NULL;
    }

    memset((char *) myClusterPtr, 0, sizeof(struct clusterNode));

    myClusterPtr->clName = putstr_(clName);
    myClusterPtr->clusterNo = nextClNo++;

    if (!candlist || candlist[0] == '\0')
        candlist = findClusterServers(clName);

    sp = (char *) candlist;
    i = 0;
    while ( ((word = getNextWord_(&sp)) != NULL) && (i < MAXCANDHOSTS)) {
        hp = (struct hostent *)getHostEntryByName_(word);
        if (!hp) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5358,
                                             "Invalid host %s for cluster %s, ignoring"), /* catgets 5358 */
                      word, clName);
            lim_CheckError = WARNING_ERR;
            continue;
        }
        myClusterPtr->candAddrList[i] =  *((u_int *) hp->h_addr);
        i++;
    }
    myClusterPtr->candAddrList[i] = (u_int)0;
    myClusterPtr->status = CLUST_ACTIVE | CLUST_STAT_UNAVAIL;
    myClusterPtr->masterKnown = false;
    myClusterPtr->masterInactivityCount = 0;
    myClusterPtr->masterPtr = (struct hostNode *)NULL;
    myClusterPtr->prevMasterPtr = (struct hostNode *)NULL;
    myClusterPtr->hostList = (struct hostNode *)NULL;
    myClusterPtr->clientList = (struct hostNode *)NULL;
    myClusterPtr->eLimArgs   = (char *)NULL;
    myClusterPtr->eLimArgv   = (char **)NULL;
    myClusterPtr->currentAddr = 0;
    myClusterPtr->masterName = (char *)NULL;
    myClusterPtr->managerName = (char *)NULL;
    myClusterPtr->resClass = 0;
    myClusterPtr->typeClass = 0;
    myClusterPtr->modelClass = 0;
    myClusterPtr->chanfd = -1;
    myClusterPtr->numIndx = 0;
    myClusterPtr->numUsrIndx = 0;
    myClusterPtr->usrIndxClass = 0;
    myClusterPtr->nAdmins = 0;
    myClusterPtr->adminIds = NULL;
    myClusterPtr->admins   = NULL;
    myClusterPtr->nRes = 0;
    myClusterPtr->resBitMaps = NULL;
    myClusterPtr->hostTypeBitMaps = NULL;
    myClusterPtr->hostModelBitMaps = NULL;

    return myClusterPtr;
}

static char *
findClusterServers(char *clName)
{
    static char fname[]="findClusterServers";
    static char servers[MAXLINELEN];
    FILE   *clfp;
    char   *cp;
    int LineNum = 0;
    char   fileName[MAXFILENAMELEN];
    char   *word, *linep;

    sprintf(fileName, "%s/lsf.cluster.%s",
            limParams[LSF_CONFDIR].paramValue, clName);
    servers[0]='\0';

    if ( (clfp = confOpen(fileName, "r")) == NULL) {
        return servers;
    }

    for (;;) {
        cp = getBeginLine(clfp, &LineNum);
        if (!cp) {
            FCLOSEUP(&clfp);
            return servers;
        }
        word = getNextWord_(&cp);
        if (!word) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5360,
                                             "%s: %s(%d): Keyword expected after Begin, ignoring section"),fname, fileName, LineNum); /* catgets 5360 */
            lim_CheckError = WARNING_ERR;
            doSkipSection(clfp, &LineNum, fileName, "unknown");
        } else if (strcasecmp(word, "host") == 0) {
            char first = true;
            int  nServers=0;


            while ((linep = getNextLineC_(clfp, &LineNum, true)) != NULL) {
                if (isSectionEnd(linep, fileName, &LineNum, "host")) {
                    FCLOSEUP(&clfp);
                    return servers;
                }

                if (first) {
                    first = false;
                    continue;
                }
                cp = linep;
                word = getNextWord_(&cp);
                if (word) {
                    nServers++;
                    strcat(servers,word);
                    strcat(servers," ");
                }
                if (nServers > MAXCANDHOSTS)
                    break;
            }
            FCLOSEUP(&clfp);
            return servers;
        } else {
            doSkipSection(clfp, &LineNum, fileName, word);
        }
    }
}


void
reCheckRes(void)
{
    static char fname[] = "reCheckRes()";
    int i,j, resNo;
    struct resItem *newTable;

    allInfo.numIndx = 0;


    newTable = (struct resItem *)malloc(allInfo.nRes * sizeof(struct resItem));
    if (newTable == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        lim_Exit("reCheckRes");
    }


    for (i=0,j=0; i < allInfo.nRes; i++) {

        if (allInfo.resTable[i].valueType == LS_NUMERIC
            && (allInfo.resTable[i].flags & RESF_DYNAMIC)
            && (allInfo.resTable[i].flags & RESF_GLOBAL)) {
            memcpy((char *)&newTable[j], (char *)&allInfo.resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }
    for (i=0; i < allInfo.nRes; i++) {

        if (allInfo.resTable[i].valueType == LS_NUMERIC
            && (!(allInfo.resTable[i].flags & RESF_DYNAMIC) ||
                !(allInfo.resTable[i].flags & RESF_GLOBAL))) {
            memcpy((char *)&newTable[j], (char *)&allInfo.resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }

    for (i=0; i < allInfo.nRes; i++) {
        if (allInfo.resTable[i].valueType == LS_BOOLEAN) {
            memcpy((char *)&newTable[j], (char *)&allInfo.resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }

    for (i=0; i < allInfo.nRes; i++) {
        if (allInfo.resTable[i].valueType == LS_STRING) {
            memcpy((char *)&newTable[j], (char *)&allInfo.resTable[i],
                   sizeof(struct resItem));
            j++;
        }
    }
    free(allInfo.resTable);
    allInfo.resTable = newTable;


    shortInfo.nRes = 0;
    shortInfo.resName = (char **) malloc (allInfo.nRes * sizeof(char*));
    shortInfo.stringResBitMaps = (int *) malloc (GET_INTNUM(allInfo.nRes) *
                                                 sizeof (int));
    shortInfo.numericResBitMaps = (int *) malloc (GET_INTNUM(allInfo.nRes) *
                                                  sizeof (int));
    if (shortInfo.resName == NULL || shortInfo.stringResBitMaps == NULL
        || shortInfo.numericResBitMaps == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        lim_Exit("reCheckRes");
    }
    for (i = 0; i < GET_INTNUM(allInfo.nRes); i++) {
        shortInfo.stringResBitMaps[i] = 0;
        shortInfo.numericResBitMaps[i] = 0;
    }
    for (resNo=0; resNo < allInfo.nRes; resNo++) {
        if ((allInfo.resTable[resNo].flags & RESF_DYNAMIC)
            && (allInfo.resTable[resNo].valueType == LS_NUMERIC)
            && (allInfo.resTable[resNo].flags & RESF_GLOBAL))
            allInfo.numIndx++;


        if ((allInfo.resTable[resNo].flags & RESF_BUILTIN)
            || (allInfo.resTable[resNo].flags & RESF_DYNAMIC
                && allInfo.resTable[resNo].valueType == LS_NUMERIC) ||
            (allInfo.resTable[resNo].valueType != LS_STRING
             && allInfo.resTable[resNo].valueType != LS_BOOLEAN))
            continue;
        shortInfo.resName[shortInfo.nRes] = putstr_(allInfo.resTable[resNo].name);
        if (shortInfo.resName[shortInfo.nRes] == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            lim_Exit(fname);
        }
        if (allInfo.resTable[resNo].valueType == LS_STRING)
            SET_BIT (shortInfo.nRes,  shortInfo.stringResBitMaps);
        shortInfo.nRes++;
    }
    shortInfo.nModels = allInfo.nModels;
    for (i=0; i < allInfo.nModels; i++) {
        shortInfo.hostModels[i] = allInfo.hostModels[i];
        shortInfo.cpuFactors[i] = allInfo.cpuFactor[i];
    }

}

static int
reCheckClusterClass(struct clusterNode *clPtr)
{
    static char fname[] = "reCheckClusterClass()";
    struct hostNode *hPtr;
    int i, j;

    clPtr->resClass = 0;
    clPtr->typeClass = 0;
    clPtr->modelClass = 0;
    clPtr->numHosts = 0;
    clPtr->numClients = 0;
    clPtr->nRes = 0;

    ls_syslog(LOG_DEBUG1, "reCheckClusterClass: cluster <%s>", clPtr->clName);
    if (clPtr->resBitMaps == NULL) {
        clPtr->resBitMaps = (int *)malloc (GET_INTNUM(allInfo.nRes) *
                                           sizeof (int));
        if (clPtr->resBitMaps == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }
        for (i = 0; i < GET_INTNUM(allInfo.nRes); i++)
            clPtr->resBitMaps[i] = 0;
    }
    if (clPtr->hostTypeBitMaps == NULL) {
        clPtr->hostTypeBitMaps = (int *)malloc (GET_INTNUM(allInfo.nTypes) *
                                                sizeof (int));
        if (clPtr->hostTypeBitMaps == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }
        for (i = 0; i < GET_INTNUM(allInfo.nTypes); i++)
            clPtr->hostTypeBitMaps[i] = 0;
    }
    if (clPtr->hostModelBitMaps == NULL) {
        clPtr->hostModelBitMaps = (int *)malloc (GET_INTNUM(allInfo.nModels) *
                                                 sizeof (int));
        if (clPtr->hostModelBitMaps == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }
        for (i = 0; i < GET_INTNUM(allInfo.nModels); i++)
            clPtr->hostModelBitMaps[i] = 0;
    }
    for (hPtr=clPtr->hostList; hPtr; hPtr=hPtr->nextPtr) {
        clPtr->numHosts++;
        clPtr->resClass |= hPtr->resClass;
        if (hPtr->hTypeNo >= 0) {
            clPtr->typeClass |= ( 1 << hPtr->hTypeNo);
            SET_BIT(hPtr->hTypeNo, clPtr->hostTypeBitMaps);
        }
        if (hPtr->hModelNo >= 0) {
            clPtr->modelClass |= ( 1 << hPtr->hModelNo);
            SET_BIT(hPtr->hModelNo, clPtr->hostModelBitMaps);
        }

        addMapBits (allInfo.nRes, clPtr->resBitMaps, hPtr->resBitMaps);
    }
    for (hPtr=clPtr->clientList; hPtr; hPtr=hPtr->nextPtr) {
        clPtr->numClients++;
        clPtr->resClass |= hPtr->resClass;
        if (hPtr->hTypeNo >= 0) {
            clPtr->typeClass |= ( 1 << hPtr->hTypeNo);
            SET_BIT(hPtr->hTypeNo, clPtr->hostTypeBitMaps);
        }

        if (hPtr->hModelNo >= 0) {
            clPtr->modelClass |= ( 1 << hPtr->hModelNo);
            SET_BIT(hPtr->hModelNo, clPtr->hostModelBitMaps);
        }

        addMapBits (allInfo.nRes, clPtr->resBitMaps,  hPtr->resBitMaps);
    }
    for (i = 0; i < GET_INTNUM(allInfo.nRes); i++)
        for (j = 0; j <INTEGER_BITS; j++)
            if (clPtr->resBitMaps[i] &(1<<j))
                clPtr->nRes++;

    return 0;
}

static void
addMapBits (int num, int *toBitMaps, int *fromMaps)
{
    int j;

    for (j = 0; j < GET_INTNUM(num); j++) {
        toBitMaps[j] = (toBitMaps[j] | fromMaps[j]);
    }
}

int
reCheckClass(void)
{

    if (reCheckClusterClass(myClusterPtr) < 0)
        return -1;
    return 0;

}

static int
configCheckSum(char *file, u_short *checkSum)
{
    static char fname[] = "configCheckSum()";
    unsigned int sum;
    int i,linesum;
    FILE *fp;
    char *line;


    if ((fp = confOpen(file, "r")) == NULL) {
        ls_syslog(LOG_ERR, I18N_CANNOT_OPEN, fname, file);
        return -1;
    }
    sum = 0;
    while ((line = getNextLine_(fp, true)) != NULL ) {
        i=0;
        linesum = 0;
        while(line[i] != '\0') {
            if (line[i] == ' ' || line[i] == '\t' ||
                line[i] == '(' || line[i] == ')'  ||
                line[i] == '[' || line[i] == ']'  ) {
                i++;
                continue;
            }
            linesum += (int) line[i];
            i++;
        }
        for (i=0; i<4; i++) {
            if (sum&01)
                sum = (sum>>1) + 0x8000;
            else
                sum >>= 1;
            sum += linesum & 0xFF;
            sum &= 0xFFFF;
            linesum = linesum >> 8;
        }
    }
    FCLOSEUP(&fp);
    *checkSum = (u_short)sum;
    return 0;
}

static struct admins *
getAdmins (char *line, char *lsfile, int *LineNum, char *secName)
{
    static char fname[] = "getAdmins()";
    static struct admins admins;
    static int first = true;
    int i, numAds = 0;
    char *sp, *word;
    char *forWhat = "for LSF administrator";
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
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
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
        if ((pw = getpwnam(word)) != NULL) {
            if (putInLists (word, &admins, &numAds, forWhat) < 0)
                return(&admins);
        } else if ((unixGrp = getgrnam(word)) != NULL) {
            i = 0;
            while (unixGrp->gr_mem[i] != NULL)
                if (putInLists (unixGrp->gr_mem[i++], &admins, &numAds, forWhat)
                    < 0)
                    return(&admins);

        } else {
            if (putInLists (word, &admins, &numAds, forWhat) < 0)
                return(&admins);
        }
    }
    return (&admins);

}


static int
doubleResTable (char *lsfile, int lineNum)
{
    static char fname[] = "doubleResTable()";
    struct resItem *tempTable;

    if (sizeOfResTable <= 0)
        return -1;

    tempTable = (struct resItem *)realloc (allInfo.resTable,
                                           2* sizeOfResTable*sizeof(struct resItem));
    if (tempTable == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL, fname, "realloc",
                  2 * sizeOfResTable*sizeof (struct resItem));
        return -1;
    }
    allInfo.resTable = tempTable;
    sizeOfResTable *= 2;
    return 0;

}

#define DEFAULT_RETRY_MAX    0
#define DEFAULT_RETRY_INT    30
FILE *
confOpen(char *filename, char *type)
{
    FILE *fp;
    int max, interval;

    if (limParams[LSF_CONF_RETRY_MAX].paramValue)
        max = atoi(limParams[LSF_CONF_RETRY_MAX].paramValue);
    else
        max = DEFAULT_RETRY_MAX;

    if (limParams[LSF_CONF_RETRY_INT].paramValue)
        interval = atoi(limParams[LSF_CONF_RETRY_INT].paramValue);
    else
        interval = DEFAULT_RETRY_INT;

    for (;;) {
        fp = fopen(filename, type);
        if (fp != NULL)
            break;
        if (errno == ENOENT && max >0) {
            int sleeptime;
            sleeptime = interval * mykey() * 1000;
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5369,
                                             "%s: %m Still trying ..."),  /* catgets 5369 */
                      filename);
            millisleep_(sleeptime);
            max--;
            continue;
        }
        break;
    }

    return fp;

}

float
mykey(void)
{
    int sum = 0;
    char *myhostname = ls_getmyhostname();
    int i;
    float key;

    for (i=0; myhostname[i] != 0; i++)
        sum += myhostname[i];

    i = sum % 'z' ;
    if (i < 'A')
        i += 'A';

    key = (float) i / (float) 'Z' ;

    return key;

}

static void
setExtResourcesDefDefault(char *resName)
{

    static char fname[] = "setExtResourcesDefDefault()";


    allInfo.resTable[allInfo.nRes].valueType = LS_STRING;


    allInfo.resTable[allInfo.nRes].flags |= RESF_EXTERNAL;


    allInfo.resTable[allInfo.nRes].interval = 60;
    allInfo.resTable[allInfo.nRes].flags |= RESF_DYNAMIC;


    strncpy(allInfo.resTable[allInfo.nRes].des,
            "Fail to get external defined value, set to default", MAXRESDESLEN);

    ls_syslog(LOG_INFO, I18N(5370,
                             "%s: Fail to get external resource %s definition, set to default"),
              fname, resName);

    return;

}

static int
setExtResourcesDef(char *resName)
{

    static char fname[] = "setExtResourcesDef()";
    struct extResInfo *extResInfoPtr;
    int type;

    if ((extResInfoPtr = getExtResourcesDef(resName)) == NULL)
    {
        setExtResourcesDefDefault(resName);
        return 0;
    }


    if ((type = validType (extResInfoPtr->type)) >= 0)
        allInfo.resTable[allInfo.nRes].valueType = type;
    else {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5371,
                                         "%s: type <%s> for external resource <%s> is not valid"),  /* catgets 5371 */
                  fname, extResInfoPtr->type, resName);
        setExtResourcesDefDefault(resName);
        return 0;
    }


    allInfo.resTable[allInfo.nRes].flags |= RESF_EXTERNAL;


    if (extResInfoPtr->interval != NULL && extResInfoPtr->interval[0] != '\0')
    {
        int interval;
        if ((interval = atoi (extResInfoPtr->interval)) > 0) {
            allInfo.resTable[allInfo.nRes].interval = interval;
            allInfo.resTable[allInfo.nRes].flags |= RESF_DYNAMIC;
        } else
        {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5372,
                                             "%s: interval <%s> for external resource <%s> should be a integer greater than 0"),  /* catgets 5372 */
                      fname, extResInfoPtr->interval, resName);
            setExtResourcesDefDefault(resName);
            return 0;
        }
    }


    if (extResInfoPtr->increasing != NULL
        && extResInfoPtr->increasing[0] != '\0')
    {
        if (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC)
        {
            if (!strcasecmp (extResInfoPtr->increasing, "N"))
                allInfo.resTable[allInfo.nRes].orderType = DECR;
            else
            {
                if (!strcasecmp(extResInfoPtr->increasing, "Y"))
                    allInfo.resTable[allInfo.nRes].orderType = INCR;
                else
                {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5373,
                                                     "%s: increasing <%s> for resource <%s> is not valid"),  /* catgets 5373 */
                              fname, extResInfoPtr->increasing, resName);
                    setExtResourcesDefDefault(resName);
                    return 0;
                }
            }
        } else
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5374,
                                             "%s: increasing <%s> is not used by the resource <%s> with type <%s>; ignoring INCREASING"),    /* catgets 5374 */
                      fname, extResInfoPtr->increasing,
                      resName, extResInfoPtr->type);
    } else
    {
        if (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC)
        {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5375,
                                             "%s: No increasing specified for a numeric resource <%s>"),  /* catgets 5375 */
                      fname, resName);
            setExtResourcesDefDefault(resName);
            return 0;
        }
    }


    strncpy(allInfo.resTable[allInfo.nRes].des,
            extResInfoPtr->des, MAXRESDESLEN);

    if (allInfo.resTable[allInfo.nRes].interval > 0
        && (allInfo.resTable[allInfo.nRes].valueType == LS_NUMERIC))
    {

        if (allInfo.numUsrIndx+NBUILTINDEX >=li_len-1)
        {
            li_len *= 2;
            if (!(li=(struct liStruct *)
                  realloc(li, li_len*sizeof(struct liStruct))))
            {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
                return -1;
            }
        }
        if ((li[NBUILTINDEX + allInfo.numUsrIndx].name =
             putstr_(allInfo.resTable[allInfo.nRes].name)) == NULL)
        {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }

        li[NBUILTINDEX + allInfo.numUsrIndx].increasing =
            (allInfo.resTable[allInfo.nRes].orderType == INCR)?1:0;
        allInfo.numUsrIndx++;
        allInfo.numIndx++;
    }
    return 0;
}

static int
setExtResourcesLoc(char *resName, int resNo)
{

    static char fname[] = "setExtResourcesLoc()";
    char *extResLocPtr;
    static char defaultExtResLoc[]="[default]";
    int lineNum = 0;
    int isDefault;

    extResLocPtr = getExtResourcesLoc(resName);

    if (extResLocPtr == NULL || extResLocPtr[0] == '\0')
    {
        ls_syslog(LOG_INFO, I18N(5379,
                                 "%s: Failed to get LOCATION specified for external resource <%s>; Set to default"),  /* catgets 5379 */
                  fname, resName);
        extResLocPtr = defaultExtResLoc;
    }


    allInfo.resTable[resNo].flags |= RESF_EXTERNAL;


    if (addResourceMap (resName, extResLocPtr, fname,
                        lineNum, &isDefault) < 0)
    {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, fname, "addResourceMap", resName);
        lim_CheckError = WARNING_ERR;
        return -1;
    }

    if (!(isDefault &&
          (allInfo.resTable[resNo].flags & RESF_DYNAMIC) &&
          (allInfo.resTable[resNo].valueType == LS_NUMERIC)))
    {

        allInfo.resTable[resNo].flags &= ~RESF_GLOBAL;
        allInfo.resTable[resNo].flags |= RESF_SHARED;
    }

    return 0;
}




struct extResInfo *
getExtResourcesDef(char *resName)
{

    char fname[] = "getExtResourcesDef";

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5453,
                                     "%s: external resource object is current not support in this platform"), fname); /* catgets 5453 */
    return NULL;
}


char *
getExtResourcesLoc(char *resName)
{

    char fname[] = "getExtResourcesLoc";

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5454,
                                     "%s: external resource object is current not support in this platform"), fname); /* catgets 5454 */
    return NULL;
}


char *
getExtResourcesValDefault(char *resName)
{
    static char defaultVal[]="-";
    return defaultVal;

}


char *
getExtResourcesVal(char *resName)
{

    char fname[] = "getExtResourcesVal";

    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5455,
                                     "%s: external resource object is current not support in this platform"), fname); /* catgets 5455 */
    return(getExtResourcesValDefault(resName));
}

int
initTypeModel(struct hostNode *me)
{
    static char fname[] = "initTypeModel";
    if (me->hTypeNo == DETECTMODELTYPE) {
        me->hTypeNo = typeNameToNo(getHostType());
        if (me->hTypeNo < 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 5456,
                                             "%s: Unknown host type <%s>, using <DEFAULT>"), /* catgets 5456 */
                      fname, getHostType());
            me->hTypeNo = 1;
        }
        if (isMasterCandidate) {
            myClusterPtr->typeClass |= ( 1 << me->hTypeNo);
            SET_BIT(me->hTypeNo, myClusterPtr->hostTypeBitMaps);
        }
    }

    strcpy(me->statInfo.hostType, allInfo.hostTypes[me->hTypeNo]);

    if (me->hModelNo == DETECTMODELTYPE) {
        const char* arch = getHostModel();

        me->hModelNo = archNameToNo (arch);
        if (me->hModelNo < 0) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 5458,
                                             "%s: Unknown host architecture <%s>, using <DEFAULT>"), /* catgets 5458 */ fname, arch);
            me->hModelNo = 1;
        } else {
            if (strcmp(allInfo.hostArchs[me->hModelNo], arch) != 0) {
                if ( logclass & LC_EXEC )  {
                    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd, NL_SETN, 5457,
                                                         "%s: Unknown host architecture <%s>, using best match <%s>, model <%s>"), /* catgets 5457 */
                              fname, arch, allInfo.hostArchs[me->hModelNo],
                              allInfo.hostModels[me->hModelNo]);
                }
            }

        }
        if (isMasterCandidate) {
            myClusterPtr->modelClass |= ( 1 << me->hModelNo);
            SET_BIT(me->hModelNo, myClusterPtr->hostModelBitMaps);
        }
    }
    strcpy(me->statInfo.hostArch, allInfo.hostArchs[me->hModelNo]);


    ++allInfo.modelRefs[me->hModelNo];
    return 0;
}

char *
stripIllegalChars(char *str)
{
    char *c = str;
    char *p = str;

    while (*c) {
        if (isalnum((int)*c))
            *p++ = *c++;
        else
            c++;
    }
    *p = '\0';

    return str;
}

const char*
getHostType()
{
    return HOST_TYPE_STRING;
}

void
slaveOnlyInit(int checkMode, int *kernelPerm)
{

    initLiStruct();
    initResTable();

    if (getenv("RECONFIG_CHECK") == NULL) {
        if (initSock(lim_CheckMode) < 0) {
            lim_Exit("initSock");
        }
    }

    if (chanInit_() < 0) {
        lim_Exit("chanInit_");
    }

    if (slaveOnlyPreConf() < 0) {
        lim_Exit("slaveOnlyPreConf");
    }


    getLastActiveTime();

    limConfReady = false;

    return;
}


int
slaveOnlyPreConf()
{
    static char fname[] = "slaveOnlyPreConf";
    struct hostent *hp;
    struct hostNode *hPtr;
    char * hname;
    int  mindex;
    char rootName[MAXLSFNAMELEN];

    myClusterPtr = (struct clusterNode *)malloc(sizeof(struct clusterNode));
    if (myClusterPtr == (struct clusterNode *)NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }

    memset((char *) myClusterPtr, 0, sizeof(struct clusterNode));
    myClusterPtr->clusterNo = -1;
    myClusterPtr->clName = NULL;

    myClusterPtr->status = CLUST_ACTIVE | CLUST_STAT_UNAVAIL;
    myClusterPtr->masterKnown = false;
    myClusterPtr->masterInactivityCount = 0;
    myClusterPtr->masterPtr = (struct hostNode *)NULL;
    myClusterPtr->prevMasterPtr = (struct hostNode *)NULL;
    myClusterPtr->hostList = (struct hostNode *)NULL;
    myClusterPtr->clientList = (struct hostNode *)NULL;
    myClusterPtr->eLimArgs   = (char *)NULL;
    myClusterPtr->eLimArgv   = (char **)NULL;
    myClusterPtr->currentAddr = 0;
    myClusterPtr->masterName = (char *)NULL;
    myClusterPtr->managerName = (char *)NULL;
    myClusterPtr->resClass = 0;
    myClusterPtr->typeClass = 0;
    myClusterPtr->modelClass = 0;
    myClusterPtr->chanfd = -1;
    myClusterPtr->numIndx = 0;
    myClusterPtr->numUsrIndx = 0;
    myClusterPtr->usrIndxClass = 0;
    myClusterPtr->nRes = 0;
    myClusterPtr->resBitMaps = NULL;
    myClusterPtr->hostTypeBitMaps = NULL;
    myClusterPtr->hostModelBitMaps = NULL;


    nClusAdmins = 0;
    clusAdminIds = NULL;
    clusAdminNames = NULL;
    clusAdminIds = (int *) malloc (sizeof (int));
    if (clusAdminIds == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }
    clusAdminIds[0] = 0;
    nClusAdmins = 1;
    clusAdminNames = calloc(1, sizeof(char *));
    clusAdminNames[0] = strdup(rootName);

    myClusterPtr->nAdmins = nClusAdmins;
    myClusterPtr->adminIds = clusAdminIds;
    myClusterPtr->admins   = clusAdminNames;
    myClusterPtr->managerId   = 0;
    if (clusAdminNames) {
        myClusterPtr->managerName = clusAdminNames[0];
    }

    for (mindex = 0; mindex < numMasterCandidates; mindex++) {

        if ((hPtr = initHostNode()) == NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            return -1;
        }

        hPtr->hostNo = mindex;

        if ((hname = getMasterCandidateNameByNo_(mindex)) == NULL) {
            ls_syslog(LOG_ERR, I18N(5467, "the name of master candidate <%d> is NULL"), mindex); /* catgets 5467 */
            continue;
        }

        if ((hp = (struct hostent *)getHostEntryByName_(hname)) == NULL) {
            ls_syslog(LOG_ERR, I18N(5468, "Can not get host entry for host %s"), hname); /* catgets 5468 */
            continue;
        }

        hPtr->hostName = putstr_(hp->h_name);

        if (saveHostIPAddr(hPtr, hp) < 0 ) {
            ls_syslog(LOG_ERR, I18N(5469, "Can not save internet address of host %s"), hp->h_name); /* catgets 5469 */
            freeHostNodes (hPtr, false);
            continue;
        }

        hPtr->nextPtr = myClusterPtr->hostList;
        myClusterPtr->hostList = hPtr;

    }

    if ((myHostPtr = initHostNode()) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }


    myHostPtr->hostNo = INFINIT_SHORT;
    if ((hname = ls_getmyhostname()) == NULL) {
        ls_syslog(LOG_ERR, I18N(5470, "Can not get my host name")); /* catgets 5470 */
        return -1;
    }
    if ((hp = (struct hostent *)getHostEntryByName_(hname)) == NULL) {
        ls_syslog(LOG_ERR, I18N(5471, "Can not get host entry for host %s"), hname); /* catgets 5471 */
        return -1;
    }

    myHostPtr->hostName = putstr_(hp->h_name);
    myHostPtr->nextPtr = myClusterPtr->hostList;
    myClusterPtr->hostList = myHostPtr;

    myHostPtr->statInfo.nDisks = 0;
    myHostPtr->statInfo.portno = 0;

    if (saveHostIPAddr(myHostPtr, hp) < 0) {
        ls_syslog(LOG_ERR, I18N(5472, "Can not save internet address of host %s"), hp->h_name); /* catgets 5472 */
        freeHostNodes (myHostPtr, false);
        return -1;
    }

    return 0;
}

static int saveHostIPAddr(struct hostNode *hPtr, struct hostent *hp)
{
    static char fname[] = "saveHostIPAddr";
    int i;

    for (hPtr->naddr = 0; hp->h_addr_list &&
             hp->h_addr_list[hPtr->naddr] != NULL; hPtr->naddr++);

    if (hPtr->naddr){
        hPtr->addr = (u_int *)malloc(hPtr->naddr*sizeof(u_int));
        if (!hPtr->addr) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
            freeHostNodes(hPtr, false);
            return -1;
        }
    } else {
        hPtr->addr = 0;
    }

    for (i = 0; i < hPtr->naddr; i++)
        memcpy((char *)&hPtr->addr[i], hp->h_addr_list[i], hp->h_length);

    return 0;

}

static int
adjHostListOrder()
{
    short slaveHostNo;
    int i;
    int found;
    int realNumMasterCandidates;
    char *hname;
    struct hostNode *hPtr, *preHPtr, *firstHPtr;

    if (numMasterCandidates > 0) {

        firstHPtr = myClusterPtr->hostList;
        myClusterPtr->hostList = NULL;
        realNumMasterCandidates = numMasterCandidates;


        for (i = 0; i < numMasterCandidates; i++) {

            if ((hname = getMasterCandidateNameByNo_(i)) == NULL) {

                ls_syslog(LOG_WARNING,I18N(5465, "the master candidate No <%d> isn't a valid host. Ignoring master candidate"), i); /* catgets 5465 */
                realNumMasterCandidates--;

            } else {
                hPtr = firstHPtr;
                preHPtr = firstHPtr;
                found = false;


                while (hPtr != NULL) {
                    if (strcmp(hname, hPtr->hostName) == 0) {
                        found = true;
                        break;
                    }
                    preHPtr = hPtr;
                    hPtr = preHPtr->nextPtr;
                }

                if (found) {


                    if (hPtr == firstHPtr) {
                        firstHPtr = hPtr->nextPtr;
                    } else {
                        preHPtr->nextPtr = hPtr->nextPtr;
                    }


                    hPtr->nextPtr = myClusterPtr->hostList;
                    myClusterPtr->hostList = hPtr;
                    hPtr->hostNo = i;

                } else {

                    ls_syslog(LOG_WARNING, I18N(5464, "the master candidate <%s> isn't in hostList. Ignoring master candidate"), hname); /* catgets 5464 */

                    freeupMasterCandidate_(i);
                    realNumMasterCandidates--;
                    if (realNumMasterCandidates == 0) {
                        ls_syslog(LOG_ERR, I18N(5466,"There is no valid host in LSF_MASTER_LIST")); /* catgets 5466 */
                        return -1;
                    }
                }

            }
        }

        slaveHostNo = numMasterCandidates;
        while ( firstHPtr != NULL ) {


            hPtr = firstHPtr;
            firstHPtr = hPtr->nextPtr;


            hPtr->nextPtr = myClusterPtr->hostList;
            myClusterPtr->hostList = hPtr;
            hPtr->hostNo = slaveHostNo;
            slaveHostNo++;
        }


        for (hPtr = myClusterPtr->hostList, myClusterPtr->hostList = NULL;
             hPtr; hPtr = preHPtr) {

            preHPtr = hPtr->nextPtr;
            hPtr->nextPtr = myClusterPtr->hostList;
            myClusterPtr->hostList = hPtr;
        }

    } else {

        numMasterCandidates = 0;
        for (hPtr = myClusterPtr->hostList; hPtr; hPtr=hPtr->nextPtr) {
            numMasterCandidates++;
        }

    }

    return 0;
}
