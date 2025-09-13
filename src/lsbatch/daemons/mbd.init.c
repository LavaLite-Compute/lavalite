/* $Id: mbd.init.c,v 1.22 2007/08/15 22:18:45 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include <stdlib.h>
#include <math.h>
#include <pwd.h>
#include <grp.h>
#include "mbd.h"

#include "../../lsf/lib/lsi18n.h"
#define NL_SETN		10	

#include <malloc.h>
#include <sys/types.h>

extern int isMasterCrossPlatform(void);
extern void resetStaticSchedVariables(void);
extern void cleanSbdNode(struct jData *);
extern void uDataPtrTbInitialize();

extern struct mbd_func_type mbd_func;

extern int FY_MONTH;

extern void freeUConf(struct userConf *, int);
extern void freeHConf(struct hostConf *, int);
extern void freeQConf(struct queueConf *, int);
extern void freeWorkUser (int);
extern void freeWorkHost (int);
extern void freeWorkQueue (int);

extern void uDataTableFree(UDATA_TABLE_T *uTab); 
extern void cleanCandHosts (struct jData *);

#define setString(string, specString) { \
    FREEUP(string); \
    if(specString != NULL) \
        string = safeSave(specString);}

#define setValue(value, specValue) { \
    if(specValue != INFINIT_INT && specValue != INFINIT_FLOAT) \
        value = specValue;               \
    }

static char defUser = FALSE;
static int numofclusters = 0;
static struct clusterInfo *clusterInfo = NULL;
struct tclLsInfo *tclLsInfo = NULL;
static struct clusterConf clusterConf;
static struct lsConf *paramFileConf = NULL;
static struct lsConf *userFileConf = NULL;
static struct lsConf *hostFileConf = NULL;
static struct lsConf *queueFileConf = NULL;
struct userConf *userConf = NULL;
static struct hostConf *hostConf = NULL;
static struct queueConf *queueConf = NULL;
static struct paramConf *paramConf = NULL;
struct gData *tempUGData[MAX_GROUPS], *tempHGData[MAX_GROUPS];
int nTempUGroups = 0, nTempHGroups = 0;

static char batchName[MAX_LSB_NAME_LEN] = "root"; 
						  

#define PARAM_FILE    0x01
#define USER_FILE     0x02
#define HOST_FILE     0x04
#define QUEUE_FILE    0x08

#define HDATA         1
#define UDATA         2

extern int rusageUpdateRate;
extern int rusageUpdatePercent;

extern void initTab (struct hTab *tabPtr);
static void readParamConf (int);
static int  readHostConf (int);
static void readUserConf (int);
static void readQueueConf (int);

static int isHostAlias (char *grpName);
static int searchAll (char *); 
static void addHost (struct hostInfo *, struct hData *, char *, int);
static void initThresholds (float *, float *);
static void parseGroups(int, struct gData **, char *, char *);
static void addMember(struct gData *, char *, int, char *, 
                                                struct gData *group[], int *);
static struct gData *addGroup (struct gData **, char *, int *);
static struct  qData *initQData (void);
static int isInGrp (char *word, struct gData *);
static struct gData *addUnixGrp (struct group *, char *, char *, 
                                             struct gData *group[], int*);
static void parseAUids (struct qData *, char *);

static void getClusterData(void);
static void setManagers (struct clusterInfo);
static void setAllusers (struct qData *, struct admins *);

static void createTmpGData (struct groupInfoEnt *, int, int,
                              struct gData *tempHGData[], int *);
static void addHostData (int, struct hostInfoEnt *);
static void copyHostInfo (struct hostInfoEnt *, struct hData *);
static void setParams(struct paramConf *);
static void addUData (struct userConf *);
static void setDefaultParams(void);
static void addQData (struct queueConf *, int);
static void copyGroups(int);
static int updCondData (struct lsConf *, int);
static struct condData * initConfData (void);
static void createCondNodes (int, char **, char *, int);
static struct lsConf * getFileConf (char *, int);
static void copyQData (struct qData *, struct qData *);
static void addDefaultHost (void);
static void removeFlags (struct hTab *, int, int);
static int needPollHost (struct hData *, struct hData *);
static int needPollQHost (struct qData *, struct qData *);
static void updHostList (void);
static void updQueueList (void);
static void updUserList (int);

static void addUGDataValues (struct uData *, struct gData *);
static void addUGDataValues1 (struct uData *, struct uData *);
static int parseQHosts(struct qData *, char *);
static void fillClusterConf (struct clusterConf *);
static void fillSharedConf (struct sharedConf *);
static void createDefQueue (void); 
static void freeGrp (struct gData *);
static int validHostSpec (char *);
static void setHPartMemberCreate(struct gData *, LS_BITSET_T *);
static void getMaxCpufactor(void);
static int parseFirstHostErr(int , char *, char *, struct qData *, struct askedHost *, int );

static int  reconfig=FALSE;  

static struct hData **removedHDataArray = NULL; 

static void cleanup(int mbdInitFlags);
static void rebuildCounters();
static int  resetQData(struct qData *qp);
static int  resetJData( );

static void checkAskedHostList(struct jData *jp);
static void checkReqHistory(struct jData *jp);

static void       rebuildUsersSets(void);


int 
minit (int mbdInitFlags)
{
    static char fname[] = "minit";
    struct hData *hPtr;
    int list, i;
    char *realMaster;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    if (mbdInitFlags == FIRST_START) {
        Signal_(SIGTERM, (SIGFUNCTYPE) terminate_handler);
        Signal_(SIGINT,  (SIGFUNCTYPE) terminate_handler);
        Signal_(SIGCHLD, (SIGFUNCTYPE) child_handler);
        Signal_(SIGALRM, SIG_IGN);
        Signal_(SIGHUP,  SIG_IGN);
        Signal_(SIGPIPE, SIG_IGN);
        Signal_(SIGCHLD, (SIGFUNCTYPE) child_handler);
    
        if (!(debug || lsb_CheckMode)) {
	    Signal_(SIGTTOU, SIG_IGN);
	    Signal_(SIGTTIN, SIG_IGN);
	    Signal_(SIGTSTP, SIG_IGN);
        }

	if ((clientList = (struct clientNode *) listCreate("client list")) == NULL) {    
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "listCreate");
            mbdDie(MASTER_FATAL);
        }
        for (list = 0; list < ALLJLIST; list++) {
	    char name[216];
	    sprintf(name, "Job Data List <%d>", list);
	    if ((jDataList[list] = (struct jData *) listCreate(name)) == NULL) {
	        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "listCreate");
                mbdDie(MASTER_FATAL);
            }
	    listAllowObservers((LIST_T *) jDataList[list]);
        }
        initTab(&jobIdHT);
        initTab(&jgrpIdHT);

	
	uDataPtrTb = uDataTableCreate();

	if ((qDataList = (struct qData *) listCreate("Queue List")) == NULL) {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "listCreate");
            mbdDie(MASTER_FATAL);
        }
	
        batchId = getuid();
	if (getLSFUser_(batchName, MAX_LSB_NAME_LEN) != 0) {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "getLSFUser_");
            mbdDie(MASTER_FATAL);
	}
    }
    if (lsb_CheckMode) 
        
        TIMEIT(0, masterHost = ls_getmyhostname(), "minit_ls_getmyhostname");
    if (masterHost == NULL) {
        if (! lsb_CheckMode) 
            mbdDie(MASTER_RESIGN);
        else {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_getmyhostname");
            lsb_CheckError = FATAL_ERR;
            return 0;      
        }
    }

    if (lsb_CheckMode)
	ls_syslog(LOG_INFO, (_i18n_msg_get(ls_catd , NL_SETN, 6237, "%s: Trying to call LIM to get cluster name ...")),    /* catgets 6237 */
		  fname);
    getClusterData();

 
   
    TIMEIT(0, allLsInfo = ls_info(), "minit_ls_info");
    if (allLsInfo == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_info");
	if (lsb_CheckMode) {
	    lsb_CheckError = FATAL_ERR;  
	    return 0;                
	} else
	    mbdDie(MASTER_FATAL);
    }
    else {
        
        for(i=allLsInfo->nModels; i < MAXMODELS; i++)
	    allLsInfo->cpuFactor[i] = 1.0;
    }
	   
    TIMEIT(0, getLsfHostInfo (TRUE), "minit_getLsfHostInfo");
    if (lsfHostInfo == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "minit_getLsfHostInfo");
        if (lsb_CheckMode) {
            lsb_CheckError = FATAL_ERR;
	    return 0;
        } else
            mbdDie(MASTER_FATAL);
    }

    if (logclass & (LC_M_LOG)) {
	ls_syslog(LOG_DEBUG, "Hosts returned by getLsfHostInfo:");
        for (i = 0; i < numLsfHosts; i++) {
	    ls_syslog(LOG_DEBUG, "lsfHostInfo[%d].hostName = %s", 
                i, lsfHostInfo[i].hostName);
        }
    }

    for (i = 0; i < numLsfHosts; i++) {
	if (equalHost_(masterHost, lsfHostInfo[i].hostName)) {
	    if (lsfHostInfo[i].isServer != TRUE) {
		if (!lsb_CheckMode) {
		    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6107,
			"%s: Master host <%s> is not a server"),  /* catgets 6107 */
					       fname,lsfHostInfo[i].hostName);
		    mbdDie(MASTER_RESIGN);
		}
            }
	    break;
	}
    }
 
    
    if (mbdInitFlags == FIRST_START) {
        initParse (allLsInfo);
        tclLsInfo = getTclLsInfo();
        initTcl(tclLsInfo);                
    }
    allUsersSet = setCreate(MAX_GROUPS, getIndexByuData, getuDataByIndex,
			    "All User Set");
    setAllowObservers(allUsersSet);

    if (allUsersSet == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6109,
	    "%s: failed to create all user set: %s"), /* catgets 6109 */
	    fname, setPerror(bitseterrno));
	mbdDie(MASTER_MEM);
    }
	
    uGrpAllSet = setCreate(MAX_GROUPS, getIndexByuData, getuDataByIndex,
			       "Set of user groups containing 'all' users");
    if (uGrpAllSet == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6110,
	    "%s: failed to create the set of  'all;' user groups: %s"), /* catgets 6110 */
	    fname, setPerror(bitseterrno));
	mbdDie(MASTER_MEM);
    }

    uGrpAllAncestorSet = setCreate(MAX_GROUPS, getIndexByuData, 
			       getuDataByIndex, 
			       "Ancestor user groups of 'all' user groups");
    
    if (uGrpAllAncestorSet == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6111,
	    "%s: failed to create the set of ancestor user group of 'all;' user groups: %s"), /* catgets 6111 */
	    fname, setPerror(bitseterrno));
	mbdDie(MASTER_MEM);
    }

    TIMEIT(0, readParamConf(mbdInitFlags), "minit_readParamConf");
    TIMEIT(0, readUserConf(mbdInitFlags), "minit_readUserConf");
    TIMEIT(0, readHostConf(mbdInitFlags), "minit_readHostConf");
    getLsbResourceInfo();

    
    if ((hPtr = getHostData (masterHost)) == NULL
		    || !(hPtr->flags & HOST_UPDATE)) {
        if (lsb_CheckMode) {
	    if ((realMaster = ls_getmastername()) == NULL) {
		ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_getmastername");
		lsb_CheckError = FATAL_ERR;
		return 0;
	    } else if ( realMaster != masterHost) {
		
		ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6113,
		    "%s: Host <%s> is not defined in the Host section of the lsb.hosts file"), fname, masterHost); /* catgets 6113 */
		lsb_CheckError = WARNING_ERR;
		return 0;
	    } else {
		                 
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6114,
		    "%s: Master host <%s> is not defined in the Host section of the lsb.hosts file"), fname, masterHost); /* catgets 6114 */
                lsb_CheckError = FATAL_ERR;
	        return 0;
	    }
        } else
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6114,
		"%s: Master host <%s> is not defined in the Host section of the lsb.hosts file"), fname, masterHost);
            mbdDie(MASTER_FATAL);
    }

    
 
    getLsbHostLoad();

    
    copyGroups (TRUE);  

    
    if (defaultHostSpec != NULL && !validHostSpec (defaultHostSpec)) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6116,
	    "%s: Invalid system defined DEFAULT_HOST_SPEC <%s>; ignored"), fname, defaultHostSpec); /* catgets 6116 */
	FREEUP (defaultHostSpec);
	FREEUP (paramConf->param->defaultHostSpec);
        lsb_CheckError = WARNING_ERR;
    }

    TIMEIT(0, readQueueConf(mbdInitFlags), "minit_readQueueConf");

    
    copyGroups (FALSE);

    updUserList (mbdInitFlags);

    
    if (hReasonTb != NULL) {
	FREEUP (hReasonTb[0]);
	FREEUP (hReasonTb[1]);
	FREEUP (hReasonTb);
    }
    hReasonTb = (int **) my_calloc(2, sizeof(int *), fname);
    hReasonTb[0] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    hReasonTb[1] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    
    for (i = 0; i <= numLsfHosts + 1; i++) {
	hReasonTb[0][i] = 0;
	hReasonTb[1][i] = 0;
    }
    updHostList ();
    updQueueList ();

     
    if (mbdInitFlags == FIRST_START) {
        if (chanInit_() < 0) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "chanInit_");
            mbdDie(MASTER_FATAL);
        }

	
	treeInit();

        if (get_ports() < 0) {
	    if (!lsb_CheckMode)
	        mbdDie(MASTER_FATAL);
            else
                lsb_CheckError = FATAL_ERR;  
        }


        if (getenv("RECONFIG_CHECK") == NULL) {  
            batchSock = init_ServSock(mbd_port);
            if (batchSock < 0) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "init_ServSock");
	        if (! lsb_CheckMode)
	            mbdDie(MASTER_FATAL);
	        else
		    lsb_CheckError = FATAL_ERR;
            }
        }

        if (!(debug || lsb_CheckMode)) {
	    nice(NICE_LEAST);
    	    nice(NICE_MIDDLE);	
	    nice(0);
        }

        
        if (!(debug || lsb_CheckMode)) {
	    if (chdir(LSTMPDIR) < 0) {   
	        lsb_mperr( _i18n_printf(I18N_FUNC_S_FAIL, fname, "chdir", 
		    LSTMPDIR));
	        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, fname, "chdir",
		    LSTMPDIR);
	        if (!lsb_CheckMode)
		    mbdDie(MASTER_FATAL);
	        else
		    lsb_CheckError = FATAL_ERR;
	    }
	}

        if (!lsb_CheckMode) {
            TIMEIT(0,init_log(),"init_log()");
        }
    }


    getMaxCpufactor();

    return 0;
} 

static int 
readHostConf (int mbdInitFlags)
{
    static char fname[] = "readHostConf";
    static char fileName[MAXFILENAMELEN];
 
    if (mbdInitFlags == FIRST_START || mbdInitFlags == RECONFIG_CONF)  {
        sprintf(fileName, "%s/%s/configdir/lsb.hosts",
            daemonParams[LSB_CONFDIR].paramValue, clusterName);
        hostFileConf = getFileConf (fileName, PARAM_FILE);
        if (hostFileConf == NULL && lserrno == LSE_NO_FILE) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6122,
		"%s: File <%s> can not be found, all hosts known by LSF will be used"), fname, fileName); /* catgets 6122 */
            addDefaultHost();
            return(0);
        } 
    } else {
	if (hostFileConf == NULL) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6122,
		"%s: File <%s> can not be found, all hosts known by LSF will be used"), fname, fileName);
            addDefaultHost();
            return(0);
	}
    }
    fillClusterConf (&clusterConf);
    if ((hostConf = lsb_readhost (hostFileConf, allLsInfo, 
                                   CONF_CHECK, &clusterConf)) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "lsb_readhost");
        if (lsb_CheckMode) {
            lsb_CheckError = FATAL_ERR;
            return -1;
        } else
            mbdDie(MASTER_FATAL);
    }
    addHostData (hostConf->numHosts, hostConf->hosts);
    createTmpGData (hostConf->hgroups, hostConf->numHgroups, HOST_GRP, 
                                               tempHGData, &nTempHGroups);
    if (lsberrno == LSBE_CONF_WARNING)
        lsb_CheckError = WARNING_ERR;
    return(0);

    
} 

struct hData *
initHData (struct hData *hData)
{
    int i;

    if (hData == NULL) {
	hData = (struct hData *)my_malloc (sizeof (struct hData), "initHData");
    }

    hData->host = NULL;
    hData->hostEnt.h_name = NULL;
    hData->hostEnt.h_aliases = NULL;

    hData->pollTime = 0;
    hData->acceptTime = 0;
    hData->numDispJobs = 0;
    hData->cpuFactor = 1.0;
    hData->numCPUs  = 1;
    hData->hostType = NULL;
    hData->sbdFail = 0;        
    hData->hStatus  = HOST_STAT_OK;
    hData->uJobLimit = INFINIT_INT;
    hData->uAcct     = (struct hTab *) NULL;
    hData->maxJobs   = INFINIT_INT;
    hData->numJobs   = 0;
    hData->numRUN    = 0;
    hData->numSSUSP  = 0;
    hData->numUSUSP  = 0;
    hData->numRESERVE  = 0;
    hData->mig = INFINIT_INT;
    hData->chkSig = SIG_CHKPNT;
    hData->hostModel = NULL;
    hData->maxMem    = INFINIT_INT;
    hData->maxSwap    = INFINIT_INT;
    hData->maxTmp    = INFINIT_INT;
    hData->nDisks    = INFINIT_INT;
    hData->limStatus = NULL;
    hData->resBitMaps  = NULL;
    hData->lsfLoad  = NULL;
    hData->lsbLoad  = NULL;
    hData->busyStop  = NULL;
    hData->busySched  = NULL;
    hData->loadSched = NULL;
    hData->loadStop =  NULL;
    hData->windows = NULL;
    hData->windEdge = 0;
    for (i = 0; i < 8; i++)
        hData->week[i] = NULL;
    for (i = 0; i < 3; i++)
        hData->msgq[i] = NULL;
    hData->flags = 0;
    hData->numInstances = 0;
    hData->instances = NULL;
    hData->pxySJL = NULL;
    hData->pxyRsvJL = NULL;
    hData->leftRusageMem = INFINIT_LOAD;

    return (hData);

} 

static
void initThresholds (float loadSched[], float loadStop[])
{
    int i;

    for (i = 0; i < allLsInfo->numIndx; i++) {
	if (allLsInfo->resTable[i].orderType == INCR) {
	    loadSched[i] = INFINIT_LOAD;
	    loadStop[i]  = INFINIT_LOAD;
	} else {
	    loadSched[i] = -INFINIT_LOAD;
	    loadStop[i]  = -INFINIT_LOAD;
	}
    }
} 
    
static void 
readUserConf (int mbdInitFlags)
{
    static char fname[] = "readUserConf";
    static char fileName[MAXFILENAMELEN];
    struct sharedConf sharedConf;
    hEnt   *userEnt;

    memset((void *)&sharedConf, 0, sizeof(struct sharedConf));

    if (mbdInitFlags == FIRST_START || 
	mbdInitFlags == RECONFIG_CONF) {

        sprintf(fileName, "%s/%s/configdir/lsb.users",
            daemonParams[LSB_CONFDIR].paramValue, 
		clusterName);

        userFileConf = getFileConf (fileName, USER_FILE);
        if (userFileConf == NULL && lserrno == LSE_NO_FILE) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6125,
		    "%s: File <%s> can not be found, default user will be used"), /* catgets 6125 */
		    fname, 
		    fileName);

	    userConf = (struct userConf *)my_calloc(1, sizeof(struct userConf),
						    fname);
	    goto defaultUser;
        }
    } else if (userFileConf == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6125,
		"%s: File <%s> can not be found, default user will be used"), 
		fname, 
		fileName); 
	
	userConf = (struct userConf *)my_calloc(1, sizeof(struct userConf),
						fname);
	goto defaultUser;
    }   

    
    fillClusterConf (&clusterConf);
    
    
    fillSharedConf( &sharedConf);

    if ((userConf = lsb_readuser_ex(userFileConf, CONF_CHECK,
				    &clusterConf, &sharedConf)) == NULL) {
	ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "lsb_readuser_ex");
	if (lsb_CheckMode) {
	    lsb_CheckError = FATAL_ERR;
	    FREEUP(sharedConf.clusterName);
	    return;
        } else { 
	    mbdDie(MASTER_FATAL);
	}
    }

    createTmpGData(userConf->ugroups, 
		   userConf->numUgroups,
		   USER_GRP,
		   tempUGData, 
		   &nTempUGroups);

    if (lsberrno == LSBE_CONF_WARNING)
        lsb_CheckError = WARNING_ERR;

    addUData(userConf);

defaultUser:

    if (!defUser) {           
        userEnt = h_getEnt_(&uDataList, "default");
        if (userEnt == NULL) { 
            addUserData ("default", 
                         INFINIT_INT,
                         INFINIT_FLOAT, 
                         "readUserConf", 
                         FALSE, 
                         TRUE);
        }
        defUser = TRUE;
    }

    FREEUP(sharedConf.clusterName);
    return;

} 

static void 
readQueueConf (int mbdInitFlags)
{
    static char fname[] = "readQueueConf";
    static char fileName[MAXFILENAMELEN], *cp, *word;
    struct qData *qp = NULL;
    int numDefQue = 0, numQueues = 0;
    struct sharedConf sharedConf;

   if (mbdInitFlags == FIRST_START || mbdInitFlags == RECONFIG_CONF) {
        sprintf(fileName, "%s/%s/configdir/lsb.queues",
            daemonParams[LSB_CONFDIR].paramValue, clusterName);
        queueFileConf = getFileConf (fileName, QUEUE_FILE);
        if (queueFileConf == NULL && lserrno == LSE_NO_FILE) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6128,
		"%s: File <%s> can not be found, default queue will be used"), fname, fileName); /* catgets 6128 */
	    createDefQueue ();
	    return;
        }
    } else if (queueFileConf == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6128,
	    "%s: File <%s> can not be found, default queue will be used"), fname, fileName);
	createDefQueue ();
	return;
    }

    fillSharedConf(&sharedConf);
    if ((queueConf = lsb_readqueue (queueFileConf, 
         allLsInfo, CONF_CHECK| CONF_RETURN_HOSTSPEC, &sharedConf)) == NULL) {
        if (lsberrno == LSBE_CONF_FATAL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "lsb_readqueue");
            if (lsb_CheckMode) {
                lsb_CheckError = FATAL_ERR;
                return;
            } else
                mbdDie(MASTER_FATAL);
        } else {
            lsb_CheckError = WARNING_ERR;
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6131,
		"%s: lsb_readqueue() failded, default queue will be used"), fname); /* catgets 6131 */
	    createDefQueue ();
	    return;
        } 
    }
    FREEUP(sharedConf.clusterName);
    if (lsberrno == LSBE_CONF_WARNING)
	lsb_CheckError = WARNING_ERR;

    
    addQData (queueConf, mbdInitFlags);
   
    
    for (qp = qDataList->forw; qp != qDataList; qp = qp->forw)
	 if (qp->flags & QUEUE_UPDATE)
	     numQueues++;

    if (!numQueues) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6132,
	    "%s: File %s/%s/configdir/lsb.queues: No valid queue defined"), fname, daemonParams[LSB_CONFDIR].paramValue, clusterName); /* catgets 6132 */
	lsb_CheckError = WARNING_ERR;
    }
   
    
    if (numQueues && defaultQueues) {
        cp = defaultQueues;
	while ((word = getNextWord_(&cp))) {
	    if ((qp = getQueueData(word)) == NULL || 
			 !(qp->flags & QUEUE_UPDATE)) {
		ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6133,
	            "%s: File %s/%s/configdir/lsb.params: Invalid queue name <%s> specified by parameter DEFAULT_QUEUE; ignoring <%s>"), fname, daemonParams[LSB_CONFDIR].paramValue, clusterName, word, word); /* catgets 6133 */
		lsb_CheckError = WARNING_ERR;
	    } else {
                qp->qAttrib |= Q_ATTRIB_DEFAULT;
		numDefQue++;
            }
        }
    }

    if (numDefQue) 
        return;
        
    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6134,
	"%s: File %s/%s/configdir/lsb.queues: No valid default queue defined"), fname, daemonParams[LSB_CONFDIR].paramValue, clusterName); /* catgets 6134 */
    lsb_CheckError = WARNING_ERR;

    

    if ((qp = getQueueData("default")) != NULL) {
        ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6135,
	    "%s: Using the default queue <default> provided by lsb.queues configuration file"), fname);  /* catgets 6135 */
        qp->qAttrib |= Q_ATTRIB_DEFAULT;	
        FREEUP (defaultQueues);
        defaultQueues = safeSave ("default");
	qp->flags |= QUEUE_UPDATE;
        return;
    }
    
    createDefQueue ();
    return;

} 
static void
createDefQueue (void) 
{
    static char fname[] = "createDefQueue";
    struct qData *qp;

    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6136,
	"%s: Using the default queue <default> provided by the batch system"), fname); /* catgets 6136 */
    FREEUP (defaultQueues);
    defaultQueues = safeSave ("default");
    qp = initQData();
    qp->qAttrib |= Q_ATTRIB_DEFAULT;
    qp->description        = safeSave ("This is the default queue provided by the batch system. Jobs are scheduled with very loose control.");
    qp->queue              = safeSave ("default");
    
    qp->hostSpec = safeSave (masterHost);
    qp->flags |=  QUEUE_UPDATE;
    qp->numProcessors = numofprocs;
    qp->askedOthPrio = 0;
    qp->acceptIntvl = accept_intvl;
    inQueueList (qp);
    return;

} 

static void
addHost (struct hostInfo *hI, struct hData *hD,  char *filename, 
         int override)
{
    static char fname[] = "addHost";
    static char first = TRUE;
    hEnt *newEnt;
    struct hData *hData;
    int new, i;
    char *word;
    struct hostent *hp;
    
    if (first) {
        h_initTab_(&hDataList, 64);
        first = FALSE;
    }

    newEnt = h_addEnt_(&hDataList, hD->host, &new);
    if (new) {
        hData = initHData(NULL);
	hData->flags |= HOST_NEEDPOLL;  

    } else if (override) {
	
        hData = (struct hData*) newEnt->hData;
        if (needPollHost (hData, hD)) {
            hData->flags |= HOST_NEEDPOLL;
	    hData->pollTime = 0;
        }
	hData->hStatus &= ~HOST_STAT_REMOTE;
        freeHData (hData, FALSE);
    } else
        return;                              
    if (hI != NULL) {
	hData->host = safeSave (hI->hostName);
	hData->cpuFactor = hI->cpuFactor;
	hData->hostType = safeSave (hI->hostType);
        if (hI->maxCpus > 0)
	    hData->numCPUs = hI->maxCpus;
        else {
	    ls_syslog(LOG_DEBUG, "%s: numCPUs <%d> of host <%s> is not greater than 0; assuming as 1", fname, hI->maxCpus, hI->hostName);
            hData->numCPUs = 1;
        }
	hData->hostModel = safeSave (hI->hostModel);
	hData->maxMem    = hI->maxMem;

	
	if (hI->maxMem != 0)
	    hData->leftRusageMem = (float) hI->maxMem;

	hData->maxSwap    = hI->maxSwap;
	hData->maxTmp    = hI->maxTmp;
	hData->nDisks    = hI->nDisks;
	hData->resBitMaps  = getResMaps(hI->nRes, hI->resources);

	
	TIMEIT(0, hp = Gethostbyname_(hD->host), "addHost_gethostbyname");
    } else {
	
        hData->host = safeSave (hD->host);
	hp = NULL;
    }

    if (!hp) {
        hData->hostEnt.h_name = putstr_(hData->host);
        hData->hostEnt.h_aliases = NULL;
    } else {
        int num = 0;
        hData->hostEnt.h_name = putstr_(hp->h_name);
        if (hp->h_aliases) { 
	    for (i = 0; hp->h_aliases[i] != NULL; i++)
	        num++;
	    hData->hostEnt.h_aliases = 
		    (char **) my_calloc(num+1, sizeof (char *), fname);
	    for (i = 0; i < num; i++)
	        hData->hostEnt.h_aliases[i] = putstr_(hp->h_aliases[i]);
        }
    }
    hData->uJobLimit = hD->uJobLimit;
    
    if (hD->maxJobs == -1) {

       hData->maxJobs =  hData->numCPUs;

       
       hData->flags   |= HOST_AUTOCONF_MXJ;

    } else {
       hData->maxJobs   = hD->maxJobs;
    }
    hData->mig = hD->mig;
    hData->chkSig = hD->chkSig;
    
    hData->loadSched = (float *) my_calloc(allLsInfo->numIndx,
                                        sizeof(float), fname);
    hData->loadStop = (float *) my_calloc(allLsInfo->numIndx,
                                       sizeof(float), fname);
    hData->lsfLoad = (float *) my_calloc(allLsInfo->numIndx,
                                       sizeof(float), fname);
    hData->lsbLoad = (float *) my_calloc(allLsInfo->numIndx,
                                       sizeof(float), fname);
    hData->busySched = (int *) my_calloc(GET_INTNUM(allLsInfo->numIndx), 
				       sizeof (int), fname);
    hData->busyStop = (int *) my_calloc(GET_INTNUM(allLsInfo->numIndx), 
				       sizeof (int), fname);
    initThresholds (hData->loadSched, hData->loadStop);
    for (i = 0; i < allLsInfo->numIndx; i++) {
	if (hD->loadSched != NULL && hD->loadSched[i] != INFINIT_FLOAT)
            hData->loadSched[i] = hD->loadSched[i];

	if (hD->loadStop != NULL && hD->loadStop[i] != INFINIT_FLOAT)
            hData->loadStop[i] = hD->loadStop[i];
    }
    for (i = 0; i < GET_INTNUM(allLsInfo->numIndx); i++) {
	hData->busyStop[i] = 0;
	hData->busySched[i] = 0;
    }
    hData->flags |= HOST_UPDATE;

    if (hD->windows) {
	char *sp = hD->windows;
	
	hData->windows = safeSave(hD->windows); 
	*(hData->windows) = '\0';
	while ((word = getNextWord_(&sp)) != NULL) {
	    char *save;
	    save = safeSave(word);
	    if (addWindow(word, hData->week, hD->host) <0) {
		ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6137,
	            "%s: Bad time expression <%s>; ignored."), fname, word); /* catgets 6137 */
                lsb_CheckError = WARNING_ERR;
                freeWeek (hData->week);
		free (save);
		continue;
	    }
	    hData->windEdge = now; 
	    if (*(hData->windows) != '\0')
		strcat (hData->windows, " ");
	    strcat (hData->windows, save);
	    free (save);
	}
    } else { 
	hData->windEdge = 0 ;
	hData->hStatus = HOST_STAT_OK;
    }

    hData->limStatus = (int *) my_malloc
	  ((1 + GET_INTNUM (allLsInfo->numIndx)) *sizeof (int), fname);
    for (i = 0; i < 1 + GET_INTNUM (allLsInfo->numIndx); i++)
	hData->limStatus[i] = 0;

    if (new) {
        newEnt->hData = (int *) hData;
        numofhosts++;

       

        if (daemonParams[LSB_VIRTUAL_SLOT].paramValue) {
           if (!strcasecmp("y", daemonParams[LSB_VIRTUAL_SLOT].paramValue)) {
              if (hData->maxJobs > 0 && hData->maxJobs < INFINIT_INT )
                 hData->numCPUs = hData->maxJobs;
          }
        }

        if (hData->numCPUs > 0)
            numofprocs += hData->numCPUs; 
        else
            numofprocs++;             
    }
    QUEUE_INIT(hData->msgq[MSG_STAT_QUEUED]);
    QUEUE_INIT(hData->msgq[MSG_STAT_SENT]);
    QUEUE_INIT(hData->msgq[MSG_STAT_RCVD]);

    return;
    
} 

void
freeHData (struct hData *hData, char delete)
{
    hEnt *hostEnt = h_getEnt_(&hDataList, hData->host);
    int  i;

    FREEUP (hData->host);
    FREEUP (hData->hostType);
    FREEUP (hData->hostModel);
    FREEUP (hData->hostEnt.h_name);
    if (hData->hostEnt.h_aliases) {
	for (i = 0; hData->hostEnt.h_aliases[i]; i++) 
	    free(hData->hostEnt.h_aliases[i]);
        free (hData->hostEnt.h_aliases);
    }
    FREEUP (hData->loadSched);
    FREEUP (hData->loadStop);
    FREEUP (hData->windows);
    freeWeek (hData->week);

    if (hData->uAcct) {
	if ( hData->uAcct->slotPtr)
	    h_delTab_(hData->uAcct);
	else
	    FREEUP(hData->uAcct);
    }

    FREEUP (hData->lsfLoad);
    FREEUP (hData->lsbLoad);
    FREEUP (hData->resBitMaps);

    QUEUE_DESTROY(hData->msgq[MSG_STAT_QUEUED]);
    QUEUE_DESTROY(hData->msgq[MSG_STAT_SENT]);
    QUEUE_DESTROY(hData->msgq[MSG_STAT_RCVD]);

    FREEUP (hData->instances);
    FREEUP (hData->limStatus);
    FREEUP (hData->busySched);
    FREEUP (hData->busyStop);
    if (delete == TRUE) {  
        if (hostEnt)
            h_delEnt_(&hDataList, hostEnt);
        else
            FREEUP (hData);
    }

} 

static struct gData *
addGroup (struct gData **groups, char *gname, int *ngroups)
{
    
    groups[*ngroups] = (struct gData *) my_malloc
                                       (sizeof (struct gData), "addGroup");
    groups[*ngroups]->group = safeSave(gname);
    h_initTab_(&groups[*ngroups]->memberTab, 0);
    groups[*ngroups]->numGroups = 0;
    *ngroups += 1;
    
    return (groups[*ngroups -1]);
    
}  

static void 
addMember (struct gData *groupPtr, char *word, int grouptype, char *filename, 
                       struct gData *groups[], int *ngroups)
{
    static char fname[] = "addMember";
    struct passwd *pw = NULL;
    const char *officialName;
    char isgrp = FALSE;
    struct gData *subgrpPtr = NULL;
    char name[MAXHOSTNAMELEN];
    
    if (grouptype == USER_GRP) {
        subgrpPtr = getGrpData (tempUGData, word, nTempUGroups);
        if (!subgrpPtr)
            TIMEIT(0, pw = getpwlsfuser_(word), "addMemeber_getpwnam");
      
        isgrp = TRUE;            
        if (pw != NULL) {
            strcpy(name, word);
            isgrp = FALSE;       
        } 
        else if (!subgrpPtr) {
            char *grpSl = NULL;  
            struct group *unixGrp = NULL;
            int lastChar = strlen (word) - 1;
            if (lastChar > 0 && word[lastChar] == '/') {
                grpSl = safeSave (word);
                grpSl[lastChar] = '\0';               
                TIMEIT(0, unixGrp = mygetgrnam(grpSl),"do_Users_getgrnam");
                FREEUP (grpSl);
            }
            else
                TIMEIT(0, unixGrp = mygetgrnam(word), "do_Users_getgrnam");

            if (unixGrp) {
                
 
                subgrpPtr = addUnixGrp (unixGrp, word, filename,
                                       groups, ngroups);
                if (!subgrpPtr) {
                    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6149,
			"%s: No valid users defined in Unix group <%s>; ignoring"), fname, word); /* catgets 6149 */
                    lsb_CheckError = WARNING_ERR;
                    return;
                }

            } else {
		
		strcpy(name, word);
		isgrp = FALSE;       
            }
        }
    }

    if (grouptype == HOST_GRP) {
	if ((subgrpPtr = getGrpData (tempHGData, word, nTempHGroups)) != NULL) {
	    isgrp = TRUE;
	} else {
	    officialName = getHostOfficialByName_(word);
	    if (officialName == NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6151,
		    "%s: Bad host/group name <%s> in group <%s>; ignored"), fname, word, groupPtr->group); /* catgets 6151 */
		lsb_CheckError = WARNING_ERR;
		return;
            } 
            strcpy(name, officialName);
            if (findHost(name) == NULL && numofhosts != 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6152,
		    "%s: Host <%s> is not used by the batch system; ignored"), fname, name); /* catgets 6152 */
		lsb_CheckError = WARNING_ERR;
                return;
            }
            isgrp = FALSE;
        }
    }

    if (isInGrp (word, groupPtr)) {
	if (isgrp)
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6153,
		"%s: Group <%s> is multiply defined in group <%s>; ignored"), fname, word, groupPtr->group); /* catgets 6153 */
        else
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6154,
		"%s: Member <%s> is multiply defined in group <%s>; ignored"), fname, word, groupPtr->group); /* catgets 6154 */
	lsb_CheckError = WARNING_ERR;
        return;
    }

    if (isgrp) {
        groupPtr->gPtr[groupPtr->numGroups] = subgrpPtr;
        groupPtr->numGroups++;
    } else
	h_addEnt_(&groupPtr->memberTab, name, (int *)NULL);

    return;
    
} 

static void
parseAUids (struct qData *qp, char *line) 
{
    static char fname[] = "parseAUids";
    int i, numAds = 0, callFail = FALSE;
    char *sp, *word, *tempStr = NULL, *member;
    struct passwd *pw;
    struct  group *unixGrp;
    struct  gData *uGrp;
    struct admins admins;
    char forWhat[MAXLINELEN];

    sprintf (forWhat, "for queue <%s> administrator", qp->queue);

    sp = line;
    
    while ((word=getNextWord_(&sp)) != NULL)
        numAds++; 
    if (numAds) {
        admins.adminIds = (int *) my_malloc ((numAds) * sizeof(int) , fname);
        admins.adminGIds = (int *) my_malloc ((numAds) * sizeof(int) , fname);
        admins.adminNames = 
                    (char **) my_malloc (numAds * sizeof(char *), fname);
        admins.nAdmins = 0;
    } else {
        
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6155,
	    "%s: No queue's administrators defined; ignored"), fname); /* catgets 6155 */
        lsb_CheckError = WARNING_ERR;
        return;
    }
    sp = line;
    while ((word=getNextWord_(&sp)) != NULL) {
        if (strcmp (word, "all") == 0) {        
            setAllusers (qp, &admins);
            return;
        } else if ((pw = getpwlsfuser_(word))) {
            if (putInLists (word, &admins, &numAds, forWhat) < 0) {
                callFail = TRUE;
                break;
            }
        } else if ((uGrp = getGrpData (tempUGData, word, nTempUGroups)) &&
              (tempStr = getGroupMembers (uGrp, TRUE))) { 
            char *sp = tempStr;
            while ((member = getNextWord_(&sp)) != NULL) {
                if (strcmp (word, "all") == 0) {        
                    setAllusers (qp, &admins);
                    return;
                }
                if (putInLists (member, &admins, &numAds, forWhat) <0) {
                    callFail = TRUE;
                    break;               
                }
            } 
            FREEUP (tempStr);
        } else if ((unixGrp = mygetgrnam(word)) != NULL) { 
            i = 0;
            while (unixGrp->gr_mem[i] != NULL) {
                if (putInLists (unixGrp->gr_mem[i++], &admins, &numAds, forWhat) < 0) {
                    callFail = TRUE;
                    break;
                }
            }
        } else {
	    
            if (putInLists (word, &admins, &numAds, forWhat) < 0) {
                callFail = TRUE;
                break;
            }
        }
        if (callFail == TRUE)
            break;
    }
    if (callFail == TRUE && ! lsb_CheckMode)  
        mbdDie(MASTER_RESIGN);
         
    if (!admins.nAdmins) {         
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6157,
	    "%s: No valid queue's administrators defined in <%s> for queue <%s>; ignored"), fname, line, qp->queue); /* catgets 6157 */
        lsb_CheckError = WARNING_ERR;
        FREEUP(admins.adminIds);
        FREEUP(admins.adminGIds);
        FREEUP(admins.adminNames);
        return;
    }
    qp->nAdmins = admins.nAdmins;
    qp->adminIds = admins.adminIds;
    qp->admins = (char *) my_malloc (admins.nAdmins * MAX_LSB_NAME_LEN, fname);
    qp->admins[0] = '\0';
    for (i = 0; i < admins.nAdmins; i++) {
        strcat(qp->admins,  admins.adminNames[i]);
        strcat(qp->admins,  " "); 
        FREEUP (admins.adminNames[i]);
    } 
    FREEUP (admins.adminNames);
    FREEUP (admins.adminGIds);       
} 

static void
setAllusers (struct qData *qp, struct admins *admins)
{
    int i;

    qp->nAdmins = 1;
    qp->adminIds = (int *) my_malloc (sizeof (int), "setAllusers");
    qp->adminIds[0] = ALL_USERS_ADMINS;
    qp->admins = (char *) my_malloc (MAX_LSB_NAME_LEN, "setAllusers");
    qp->admins[0] = '\0';
    strcat (qp->admins, "all users");

    FREEUP (admins->adminIds);
    FREEUP (admins->adminGIds);
    for (i = 0; i < admins->nAdmins; i++)
        FREEUP (admins->adminNames[i]);
    FREEUP (admins->adminNames);

}      
void
freeQData (struct qData *qp, int delete)
{
    FREEUP (qp->queue);
    FREEUP (qp->description);
    if (qp->uGPtr) {
	freeGrp (qp->uGPtr);
    }

    FREEUP(qp->loadSched);
    FREEUP(qp->loadStop);
    FREEUP (qp->windows);
    FREEUP (qp->windowsD);
    freeWeek (qp->weekR);
    freeWeek (qp->week);
    FREEUP (qp->hostSpec);
    FREEUP (qp->defaultHostSpec);
    if (qp->nAdmins > 0) {
        FREEUP (qp->adminIds);
        FREEUP (qp->admins);
    }
    FREEUP (qp->preCmd);
    FREEUP (qp->postCmd);
    if (qp->requeueEValues) {
	clean_requeue(qp);
    }
    FREEUP (qp->requeueEValues);
    FREEUP (qp->resReq);
    if (qp->resValPtr) {
	FREEUP(qp->resValPtr->rusgBitMaps);
        FREEUP (qp->resValPtr);
    }
    if (qp->uAcct)
        h_delTab_(qp->uAcct);
    if (qp->hAcct)
        h_delTab_(qp->hAcct);
    FREEUP (qp->resumeCond);  
    lsbFreeResVal (&qp->resumeCondVal);
    FREEUP (qp->stopCond);  
    FREEUP (qp->jobStarter); 

    FREEUP (qp->suspendActCmd);
    FREEUP (qp->resumeActCmd);
    FREEUP (qp->terminateActCmd);

    FREEUP (qp->askedPtr);
    FREEUP (qp->hostList);

    if (delete == TRUE) {
        offList ((struct listEntry *)qp);
        numofqueues--;
    }
    if ( qp->reasonTb) {
	FREEUP (qp->reasonTb[0]);
	FREEUP (qp->reasonTb[1]);
	FREEUP (qp->reasonTb);
    }

    if (qp->hostInQueue) {
	setDestroy(qp->hostInQueue);
	qp->hostInQueue = NULL;
    }

    FREEUP (qp);
    return;
} 

static void 
parseGroups (int groupType, struct gData **group, char *line, char *filename)
{
    static char fname[] = "parseGroups";
    char *word, *groupName, *grpSl = NULL;
    int lastChar, i;
    struct group *unixGrp;
    struct gData *gp, *mygp = NULL;
    const char *officialName;
    struct passwd *pw;
 
    mygp = (struct gData *) my_malloc(sizeof (struct gData), fname);
    *group = mygp;
    mygp->group = "";        
    h_initTab_(&mygp->memberTab, 16);
    mygp->numGroups = 0;
    for (i = 0; i <MAX_GROUPS; i++)
	 mygp->gPtr[i] = NULL;

   if (groupType == USER_GRP)
       groupName = "User/User";
   else
       groupName = "Host/Host";

    while ((word = getNextWord_(&line)) != NULL) {
        if (isInGrp (word, mygp)) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6164,
		"%s/%s: Member <%s> is multiply defined; ignored"), filename, fname, word); /* catgets 6164 */
            lsb_CheckError = WARNING_ERR;
            continue;
        }
	if (groupType == USER_GRP) {
	    TIMEIT(0, pw = getpwlsfuser_(word), "parseGroups_getpwnam");
	    if (pw != NULL) {
		h_addEnt_(&mygp->memberTab, word, 0);
		continue;
	    } 
            FREEUP (grpSl);
	    lastChar = strlen (word) - 1;
	    if (lastChar > 0 && word[lastChar] == '/') {
		grpSl = safeSave (word);
		grpSl[lastChar] = '\0';       
	    }

            
	    gp = getGrpData (tempUGData, word, nTempUGroups);
	    if (gp != NULL) {                 
		mygp->gPtr[mygp->numGroups] = gp;
	    	mygp->numGroups++;
		continue;
            }
            
            if (grpSl) {
                TIMEIT(0, unixGrp = mygetgrnam(grpSl), "parseGroups_getgrnam");
                                       	      
                grpSl[lastChar] = '/';        
            } else
		TIMEIT(0, unixGrp = mygetgrnam(word), "parseGroups_getgrnam");
            if (unixGrp != NULL) {
                
                gp = addUnixGrp(unixGrp, grpSl, filename, tempUGData, &nTempUGroups);
                if (gp == NULL) {
                    ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6165,
			"%s/%s: No valid users in Unix group <%s>; ignoring"), filename, fname, word); /* catgets 6165 */
		    lsb_CheckError = WARNING_ERR;
                    continue;
                }
                mygp->gPtr[mygp->numGroups] = gp;
                mygp->numGroups++;
            } else {
		
		h_addEnt_(&mygp->memberTab, word, 0);
		continue;
            }
	    continue;
	}
        else {           

	    
	    gp = getGrpData (tempHGData, word, nTempHGroups);
	    if (gp != NULL) {
		mygp->gPtr[mygp->numGroups] = gp;
		mygp->numGroups++;
		continue;
	    }

	    
	    officialName = getHostOfficialByName_(word);
	    if (officialName != NULL) {
		word = (char*)officialName;       
		if (numofhosts) {
		    if (!h_getEnt_(&hDataList, word)) {
			ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6170,
			    "%s/%s: Host <%s> is not used by the batch system; ignored"), filename, fname, word); /* catgets 6170 */
			lsb_CheckError = WARNING_ERR;
			continue;
		    }
		}
                if (isInGrp (word, mygp)) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6171,
			"%s/%s: Host name <%s> is multiply defined; ignored"), filename, fname, word); /* catgets 6171 */
		    lsb_CheckError = WARNING_ERR;
                    continue;
                }
		h_addEnt_(&mygp->memberTab, word, 0);
		continue;
	    } else {
		ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6172,
			"%s/%s: Unknown host or host group <%s>; ignored"), filename, fname, word); /* catgets 6172 */
		lsb_CheckError = WARNING_ERR;
		continue;
	    }	    
	} 
    }
    FREEUP (grpSl);
    
    return;
} 


static void 
readParamConf (int mbdInitFlags)
{
    static char fileName[MAXFILENAMELEN];
    static char fname[] = "readParamFile";

    setDefaultParams();

    if (mbdInitFlags == FIRST_START || mbdInitFlags == RECONFIG_CONF) { 

        sprintf(fileName, "%s/%s/configdir/lsb.params",
            daemonParams[LSB_CONFDIR].paramValue, clusterName);
        paramFileConf = getFileConf (fileName, PARAM_FILE);
        if (paramFileConf == NULL && lserrno == LSE_NO_FILE) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6173,"\
%s: File <%s> can not be found, default parameters will be used"), 
		      fname, fileName);  /* catgets 6173 */
            return;
        }
    }

    if ((paramConf = lsb_readparam(paramFileConf)) == NULL) {
	if (lsberrno == LSBE_CONF_FATAL) {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "lsb_readparam");
            if (lsb_CheckMode) {
                lsb_CheckError = FATAL_ERR;
                return;
            } else
                mbdDie(MASTER_FATAL);
        } else {
            if (lsberrno == LSBE_CONF_WARNING)
                lsb_CheckError = WARNING_ERR;
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "lsb_readparam");
	    return;
        }
    }

    if (lsberrno == LSBE_CONF_WARNING)
        lsb_CheckError = WARNING_ERR;

    setParams(paramConf);

    return;
} 

int
my_atoi (char *arg, int upBound, int botBound)
{
    int num;

    if (!isint_ (arg)) {
        return (INFINIT_INT);
    }
    num = atoi (arg);
    if (num >= upBound || num <= botBound) 
        return (INFINIT_INT);
    return (num);

} 

static struct qData *
initQData (void)
{
    static char fname[] = "initQData";
    struct qData *qp;
    int    i;

    qp = (struct qData *) my_malloc(sizeof (struct qData), "initQData");
    qp->queue = NULL;
    qp->queueId = 0;
    qp->description = NULL;
    qp->numProcessors      = 0;
    qp->priority           = DEF_PRIO;
    qp->nice               = DEF_NICE;
    qp->uJobLimit          = INFINIT_INT;
    qp->uAcct = (struct hTab *) NULL;
    qp->pJobLimit          = INFINIT_FLOAT;
    qp->hAcct = (struct hTab *) NULL;
    qp->windows            = NULL;
    qp->windowsD           = NULL;
    qp->windEdge           = 0;                   
    qp->runWinCloseTime    = 0;
    qp->numHUnAvail = 0 ;
    for (i = 0; i < 8; i++) {
        qp->weekR[i] = NULL;
        qp->week[i] = NULL;
    }
    for (i = 0; i < LSF_RLIM_NLIMITS; i++) {
        qp->rLimits[i] = -1;
	qp->defLimits[i] = -1;
    }
    qp->hostSpec           = NULL;
    qp->defaultHostSpec    = NULL;
    qp->qAttrib		   = 0;
    qp->qStatus = (QUEUE_STAT_OPEN | QUEUE_STAT_ACTIVE | QUEUE_STAT_RUN);
    qp->maxJobs           = INFINIT_INT;
    qp->numJobs           = 0;
    qp->numPEND            = 0;
    qp->numRUN             = 0;
    qp->numSSUSP           = 0;
    qp->numUSUSP           = 0;
    qp->mig                = INFINIT_INT;
    qp->schedDelay         = INFINIT_INT;
    qp->acceptIntvl        = INFINIT_INT;

    qp->loadSched = (float *) my_calloc(allLsInfo->numIndx, 
                                        sizeof(float), fname);
    qp->loadStop = (float *) my_calloc(allLsInfo->numIndx, 
                                        sizeof(float), fname);
    
    initThresholds (qp->loadSched, qp->loadStop);
    qp->procLimit = -1;
    qp->minProcLimit = -1;
    qp->defProcLimit = -1;
    qp->adminIds = NULL;
    qp->admins   = NULL;
    qp->nAdmins  = 0;
    qp->preCmd   = NULL;
    qp->postCmd = NULL;
    qp->requeueEValues = NULL;
    qp->requeEStruct = NULL;
    qp->hJobLimit = INFINIT_INT;
    qp->resValPtr = NULL;
    qp->resReq = NULL;
    qp->reasonTb = NULL;
    qp->numReasons = 0;
    qp->numUsable = 0;
    qp->numRESERVE = 0;
    qp->slotHoldTime = 0;
    qp->resumeCond = NULL;
    qp->resumeCondVal = NULL;
    qp->stopCond = NULL;
    qp->jobStarter = NULL;    

    qp->suspendActCmd = NULL;
    qp->resumeActCmd = NULL;
    qp->terminateActCmd = NULL;
    for (i=0; i< LSB_SIG_NUM; i++)
        qp->sigMap[i] = 0;

    qp->flags = 0;    
    qp->uGPtr    = NULL;
    qp->hostList = NULL;
    qp->hostInQueue = NULL;      
    qp->askedPtr = NULL;    
    qp->numAskedPtr = 0;    
    qp->askedOthPrio = -1;    

    qp->reasonTb = (int **) my_calloc(2, sizeof(int *), fname);
    qp->reasonTb[0] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    qp->reasonTb[1] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    for (i = 0; i <= numLsfHosts+1; i++) {
	qp->reasonTb[0][i] = 0;
	qp->reasonTb[1][i] = 0;
    }
    qp->schedStage = 0;
    for (i = 0; i <= PJL; i++) {
	qp->firstJob[i] = NULL;
	qp->lastJob[i] = NULL;
    }

    qp->chkpntPeriod = -1;
    qp->chkpntDir    = NULL;
    return (qp);

} 

static int
searchAll (char *word)
{
    char *sp, *cp;
    
    if (!word)
        return (FALSE);
    cp = word;                                  
    while ((sp = getNextWord_(&cp))) {
        if (strcmp (sp, "all") == 0)
            return (TRUE);
    }
    return (FALSE);

} 

void
freeKeyVal (struct keymap keylist[])
{
    int cc;
    for (cc = 0; keylist[cc].key != NULL; cc++) {
        if (keylist[cc].val != NULL) {
            free(keylist[cc].val);
            keylist[cc].val = NULL;
        }
    }
} 

static int
isInGrp (char *word, struct gData *gp)
{
    int i;

    if (h_getEnt_(&gp->memberTab, word))
        return TRUE;

    for (i = 0; i < gp->numGroups; i++) {
        if (strcmp (gp->gPtr[i]->group, word) == 0)
            return TRUE;
        if (isInGrp(word, gp->gPtr[i]))
            return TRUE;
    }
    return FALSE;

} 

struct qData *
lostFoundQueue(void)
{
    static struct qData *qp;

    if (qp != NULL)
	return qp;

    qp = initQData();
    qp->description = safeSave ("This queue is created by the system to hold the jobs that were in the queues being removed from the configuration by the LSF Administrator.  Jobs in this queue will not be started unless they are switched to other queues.");
    qp->queue = safeSave (LOST_AND_FOUND);
    qp->uJobLimit = 0;
    qp->pJobLimit = 0.0;
    qp->acceptIntvl = DEF_ACCEPT_INTVL;
    qp->qStatus = (!QUEUE_STAT_OPEN | !QUEUE_STAT_ACTIVE);
    qp->uGPtr = (struct gData *) my_malloc
                                (sizeof (struct gData), "lostFoundQueue");
    qp->uGPtr->group = "";
    h_initTab_(&qp->uGPtr->memberTab, 16);
    qp->uGPtr->numGroups = 0;
    h_addEnt_(&qp->uGPtr->memberTab, "nobody", 0);
    

    qp->hostSpec = safeSave (masterHost);
    inQueueList (qp);
    checkQWindow();

    return qp;

} 

struct hData *
lostFoundHost (void)
{
    static char fname[] = "lostFoundHost()";
    struct hData *lost;
    struct hData hp;

    initHData (&hp);
    hp.host = LOST_AND_FOUND;            
    hp.cpuFactor = 1.0;
    hp.uJobLimit = 0;
    hp.maxJobs =  0;

    addHost (NULL, &hp, "lostFoundHost", FALSE);
    checkHWindow ();
    lost = getHostData (LOST_AND_FOUND);
    lost->hStatus = HOST_STAT_DISABLED;
    if (lost == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, 
	    fname, "getHostData",
	    LOST_AND_FOUND);
        if (! lsb_CheckMode)
            mbdDie(MASTER_RESIGN);
        else
            lsb_CheckError = FATAL_ERR;
    }
    return (lost);

} 

void
queueHostsPF(struct qData *qp, int *allPoll)
{
    int j;
    struct hData *hData;
 
    
    qp->numProcessors = 0;

    if (qp->hostList == NULL ||  qp->askedOthPrio > 0) {
        
        qp->numProcessors = numofprocs;

        if ((qp->flags & QUEUE_NEEDPOLL) && *allPoll == FALSE) {
            sTab hashSearchPtr;   
            hEnt *hashEntryPtr;
            hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
            while (hashEntryPtr) {
                hData = (struct hData *)hashEntryPtr->hData;
                hashEntryPtr = h_nextEnt_(&hashSearchPtr);
                if (hData->hStatus & HOST_STAT_REMOTE)
                    continue;
                if (strcmp (hData->host, LOST_AND_FOUND) == 0)
                    continue;
                hData->flags |= HOST_NEEDPOLL;
		hData->pollTime = 0;
            }
            *allPoll = TRUE;
        }

    } else {                                    
        for (j = 0; j < qp->numAskedPtr; j++) {
            if (qp->askedPtr[j].hData== NULL)
                continue;
            qp->numProcessors += qp->askedPtr[j].hData->numCPUs;
	    hData = qp->askedPtr[j].hData;
            if ((qp->flags & QUEUE_NEEDPOLL) && *allPoll == FALSE) {
                hData->flags |= HOST_NEEDPOLL;
		hData->pollTime = 0;
	    }
        }
    }

}                        

static struct gData *
addUnixGrp (struct group *unixGrp, char *grpName, 
            char *filename, struct gData *groups[], int *ngroups)
{
    int i = -1;
    struct gData *gp;
    struct passwd *pw = NULL;
    char *grpTemp;

    if (grpName == NULL) {
	grpTemp = unixGrp->gr_name;
    } else {
	grpTemp = grpName;
    }
    gp = addGroup (groups, grpTemp, ngroups);
    while (unixGrp->gr_mem[++i] != NULL)  {
	if ((pw = getpwlsfuser_(unixGrp->gr_mem[i])))
            addMember (gp, unixGrp->gr_mem[i], USER_GRP, filename, 
		       groups, ngroups);
        else {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd, NL_SETN, 6179,
                "%s: Unknown user <%s> in group <%s>"), /* catgets 6179 */
		    filename, unixGrp->gr_mem[i], grpTemp);
	    lsb_CheckError = WARNING_ERR;
        }
    }
    if (gp->memberTab.numEnts == 0 && gp->numGroups == 0) {
	freeGrp (gp);
        *ngroups -= 1;
        return (NULL);
    }
    return (gp);

} 

static void
getClusterData(void)
{
    static char fname[]="getClusterData";
    int i, num;

    TIMEIT(0, clusterName = ls_getclustername(), "minit_ls_getclustername");
    if (clusterName == NULL) { 
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_getclustername");
        if (! lsb_CheckMode)
            mbdDie(MASTER_RESIGN);
        else {
            lsb_CheckError = FATAL_ERR;
            return ;
        }
    }
    if (debug) {                        
        FREEUP (managerIds);
	if (lsbManagers)
	    FREEUP (lsbManagers[0]);
        FREEUP (lsbManagers);
        nManagers = 1;
        lsbManagers = (char **)my_malloc(sizeof (char *), fname); 
        lsbManagers[0] = (char *)my_malloc(MAX_LSB_NAME_LEN, fname);
	if (getLSFUser_(lsbManagers[0], MAX_LSB_NAME_LEN) == 0) {
	    managerIds = (int *) my_malloc (sizeof (int), fname);
	    if (getOSUid_(lsbManagers[0], &managerIds[0]) != 0) {
		ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "getOSUid_");
		if (! lsb_CheckMode)
		    mbdDie(MASTER_RESIGN);
		else {
		    lsb_CheckError = FATAL_ERR;
		    return ;
		}
		managerIds[0] = -1;
	    }
	} else {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "getLSFUser_");
            mbdDie(MASTER_RESIGN);
	}
        if (!lsb_CheckMode)
            ls_syslog(LOG_NOTICE,I18N(6256, 
	    "%s: The LSF administrator is the invoker in debug mode"),fname); /*catgets 6256 */
        lsbManager = lsbManagers[0];
        managerId  = managerIds[0];
    } 
    num = 0;
    clusterInfo = ls_clusterinfo(NULL, &num, NULL, 0, 0);
    if (clusterInfo == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_clusterinfo");
        if (!lsb_CheckMode)
            mbdDie(MASTER_RESIGN);
        else {
            lsb_CheckError = FATAL_ERR;
            return ;      
        }
    } else {
        if (!debug) {
            if (nManagers > 0) {
                for (i = 0; i < nManagers; i++)
                    FREEUP (lsbManagers[i]);
                FREEUP (lsbManagers);
                FREEUP (managerIds);
            }
        }

        for(i=0; i < num; i++) {
            if (!debug && 
                (strcmp(clusterName, clusterInfo[i].clusterName) == 0)) 
                setManagers (clusterInfo[i]);
        }
        
        if (!nManagers && !debug) {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6182,
		"%s: Local cluster %s not returned by LIM"),fname, clusterName); /* catgets 6182 */
            mbdDie(MASTER_FATAL);
        }
        numofclusters = num;
    }
} 

static void
setManagers (struct clusterInfo clusterInfo)
{
    static char fname[]="setManagers";
    struct passwd   *pw;
    int i, numValid = 0, gid, temNum = 0, tempId;
    char *sp;
    char managerIdStr[MAX_LSB_NAME_LEN], managerStr[MAX_LSB_NAME_LEN];
 
    temNum = (clusterInfo.nAdmins) ? clusterInfo.nAdmins : 1; 
    managerIds = (int *) my_malloc (sizeof (int) * temNum, fname);
    lsbManagers = (char **) my_malloc (sizeof (char *) * temNum, fname);

    
    for (i = 0; i < temNum; i++) {
        sp = (clusterInfo.nAdmins) ? 
                             clusterInfo.admins[i] : clusterInfo.managerName;
        tempId = (clusterInfo.nAdmins) ? 
                             clusterInfo.adminIds[i] : clusterInfo.managerId;
 
	lsbManagers[i] = safeSave (sp); 

	if ((pw = getpwlsfuser_ (sp)) != NULL) {
	    managerIds[i] = pw->pw_uid;
	    if (numValid == 0) {
		gid = pw->pw_gid;

		
		lsbManager = lsbManagers[i];
		managerId  = managerIds[i];
	    }
            numValid++;
	} else {
	    if (logclass & LC_AUTH) {
		ls_syslog(LOG_DEBUG,
		    "%s: Non recognize LSF administrator name <%s> and userId <%d> detected. It may be a user from other realm", fname, sp, tempId);
	    }
	    managerIds[i] = -1;
	}    
    }
    if (numValid == 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6184,
	    "%s: No valid LSF administrators"), fname); /* catgets 6184 */
        mbdDie(MASTER_FATAL);
    }
    nManagers = temNum;
    
    setgid(gid);

    
    if (getenv("LSB_MANAGERID") == NULL) {
        sprintf (managerIdStr, "%d", managerId);
        putEnv ("LSB_MANAGERID", managerIdStr);    
    }

    if (getenv("LSB_MANAGER") == NULL) {
        sprintf (managerStr, "%s", lsbManager);
        putEnv ("LSB_MANAGER", managerStr);    
    }
} 

static void 
setParams(struct paramConf *paramConf)
{
    static char fname[] = "setParams";
    struct parameterInfo *params;

    if (paramConf == NULL || paramConf->param == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6185,
	    "%s: paramConf or param in paramConf is NULL; default  parameters will be used"), fname); /* catgets 6185 */
        return;
    }
    params = paramConf->param;
    setString(defaultQueues, params->defaultQueues); 
    setString(defaultHostSpec, params->defaultHostSpec); 
    setString(lsfDefaultProject, params->defaultProject); 
    
    setValue(msleeptime, params->mbatchdInterval);
    setValue(sbdSleepTime, params->sbatchdInterval);
    setValue(accept_intvl, params->jobAcceptInterval);
    setValue(max_retry, params->maxDispRetries);
    setValue(max_sbdFail, params->maxSbdRetries);
    setValue(clean_period, params->cleanPeriod);
    setValue(maxjobnum, params->maxNumJobs);
    setValue(pgSuspIdleT, params->pgSuspendIt);
    setValue(retryIntvl, params->retryIntvl);
    setValue(rusageUpdateRate, params->rusageUpdateRate);
    setValue(rusageUpdatePercent, params->rusageUpdatePercent);
    setValue(condCheckTime, params->condCheckTime);
    setValue(maxJobArraySize, params->maxJobArraySize);
    setValue(jobRunTimes, params->jobRunTimes);
    setValue(jobDepLastSub, params->jobDepLastSub);

    setValue(maxSbdConnections, params->maxSbdConnections);

    setValue(maxSchedStay, params->maxSchedStay);
    setValue(freshPeriod, params->freshPeriod);
    setValue(jobTerminateInterval, params->jobTerminateInterval);
    setString(pjobSpoolDir, params->pjobSpoolDir);
    
    setValue(maxUserPriority, params->maxUserPriority);
    setValue(jobPriorityValue, params->jobPriorityValue);
    setValue(jobPriorityTime, params->jobPriorityTime);
    setJobPriUpdIntvl( );  
    
    
    setValue(sharedResourceUpdFactor, params->sharedResourceUpdFactor);

    setValue(scheRawLoad, params->scheRawLoad);
 
    setValue(preExecDelay, params->preExecDelay);
    setValue(slotResourceReserve, params->slotResourceReserve);
	
    setValue(maxJobId, params->maxJobId);

    setValue(maxAcctArchiveNum, params->maxAcctArchiveNum);
    setValue(acctArchiveInDays, params->acctArchiveInDays);
    setValue(acctArchiveInSize, params->acctArchiveInSize);

} 

static void 
addUData (struct userConf *userConf) 
{
    static char fname[] = "addUData";
    struct userInfoEnt *uPtr;
    int i;

    removeFlags(&uDataList, USER_UPDATE, UDATA); 
    for (i = 0; i < userConf->numUsers; i++) {
       uPtr = &(userConf->users[i]);
       addUserData(uPtr->user, uPtr->maxJobs, uPtr->procJobLimit, 
		   fname, TRUE, TRUE);
       if (strcmp ("default", uPtr->user) == 0)
           defUser = TRUE;
    }
} 

static void 
createTmpGData (struct groupInfoEnt *groups, 
		int num, 
		int groupType, 
		struct gData *tempGData[], 
		int *nTempGroups) 
{
    static char fname[] = "createTmpGData";
    struct groupInfoEnt *gPtr;
    char *HUgroups, *sp, *wp;
    int i;
    struct gData *grpPtr;

    if (groupType == USER_GRP)
        HUgroups = "usergroup";
    else
        HUgroups = "hostgroup";

    for (i = 0; i < num; i++) {

        gPtr = &(groups[i]);

        if (groupType == HOST_GRP && gPtr && gPtr->group 
            && isHostAlias(gPtr->group)) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6186,
	        "%s: Host group name <%s> conflicts with host name; ignoring"), fname, gPtr->group); /* catgets 6186 */
            lsb_CheckError = WARNING_ERR;
            continue;
        }

        if (getGrpData (tempGData, gPtr->group, *nTempGroups)) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6187,
	        "%s: Group <%s> is multiply defined in %s; ignoring"), fname, gPtr->group, HUgroups); /* catgets 6187 */
            lsb_CheckError = WARNING_ERR;
            continue;
        }

        grpPtr = addGroup(tempGData, gPtr->group, nTempGroups);
        sp = gPtr->memberList;

        while (strcmp(gPtr->memberList, "all") != 0 &&
	       (wp = getNextWord_(&sp)) != NULL) { 
            addMember(grpPtr, 
		      wp, 
		      groupType, 
		      fname, 
		      tempGData, 
		      nTempGroups);
	}

        if (grpPtr->memberTab.numEnts == 0 && 
	    grpPtr->numGroups == 0 &&
            strcmp(gPtr->memberList, "all") != 0) {
            ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6188,
	        "%s: No valid member in group <%s>; ignoring the group"), fname, grpPtr->group); /* catgets 6188 */
            lsb_CheckError = WARNING_ERR;
	    freeGrp (grpPtr);
            *nTempGroups -= 1;
        }
    }

}  

static int
isHostAlias (char *grpName)
{
    int i;
    hEnt *hashEntryPtr;
    sTab hashSearchPtr;
    struct hData *hData;

    hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
    while (hashEntryPtr) {
        hData = (struct hData *) hashEntryPtr->hData;
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
        if (hData->hostEnt.h_aliases) {
            for (i = 0; hData->hostEnt.h_aliases[i]; i++) {
                if (equalHost_(grpName, hData->hostEnt.h_aliases[i]))
                    return TRUE;
            }
        }
    }
    return FALSE;

} 

static void 
addHostData (int numHosts, struct hostInfoEnt *hosts)
{
    static char fname[] = "addHostData";
    int i, j;
    struct hData       hPtr;

    
    removeFlags (&hDataList, HOST_UPDATE | HOST_AUTOCONF_MXJ, HDATA);
    if (numHosts == 0 || hosts == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6189,
	    "%s: No hosts specified in lsb.hosts; all hosts known by LSF will be used"), fname); /* catgets 6189 */
        addDefaultHost();
        return;
    }
    for (i = 0; i < numHosts; i++) {
        for (j = 0; j < numLsfHosts; j++) {
            if (equalHost_(hosts[i].host, lsfHostInfo[j].hostName)) 
                break;
        }
        if (j == numLsfHosts) {   
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6190,
	        "%s: Host <%s> is not used by the batch system; ignored"), fname, hosts[i].host); /* catgets 6190 */
            continue;
        }
	if (lsfHostInfo[j].isServer != TRUE) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6191,
	        "%s: Host <%s> is not a server; ignoring"), fname, hosts[i].host); /* catgets 6191 */
            continue;
        }
        
	if (!isValidHost_(lsfHostInfo[j].hostName)) {
	    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6242,
		"%s: Host <%s> is not a valid host; ignoring"), fname, hosts[i].host); /* catgets 6242 */
	    continue;
	}

       

        initHData (&hPtr);

       
        copyHostInfo(&hostConf->hosts[i], &hPtr);

       
        addHost(&lsfHostInfo[j], &hPtr, fname, TRUE);
   }

}     
    
static void 
copyHostInfo (struct hostInfoEnt *from, struct hData *to)
{
    to->host = from->host;
    to->uJobLimit = from->userJobLimit;
    to->maxJobs   = from->maxJobs;
    if (from->chkSig != INFINIT_INT)
        to->chkSig    = from->chkSig;
    if (from->mig == INFINIT_INT)
	to->mig       = from->mig;
    else
        to->mig       = from->mig * 60;
    to->loadSched = from->loadSched;
    to->loadStop = from->loadStop;
    to->windows = from->windows;
    
} 
 
static void
setDefaultParams(void)
{

    FREEUP (defaultQueues);
    FREEUP (defaultHostSpec);
    FREEUP (lsfDefaultProject);
    FREEUP (pjobSpoolDir);

    msleeptime = DEF_MSLEEPTIME;
    sbdSleepTime = DEF_SSLEEPTIME;
    accept_intvl = DEF_ACCEPT_INTVL;
    max_retry = DEF_MAX_RETRY;
    max_sbdFail = DEF_MAXSBD_FAIL;
    preemPeriod = DEF_PREEM_PERIOD;
    clean_period = DEF_CLEAN_PERIOD;
    maxjobnum = DEF_MAX_JOB_NUM;
    pgSuspIdleT = DEF_PG_SUSP_IT;
    retryIntvl = DEF_RETRY_INTVL;
    rusageUpdateRate = DEF_RUSAGE_UPDATE_RATE;
    rusageUpdatePercent = DEF_RUSAGE_UPDATE_PERCENT;
    condCheckTime = DEF_COND_CHECK_TIME;
    maxSbdConnections = DEF_MAX_SBD_CONNS;
    maxSchedStay = DEF_SCHED_STAY;
    freshPeriod = DEF_FRESH_PERIOD;
    maxJobArraySize = DEF_JOB_ARRAY_SIZE;
    jobTerminateInterval = DEF_JTERMINATE_INTERVAL;
    jobRunTimes = INFINIT_INT;
   jobDepLastSub = 0;
    scheRawLoad = 0;
    maxUserPriority = -1;
    jobPriorityValue = -1;
    jobPriorityTime = -1;
    preExecDelay = DEF_PRE_EXEC_DELAY;
    slotResourceReserve = FALSE;
    maxJobId = DEF_MAX_JOBID;
    maxAcctArchiveNum = -1;
    acctArchiveInDays = -1;
    acctArchiveInSize = -1;
} 

static void
addQData (struct queueConf *queueConf, int mbdInitFlags )
{
    static char fname[]="addQData";
    int i, badqueue, j;
    struct qData  *qp, *oldQp;
    struct queueInfoEnt *queue;
    char *sp, *word;
    int queueIndex = 0;

    
    for (qp = qDataList->forw; qp != qDataList; qp = qp->forw) {
        qp->flags &= ~QUEUE_UPDATE;
    }
 
    if (queueConf == NULL || queueConf->numQueues <= 0 
					|| queueConf->queues == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6194,
	    "%s: No valid queue in queueConf structure"), /* catgets 6194 */
	    fname);
        lsb_CheckError = WARNING_ERR;
        return;
    }

    
    for (i = 0; i < queueConf->numQueues; i++) {
        queue = &(queueConf->queues[i]);
        qp = initQData();

	
        qp->queue = safeSave (queue->queue);
        if (queue->description)
            qp->description = safeSave(queue->description);
        else
            qp->description = safeSave("No description provided.");

        
        setValue(qp->priority, queue->priority); 

        
        
        if (queue->userList) {
            parseGroups(USER_GRP, &qp->uGPtr, queue->userList, fname);
        }

        
        if (queue->hostList) {
            if (strcmp(queue->hostList, "none") == 0) {
		
		qp->hostList = safeSave(queue->hostList);
	    } else {
		if (searchAll (queue->hostList) == FALSE) {
                    if ( parseQHosts (qp, queue->hostList) != 0 ) {
                       qp->hostList = safeSave(queue->hostList);
                    }
                }
            }
        }

        badqueue = FALSE;
        if (qp->uGPtr)
            if (qp->uGPtr->memberTab.numEnts == 0 &&
                           qp->uGPtr->numGroups == 0) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6196,
	            "%s: No valid value for key USERS in the queue <%s>; ignoring the queue"), /* catgets 6196 */
	            fname, qp->queue);
                lsb_CheckError = WARNING_ERR;
                badqueue = TRUE;
            }
	if (   qp->hostList != NULL 
	    && strcmp(qp->hostList, "none")
	    && qp->numAskedPtr <= 0 
	    && qp->askedOthPrio <= 0) 
        {
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6197,
	        "%s: No valid value for key HOSTS in the queue <%s>; ignoring the queue"), /* catgets 6197 */
	        fname, qp->queue);
            lsb_CheckError = WARNING_ERR;
            badqueue = TRUE;
        }
        
        if (badqueue) {
	    freeQData (qp, FALSE);
	    continue;
        }

	
	++queueIndex;  
	qp->queueId = queueIndex;

        
        if ((oldQp = getQueueData(qp->queue)) == NULL) { 
            inQueueList (qp);
	    qp->flags |= QUEUE_UPDATE;
        } else {
            copyQData (qp, oldQp); 
	    oldQp->flags |= QUEUE_UPDATE;
	    FREEUP (qp->queue);
            FREEUP(qp->reasonTb[0]);
            FREEUP(qp->reasonTb[1]);
            FREEUP(qp->reasonTb);
	    FREEUP (qp);
	    
	    if ( mbdInitFlags == RECONFIG_CONF || mbdInitFlags == WINDOW_CONF ) {
		int j;
		FREEUP(oldQp->reasonTb[0]);
		FREEUP(oldQp->reasonTb[1]);
                oldQp->reasonTb[0] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
                oldQp->reasonTb[1] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
 
                for(j = 0; j < numLsfHosts+2; j++) {
                    oldQp->reasonTb[0][j] = 0;
                    oldQp->reasonTb[1][j] = 0;
                }
	    }
        }
    } 

    
    for (i = 0; i < queueConf->numQueues; i++) {
        queue = &(queueConf->queues[i]);
        qp = getQueueData(queue->queue);

        
	if (queue->nice != INFINIT_SHORT)
	    qp->nice = queue->nice;

        
        setValue(qp->uJobLimit, queue->userJobLimit);

        
        setValue(qp->pJobLimit, queue->procJobLimit);

        
        setValue(qp->maxJobs, queue->maxJobs);

        
        setValue(qp->hJobLimit, queue->hostJobLimit);

        
        setValue(qp->procLimit, queue->procLimit);
	setValue(qp->minProcLimit, queue->minProcLimit);
	setValue(qp->defProcLimit, queue->defProcLimit);

        
	
	qp->windEdge = 0 ;

	

        if (queue->windows != NULL) { 
            if (1){       
                
                qp->windows = safeSave(queue->windows);
                *(qp->windows) = '\0';
                sp = queue->windows;
                while ((word = getNextWord_(&sp)) != NULL) {
                    char *save;
                    save = safeSave(word);
                    if (addWindow(word, qp->weekR, qp->queue) < 0) {
                        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6198,
	                    "%s: Bad time expression <%s>/<%s> for run windows of queue <%s>; ignored"), /* catgets 6198 */
	                    fname, queue->windows, save, qp->queue);
                        lsb_CheckError = WARNING_ERR;
                        freeWeek (qp->weekR);
                        free (save);
                        continue;
                    }
                    qp->windEdge = now;       
                    if (*(qp->windows) != '\0')
                        strcat (qp->windows, " ");
                    strcat (qp->windows, save);
                    free (save);
                }

		if (logclass & (LC_TRACE | LC_SCHED)) {
		    int i, j;
		    windows_t *wp;
		    ls_syslog(LOG_DEBUG2, "%s: queue <%s> run window <%s>",
			      fname, qp->queue, qp->windows);
		    for (i = 0; i < 8; i++) {
			for (wp = qp->weekR[i], j = 0; wp != NULL;
			     wp = wp->nextwind, j++) {
			    ls_syslog(LOG_DEBUG2, "%s: queue <%s> weekR[%d] window #%d opentime <%f> closetime <%f>", fname, qp->queue, i, j, wp->opentime, wp->closetime);
			}
		    }
		}
            }
        } 

        
        if (queue->windowsD) {
            
            qp->windowsD = safeSave(queue->windowsD);
            *(qp->windowsD) = '\0';
            sp = queue->windowsD;
            while ((word = getNextWord_(&sp)) != NULL) {
                char *save;
                save = safeSave(word);
                if (addWindow(word, qp->week, qp->queue) <0) {
                    ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6200,
	                "%s: Bad time expression <%s>/<%s> for dispatch windows of queue <%s>; ignored"), /* catgets 6200 */
	                fname, queue->windowsD, save, qp->queue);
                    lsb_CheckError = WARNING_ERR;
                    freeWeek (qp->week);
                    free (save);
                    continue;
                }
                qp->windEdge = now;       
                if (*(qp->windowsD) != '\0')
                    strcat (qp->windowsD, " ");
    
                strcat (qp->windowsD, save);
                free (save);
            }
        }

 	if (!qp->windows && (qp->qStatus & QUEUE_STAT_RUNWIN_CLOSE)) {
	    qp->qStatus &= ~QUEUE_STAT_RUNWIN_CLOSE;
	}

 	if (!qp->windows && !qp->windowsD 
		&& !(qp->qStatus & QUEUE_STAT_RUN)) {
	    qp->qStatus |= QUEUE_STAT_RUN;
	}

        
        if (queue->defaultHostSpec) {
            float *cpuFactor;
            if ((cpuFactor =
		 getModelFactor (queue->defaultHostSpec)) == NULL &&
                (cpuFactor =
		 getHostFactor (queue->defaultHostSpec)) == NULL) {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6201,
	            "%s: Invalid value <%s> for %s in queue <%s>; ignored"), /* catgets 6201 */
	            fname, 
		    queue->defaultHostSpec, 
		    "DEFAULT_HOST_SPEC", 
		    qp->queue);
                lsb_CheckError = WARNING_ERR;
            }
            if (cpuFactor != NULL)
                qp->defaultHostSpec = safeSave (queue->defaultHostSpec);
        } 
       
	
	if (queue->hostSpec)
	    qp->hostSpec = safeSave (queue->hostSpec);
        
        for (j = 0; j < LSF_RLIM_NLIMITS; j++) {
            if (queue->rLimits[j] == INFINIT_INT)  
                qp->rLimits[j] = -1;
            else
                qp->rLimits[j] = queue->rLimits[j];

	    if (queue->defLimits[j] == INFINIT_INT) 
	        qp->defLimits[j] = -1;
	    else
	        qp->defLimits[j] = queue->defLimits[j];
        }

         
        qp->qAttrib = (qp->qAttrib | queue->qAttrib);

        
        if (queue->mig != INFINIT_INT) {
            setValue(qp->mig, queue->mig);      
            qp->mig *= 60; 
        }

        
        if (queue->schedDelay != INFINIT_INT)
            qp->schedDelay = queue->schedDelay;         

        
        if (queue->acceptIntvl != INFINIT_INT)
            qp->acceptIntvl = queue->acceptIntvl;         
        else
            qp->acceptIntvl = accept_intvl;         

        
        if (queue->admins) 
            parseAUids (qp, queue->admins);

        
        if (queue->preCmd)
            qp->preCmd = safeSave (queue->preCmd);

        
        if (queue->postCmd)
            qp->postCmd = safeSave (queue->postCmd);

        
        if (queue->requeueEValues) {
            qp->requeueEValues = safeSave (queue->requeueEValues);
	    
	    if (qp->requeEStruct) {
		clean_requeue(qp);
	    }
            requeueEParse (&qp->requeEStruct, queue->requeueEValues, &j);
               
	}

        
        if (queue->resReq) {
            if ((qp->resValPtr = 
                 checkResReq (queue->resReq, 
			      USE_LOCAL | PARSE_XOR | CHK_TCL_SYNTAX)) == NULL)
            {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6203,
		    "%s: RES_REQ <%s> for the queue <%s> is invalid; ignoring"), fname, queue->resReq, qp->queue); /* catgets 6203 */
                lsb_CheckError = WARNING_ERR;
            } else
                qp->resReq = safeSave (queue->resReq);
        }
        
        if (queue->slotHoldTime != INFINIT_INT) 
            qp->slotHoldTime = queue->slotHoldTime * msleeptime;  
 
        
        if (queue->resumeCond) {
	    struct resVal *resValPtr;
            if ((resValPtr = checkResReq (queue->resumeCond,
					  USE_LOCAL | CHK_TCL_SYNTAX)) == NULL)
            {
                 ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6204,
                 "%s: RESUME_COND <%s> for the queue <%s> is invalid; ignoring"), fname, queue->resumeCond, qp->queue); /* catgets 6204 */
                 lsb_CheckError = WARNING_ERR;
	    } else {
                qp->resumeCondVal = resValPtr;
                qp->resumeCond = safeSave (queue->resumeCond);
            }
        }

        
        if (queue->stopCond) {
	    struct resVal *resValPtr;
            if ((resValPtr =
                 checkResReq (queue->stopCond, 
			      USE_LOCAL | CHK_TCL_SYNTAX)) == NULL)
           {
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6205,
                    "%s: STOP_COND <%s> for the queue <%s> is invalid; ignoring"), fname, queue->stopCond, qp->queue); /* catgets 6205 */
                lsb_CheckError = WARNING_ERR;
            } else {
                lsbFreeResVal (&resValPtr);
                qp->stopCond = safeSave (queue->stopCond);
            }
        }

        
        if (queue->jobStarter) 
            qp->jobStarter = safeSave (queue->jobStarter);

        
        if (queue->suspendActCmd != NULL)
            qp->suspendActCmd = safeSave (queue->suspendActCmd);

        if (queue->resumeActCmd != NULL)
            qp->resumeActCmd = safeSave (queue->resumeActCmd);

        if (queue->terminateActCmd != NULL)
            qp->terminateActCmd = safeSave (queue->terminateActCmd);

        for (j=0; j<LSB_SIG_NUM; j++)
            qp->sigMap[j] = queue->sigMap[j];

        
        initThresholds (qp->loadSched, qp->loadStop); 
        for (j = 0; j < queue->nIdx; j++) {
            if (queue->loadSched[j] != INFINIT_FLOAT)
                qp->loadSched[j] = queue->loadSched[j];
            if (queue->loadStop[j] != INFINIT_FLOAT)
                qp->loadStop[j] = queue->loadStop[j];
        }

	setValue(qp->chkpntPeriod, queue->chkpntPeriod);
	if (queue->chkpntDir) {
	    if (qp->chkpntDir)
	        FREEUP(qp->chkpntDir);
	    qp->chkpntDir = safeSave (queue->chkpntDir);
        }

        if (queue->qAttrib & Q_ATTRIB_CHKPNT)
            qp->qAttrib |= Q_ATTRIB_CHKPNT;
 
       if (queue->qAttrib & Q_ATTRIB_RERUNNABLE)
            qp->qAttrib |= Q_ATTRIB_RERUNNABLE;

        if (qp->qAttrib & Q_ATTRIB_BACKFILL)
            qAttributes |= Q_ATTRIB_BACKFILL;
    } 

    return;
} 

static void
copyGroups(int copyHGroups)
{
    int i;
    if (copyHGroups == FALSE) {
	
	hEnt  *userEnt;
	struct uData *user;
        for (i = 0; i < numofugroups; i++) {
	    user = NULL;
	    if (usergroups[i] == NULL)
		continue;

	    if (reconfig) {
		userEnt = h_getEnt_(&uDataList, usergroups[i]->group);
		if (userEnt) {
		    user = (struct uData *) userEnt->hData;
	        } 
            } 
	    freeGrp(usergroups[i]);

	    if ( reconfig && user ) {
		user->gData = NULL;
		user->flags &= ~USER_GROUP;
	    } 
        }
        for (i = 0; i < nTempUGroups; i++) {
            usergroups[i] = tempUGData[i];
            tempUGData[i] = NULL;
	    if ( reconfig ) {
		userEnt = h_getEnt_(&uDataList, usergroups[i]->group);
		if (userEnt) {
		    user = (struct uData *) userEnt->hData;
		    if ( user ) { 
			user->flags |= USER_GROUP; 
			user->gData = usergroups[i];
		    }
		}
	    }
        }
        numofugroups = nTempUGroups; 
        nTempUGroups = 0; 
        return;
    }
    for (i = 0; i < numofhgroups; i++) {
	if (hostgroups[i] == NULL)
	    continue;
        freeGrp (hostgroups[i]);
    }
 
    for (i = 0; i < nTempHGroups; i++)
        hostgroups[i] = tempHGData[i];
    numofhgroups = nTempHGroups;
    nTempHGroups = 0;

}     

static void 
createCondNodes (int numConds, char **conds, char *fileName, int flags)
{
    static char fname[] = "createCondNodes";
    static int first = TRUE;
    int i, errcode = 0, jFlags =0, new;
    struct lsfAuth auth;
    struct condData *condNode;
    struct hEnt *newEnt; 

    if (numConds <= 0 || conds == NULL)
	return;

    if (first == TRUE) {
	h_initTab_(&condDataList, 20);
	first = FALSE;
    }

    auth.uid = batchId;
    strncpy(auth.lsfUserName, batchName, MAXLSFNAMELEN);
    auth.lsfUserName[MAXLSFNAMELEN-1] = '\0';

    for (i = 0; i < numConds; i++) {
        if (conds[i] == NULL || conds[i][0] == '\0')
            continue;
        if (h_getEnt_(&condDataList, conds[i])) {
	    ls_syslog(LOG_DEBUG, "%s: Condition <%s> in file <%s> is multiply specified;retaining old definition", fname, conds[i], fileName);
	    continue;
        }
        condNode = initConfData(); 
	condNode->name = safeSave (conds[i]);
	condNode->flags = flags;
        if ((condNode->rootNode = parseDepCond (conds[i], &auth, &errcode, 
		       NULL, &jFlags, 0)) == NULL) 
	{
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6207,
		"%s: Bad condition <%s> in file <%s> :%M; ignored"), fname, conds[i], fileName); /* catgets 6207 */
	    lsb_CheckError = WARNING_ERR;
	}
        newEnt = h_addEnt_(&condDataList, condNode->name, &new);
	newEnt->hData = (int *) condNode;
        continue;
    }
}  

int
updAllConfCond (void)
{
    int needReadAgain = FALSE;

    if (hostFileConf != NULL && updCondData (hostFileConf, HOST_FILE) == TRUE)
	needReadAgain = TRUE;
    if (userFileConf != NULL && updCondData (userFileConf, USER_FILE) == TRUE)
	needReadAgain = TRUE;
    if (queueFileConf != NULL && updCondData (queueFileConf, QUEUE_FILE) == TRUE)
	needReadAgain = TRUE;
    if (paramFileConf != NULL && updCondData (paramFileConf, PARAM_FILE) == TRUE)
	needReadAgain = TRUE;

    return (needReadAgain);

} 

static int 
updCondData (struct lsConf *conf, int fileType)
{
    static char fname[] = "updCondData";
    hEnt *hashEntryPtr;
    struct condData *condition;
    int status, i, needReadAgain = FALSE;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG2, "%s: Entering this routine...", fname);

    if (conf == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6208,
	    "%s: NULL Conf pointer for fileType <%x>"), fname, fileType); /* catgets 6208 */
        return FALSE;
    }
    if (conf->numConds <= 0 || conf->conds == NULL)
        return FALSE;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG2, "%s: numConds <%d>, fileType <%x>", fname, conf->numConds, fileType);

    for (i = 0; i < conf->numConds; i++) {
        if ((hashEntryPtr = h_getEnt_(&condDataList, conf->conds[i])) == NULL) 
            continue;
        condition = (struct condData *) hashEntryPtr->hData;   
        if (condition->rootNode == NULL) 
            continue;
        status = evalDepCond (condition->rootNode, NULL);
        if (status == DP_TRUE)
            status = TRUE;
        else
            status = FALSE;
	if (status == TRUE) {
	    conf->values[i] = 1;
	} else {
	    conf->values[i] = 0;
	}
        if (status != condition->status) {
            condition->lastStatus = condition->status;
	    condition->status = status;
            condition->lastTime = now;
            needReadAgain = TRUE;
        }
	if (logclass & LC_TRACE)
	    ls_syslog(LOG_DEBUG3, "%s: Condition name <%s> for %dth condition status <%d> lastStatus <%d>", fname, conf->conds[i], i+1, condition->status, condition->lastStatus);
   }
   return (needReadAgain);

}           
      
static struct condData *
initConfData (void)
{
    struct condData *cData;

    cData = (struct condData *)my_malloc (sizeof (struct condData), "initConfData");
    cData->name = NULL;
    cData->status = FALSE;
    cData->lastStatus = FALSE;
    cData->lastTime = now;
    cData->flags = 0;
    cData->rootNode = NULL;
    return (cData);

} 

static struct lsConf *
getFileConf (char *fileName, int fileType)
{
    static char fname[] = "getFileConf";
    struct lsConf *confPtr = NULL;

    confPtr = ls_getconf (fileName);
    if (confPtr == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "ls_getconf");
        if (lserrno != LSE_NO_FILE) {
            if (lsb_CheckMode) {
                lsb_CheckError = FATAL_ERR;
                return NULL;
            } else
                mbdDie(MASTER_FATAL);
        } 
            
    }
    if (confPtr != NULL) {
        createCondNodes (confPtr->numConds, confPtr->conds, fileName, fileType);
        updCondData (confPtr, fileType);
    }
    return (confPtr);

} 

static void 
copyQData (struct qData *fromQp, struct qData *toQp)
{
    int i;

#define copyString(string, newPointer) { \
    FREEUP(string); \
    string = newPointer;}

    if (needPollQHost(fromQp, toQp))
        toQp->flags |= QUEUE_NEEDPOLL;

    
    copyString(toQp->description, fromQp->description);
    copyString(toQp->resReq, fromQp->resReq);
    copyString(toQp->preCmd, fromQp->preCmd);
    copyString(toQp->postCmd, fromQp->postCmd);
    copyString(toQp->requeueEValues, fromQp->requeueEValues);
    copyString(toQp->windowsD, fromQp->windowsD);
    copyString(toQp->windows, fromQp->windows);
    copyString(toQp->hostSpec, fromQp->hostSpec);
    copyString(toQp->defaultHostSpec, fromQp->defaultHostSpec);
    copyString(toQp->resumeCond, fromQp->resumeCond);
    copyString(toQp->stopCond, fromQp->stopCond);
    copyString(toQp->jobStarter, fromQp->jobStarter);


    copyString(toQp->suspendActCmd, fromQp->suspendActCmd);
    copyString(toQp->resumeActCmd, fromQp->resumeActCmd);
    copyString(toQp->terminateActCmd, fromQp->terminateActCmd); 

    copyString(toQp->hostList, fromQp->hostList);

    
    toQp->numProcessors = fromQp->numProcessors;
    toQp->mig = fromQp->mig;
    toQp->priority = fromQp->priority;
    toQp->nice = fromQp->nice;
    toQp->procLimit = fromQp->procLimit;
    toQp->minProcLimit = fromQp->minProcLimit;
    toQp->defProcLimit = fromQp->defProcLimit;
    toQp->hJobLimit = fromQp->hJobLimit;
    toQp->pJobLimit = fromQp->pJobLimit;
    toQp->uJobLimit = fromQp->uJobLimit;
    toQp->maxJobs = fromQp->maxJobs;
    toQp->slotHoldTime = fromQp->slotHoldTime;
    toQp->askedOthPrio = fromQp->askedOthPrio;
    toQp->numAskedPtr = fromQp->numAskedPtr;
    toQp->queueId = fromQp->queueId;

    
    if (toQp->uGPtr) {
	freeGrp (toQp->uGPtr);
    }
    toQp->uGPtr = fromQp->uGPtr;

    FREEUP (toQp->askedPtr);
    toQp->askedPtr = fromQp->askedPtr;

    FREEUP(toQp->loadSched);
    toQp->loadSched = fromQp->loadSched;

    FREEUP(toQp->loadStop);
    toQp->loadStop = fromQp->loadStop;

    lsbFreeResVal (&toQp->resValPtr);
    toQp->resValPtr = fromQp->resValPtr;

    lsbFreeResVal (&toQp->resumeCondVal);
    toQp->resumeCondVal = fromQp->resumeCondVal;

    for (i = 0; i < LSF_RLIM_NLIMITS; i++) { 
        toQp->rLimits[i] = fromQp->rLimits[i];
	toQp->defLimits[i] = fromQp->defLimits[i];
    }

    if (toQp->nAdmins > 0) {
        FREEUP (toQp->adminIds);
        FREEUP (toQp->admins);
    }
    toQp->nAdmins = fromQp->nAdmins;
    toQp->adminIds = fromQp->adminIds;
    toQp->admins = fromQp->admins;

    if (toQp->weekR)
        freeWeek (toQp->weekR);

    if (toQp->week)
        freeWeek (toQp->week);
    for (i = 0; i < 8; i++) {
        toQp->weekR[i] = fromQp->weekR[i];
        toQp->week[i] = fromQp->week[i];
    } 
   
    toQp->qAttrib = fromQp->qAttrib;

    
}  

static void 
addDefaultHost (void)
{
    int i;
    struct hData hData;
    
    for (i =0; i < numLsfHosts; i++) {
	if (lsfHostInfo[i].isServer != TRUE) 
	    continue;
        initHData(&hData);
	hData.host = lsfHostInfo[i].hostName;
        addHost (&lsfHostInfo[i], &hData, "addDefaultHost", TRUE);
    }
} 

static void 
removeFlags (struct hTab *dataList, int flags, int listType)
{
    static char fname[] = "removeFlags()";
    sTab sTabPtr;
    hEnt *hEntPtr;
    struct hData  *hData;
    struct uData  *uData;

    hEntPtr = h_firstEnt_(dataList, &sTabPtr);
    while (hEntPtr) {
        switch (listType) {
            case HDATA:
                hData = (struct hData *) hEntPtr->hData;
                hData->flags &= ~(flags);
                break;
            case UDATA:
                uData = (struct uData  *)hEntPtr->hData;
                uData->flags &= ~(flags);
                break;
            default:
                ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6210,
		    "%s: Bad data list type <%d>"), /* catgets 6210 */
		    fname, 
		    listType);
                return;
         }
         hEntPtr = h_nextEnt_(&sTabPtr);
    }

} 

static int 
needPollHost (struct hData *oldHp, struct hData *newHp)
{
    int i;

    if (oldHp->mig != newHp->mig) 
        return TRUE;
 
    if ((oldHp->windows == NULL && newHp->windows != NULL) ||
	 (oldHp->windows != NULL && newHp->windows == NULL) ||
	  (oldHp->windows != NULL && newHp->windows != NULL &&
			  strcmp (oldHp->windows, newHp->windows)))
         return TRUE;
	
    if ((oldHp->loadSched == NULL && newHp->loadSched != NULL) ||
	 (oldHp->loadSched != NULL && newHp->loadSched == NULL) ||
         (oldHp->loadStop != NULL && newHp->loadStop == NULL) ||
         (oldHp->loadStop == NULL && newHp->loadStop != NULL))
        return TRUE;

    for (i = 0; i < allLsInfo->numIndx; i++) {
        if ((oldHp->loadSched != NULL && newHp->loadSched != NULL &&
		 oldHp->loadSched[i] != newHp->loadSched[i]) ||
             (oldHp->loadStop != NULL && newHp->loadStop != NULL &&
		    oldHp->loadStop[i] != newHp->loadStop[i]))
            return TRUE;
    }
    return FALSE;

} 

static int 
needPollQHost (struct qData *newQp, struct qData *oldQp)
{

    if ((newQp->resumeCond == NULL && oldQp->resumeCond != NULL) ||
	  (newQp->resumeCond != NULL && oldQp->resumeCond == NULL) ||
	    (newQp->resumeCond != NULL && oldQp->resumeCond != NULL &&
	      strcmp (newQp->resumeCond, oldQp->resumeCond)))
        return (TRUE);
    if ((newQp->stopCond == NULL && oldQp->stopCond != NULL) ||
	  (newQp->stopCond != NULL && oldQp->stopCond == NULL) ||
	    (newQp->stopCond != NULL && oldQp->stopCond != NULL &&
	   strcmp (newQp->stopCond, oldQp->stopCond)))
        return (TRUE);

    if ((newQp->windows == NULL && oldQp->windows != NULL) ||
	    (newQp->windows != NULL && oldQp->windows == NULL) ||
	      (newQp->windows != NULL && oldQp->windows != NULL &&
	  strcmp (newQp->windows, oldQp->windows)))
        return (TRUE);

    if (newQp->nice != oldQp->nice || 
         newQp->mig != oldQp->mig)  
        return TRUE;

    if (newQp->rLimits[LSF_RLIMIT_RUN] != oldQp->rLimits[LSF_RLIMIT_RUN] ||
        newQp->rLimits[LSF_RLIMIT_CPU] != oldQp->rLimits[LSF_RLIMIT_CPU])
        return TRUE;

    return FALSE;

} 


static void 
updHostList (void)
{
    struct hData *hPtr, *lost_and_found = NULL;
    sTab hashSearchPtr;
    hEnt *hashEntryPtr, *curEntry;
    int i, index=0;

    
    FREEUP(removedHDataArray);
    removedHDataArray = (struct hData **) my_calloc(numLsfHosts + 2,
              sizeof(struct hData *), "updHostList");
 
    
    FREEUP (hDataPtrTb);
    hDataPtrTb = (struct hData **) my_calloc(numLsfHosts + 2,
              sizeof(struct hData *), "updHostList");
    for (i = 0; i < numLsfHosts + 1; i++)
	hDataPtrTb[i] = 0;

    
    if ((lost_and_found = getHostData(LOST_AND_FOUND)) == NULL) 
        lost_and_found = lostFoundHost();
    numofhosts = 0;
    numofprocs = 0;
    hashEntryPtr = h_firstEnt_(&hDataList, &hashSearchPtr);
    
    while (hashEntryPtr) {
        hPtr = (struct hData *)hashEntryPtr->hData;
	curEntry = hashEntryPtr;
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);

        if (hPtr == lost_and_found)
            continue;

	if (hPtr->hStatus & HOST_STAT_REMOTE)
	    continue;
       
        if(hPtr->flags & HOST_UPDATE){
           hPtr->flags &=~HOST_UPDATE;
        }else if (reconfig) { 
	    
	    removedHDataArray[index] = hPtr;
	    index++;
	    h_rmEnt_(&hDataList, curEntry);
	    continue;
	} else {
	    freeHData(hPtr,TRUE);
	    continue;
        }

	numofhosts++;
	hPtr->hostId = numofhosts;
        hDataPtrTb[numofhosts] = hPtr;

        

        if (daemonParams[LSB_VIRTUAL_SLOT].paramValue) {
           if (!strcasecmp("y", daemonParams[LSB_VIRTUAL_SLOT].paramValue)) {
              if (hPtr->maxJobs > 0 && hPtr->maxJobs < INFINIT_INT)
                hPtr->numCPUs = hPtr->maxJobs;
          }
        }
        if (hPtr->numCPUs > 0)
            numofprocs += hPtr->numCPUs; 
        else
            numofprocs++;             

	
	if (hPtr->numJobs >= hPtr->maxJobs) {
	    hPtr->hStatus |= HOST_STAT_FULL;
	    hReasonTb[1][hPtr->hostId] = PEND_HOST_JOB_LIMIT;
	}
	else {
	    struct qData *qp;
	    
	    hPtr->hStatus &= ~HOST_STAT_FULL;
	    CLEAR_REASON(hReasonTb[1][hPtr->hostId], PEND_HOST_JOB_LIMIT);
	    for (qp = qDataList->forw; qp != qDataList; qp = qp->forw)
		CLEAR_REASON(qp->reasonTb[1][hPtr->hostId], PEND_HOST_JOB_LIMIT);
	}
    }
    
    numofhosts++;
    lost_and_found->hostId = numofhosts;
    hDataPtrTb[numofhosts] = lost_and_found;
    checkHWindow();

    if (logclass & LC_TRACE) {
        for (i =1; i <= numofhosts; i++)
	    ls_syslog(LOG_DEBUG, "updHostList: Host <%s> now is still used by the batch system and put into hDataPtrTb[%d]", hDataPtrTb[i]->host, hDataPtrTb[i]->hostId);
        ls_syslog(LOG_DEBUG, "updHostList: Current batch system has numofhosts<%d>, numofprocs <%d>", numofhosts, numofprocs); 
    }
}  

static void 
updQueueList (void)
{
    int list, allHosts = FALSE;
    struct qData *qp, *lost_and_found = NULL, *next;
    struct jData *jpbw;
    struct qData *entry;
    struct qData *newqDataList = NULL; 
 
    
    if (reconfig) {
        for (qp = qDataList->forw; qp != qDataList; qp = next) {
	    next = qp->forw;
            if (strcmp (qp->queue, LOST_AND_FOUND) == 0) {
                continue;
	    }
            if (!(qp->flags & QUEUE_UPDATE)) {
	        resetQData(qp);
	    }
	} 
    }

    
    if ((newqDataList = (struct qData *) listCreate("Queue List")) ==
NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL, "updQueueList", "listCreate");
        mbdDie(MASTER_FATAL);
    }
    for (entry = qDataList->forw; entry != qDataList; entry = next) {
        next = entry->forw;
        listRemoveEntry((LIST_T*)qDataList, (LIST_ENTRY_T *)entry);
        for(qp = newqDataList->forw; qp != newqDataList; qp = qp->forw)
            if (entry->priority < qp->priority)
                break;
        
        inList((struct listEntry *)qp, (struct listEntry *)entry);
    }
    listDestroy((LIST_T *)qDataList, NULL);
    qDataList = newqDataList;
    
    for (qp = qDataList->forw; qp != qDataList; qp = next) {
	next = qp->forw;
        if (strcmp (qp->queue, LOST_AND_FOUND) == 0)
            continue;
        if (qp->flags & QUEUE_UPDATE) {
            qp->flags &= ~QUEUE_UPDATE;
            continue;
        } 
	
        if (lost_and_found == NULL &&
	    (lost_and_found = getQueueData(LOST_AND_FOUND)) == NULL)
            lost_and_found = lostFoundQueue();
        for (list = SJL; list < FJL; list++) {
            for (jpbw = jDataList[list]->back; jpbw != jDataList[list];
		 jpbw = jpbw->back) {
                if (jpbw->qPtr != qp) 
                    continue;
                
                updSwitchJob(jpbw, qp, lost_and_found,
			     jpbw->shared->jobBill.maxNumProcessors); 
            }  
        } 
        
        freeQData (qp, TRUE);
    }

    
    numofqueues = 0;
    for (qp = qDataList->forw; qp != qDataList; qp = qp->forw) {
        queueHostsPF (qp, &allHosts); 
        createQueueHostSet(qp);
        numofqueues++;
    }

    checkQWindow();

    return;

} 

static void
setHPartMemberCreate(struct gData *g, LS_BITSET_T *set)
{
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;
    struct hData *h;
    int i;

    hashEntryPtr = h_firstEnt_(&g->memberTab, &hashSearchPtr);
    while (hashEntryPtr) {
	char *host;
        host = hashEntryPtr->keyname;
	h = getHostData(host);
	setAddElement(set, (void *)h->hostId);
	hashEntryPtr = h_nextEnt_(&hashSearchPtr);
    }
    for (i = 0; i < g->numGroups; i++) 
	setHPartMemberCreate(g->gPtr[i], set);
}

static void 
updUserList (int mbdInitFlags)
{
    static char fname[] = "updUserList()";
    struct uData *  defUser;
    struct gData *  gPtr;
    int             i;
    char *          key;
    hEnt *          hashTableEnt; 

    defUser = getUserData ("default");

    if (defUser && !(defUser->flags & USER_UPDATE))
        defUser = NULL;

    FOR_EACH_HTAB_ENTRY(key, hashTableEnt, &uDataList) {
	struct uData *uData;

	uData = (struct uData *)hashTableEnt->hData;
        
	
	if ( mbdInitFlags == RECONFIG_CONF || mbdInitFlags == WINDOW_CONF) {
	    int j;
	    FREEUP( uData->reasonTb[0]);
	    FREEUP( uData->reasonTb[1]);
	    uData->reasonTb[0] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
	    uData->reasonTb[1] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
 
	    for(j = 0; j < numLsfHosts+2; j++) {
		uData->reasonTb[0][j] = 0;
		uData->reasonTb[1][j] = 0;
	    }
	}

        
	if (uData->flags & USER_UPDATE) {

            if ((gPtr= getUGrpData (uData->user)) != NULL) {
                if (mbdInitFlags != FIRST_START && 
		    uData->numJobs == 0 && 
		    uData->numPEND   == 0 && 
		    uData->numRUN  == 0 && 
		    uData->numSSUSP == 0 && 
		    uData->numUSUSP  == 0 && 
		    uData->numRESERVE  == 0)
                    
                    addUGDataValues (uData, gPtr);
            }
            
            if (uData != defUser)
                uData->flags &= ~USER_UPDATE;
            continue;

        } else if (getpwlsfuser_ (uData->user) != NULL) {
            
	    if (defUser != NULL) {
		
                uData->pJobLimit = defUser->pJobLimit;
		uData->maxJobs   = defUser->maxJobs;
            } else {
                uData->pJobLimit = INFINIT_FLOAT;
		uData->maxJobs   = INFINIT_INT;
            } 
		
        } else {
           
           if (mbdInitFlags != RECONFIG_CONF && mbdInitFlags != WINDOW_CONF) {
	       
	       FREEUP (uData->user);
               
               FREEUP(uData->reasonTb[0]);
               FREEUP(uData->reasonTb[1]);
               FREEUP(uData->reasonTb);
               setDestroy(uData->ancestors);
               uData->ancestors = NULL;
               setDestroy(uData->parents);
               uData->parents = NULL;
               setDestroy(uData->children);
               uData->children = NULL;
               setDestroy(uData->descendants);
               uData->descendants = NULL;
           
               if (uData->hAcct)
                   h_delTab_(uData->hAcct);
	       h_delEnt_(&uDataList, hashTableEnt);
           }
        }

    } END_FOR_EACH_HTAB_ENTRY;

    
    uDataGroupCreate();

    
    setuDataCreate();
    
    

    if (mbdInitFlags == RECONFIG_CONF || mbdInitFlags == WINDOW_CONF) {
	struct uData*  uData;

        
        for (i = 0; i < numofugroups; i++) {

            uData = getUserData (usergroups[i]->group);
		
	    if (defUser != NULL) {

	        if(uData->maxJobs == INFINIT_INT){
                     uData->maxJobs = defUser->maxJobs;
                }
                if(uData->pJobLimit == INFINIT_FLOAT){
                    uData->pJobLimit = defUser->pJobLimit;
                }

                
            } 
            
        }
    }
    
} 


static void 
addUGDataValues (struct uData *gUData, struct gData *gPtr)
{
    static char fname[] = "addUGDataValues()";
    int i, numMembers;
    char **groupMembers; 
    struct uData *uPtr;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "addUGDataValues: Enter the routine...");

    groupMembers = expandGrp(gPtr, gUData->user, &numMembers);
    
    if (numMembers == 1
	&& strcmp(gUData->user, groupMembers[0]) == 0) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6215,
	    "%s: User group <%s> with no members is ignored"),/* catgets 6215 */
	    fname, gUData->user);
    } else {
	for (i = 0; i < numMembers; i++) { 
            if ((uPtr = getUserData (groupMembers[i])) == NULL)
                continue;
            addUGDataValues1 (gUData, uPtr);
        }
        FREEUP(groupMembers);
    }
} 
static void 
addUGDataValues1 (struct uData *gUData, struct uData *uData)
{
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;
    struct hostAcct *hAcct;

    if (logclass & LC_TRACE)
	ls_syslog(LOG_DEBUG, "addUGDataValues1: Enter the routine...");
   
    gUData->numJobs += uData->numJobs;
    gUData->numPEND += uData->numPEND;
    gUData->numRUN += uData->numRUN;
    gUData->numSSUSP += uData->numSSUSP;
    gUData->numUSUSP += uData->numUSUSP;
    gUData->numRESERVE += uData->numRESERVE;

    if (uData->hAcct == NULL)
	return;

    hashEntryPtr = h_firstEnt_(uData->hAcct, &hashSearchPtr);    
    while (hashEntryPtr) {
        hAcct = (struct hostAcct *)hashEntryPtr->hData;
	hashEntryPtr = h_nextEnt_(&hashSearchPtr);
	addHAcct (&gUData->hAcct, hAcct->hPtr, hAcct->numRUN, hAcct->numSSUSP,
			 hAcct->numUSUSP, hAcct->numRESERVE);
    }    
} 

static int
parseQHosts(struct qData *qp, char *hosts)
{
    static char fname[] = "parseQHosts";
    int i;
    int  numAskedHosts = 0;
    struct askedHost *returnHosts;
    int returnErr, badReqIndx, others, numReturnHosts = 0;
    char *sp, *word, **askedHosts;

    if (qp == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6216,
	    "%s: NULL pointer for queue"), fname); /* catgets 6216 */
	return -1;
    }
    if (hosts == NULL) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6217,
	    "%s: NULL pointer in HOSTS for queue <%s>"), /* catgets 6217 */
	    fname, qp->queue); 
	return -1;
    }

    sp = hosts;
    
    while ((word=getNextWord_(&sp)) != NULL)
        numAskedHosts++; 
    if (numAskedHosts == 0) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6218,
	    "%s: No host specified in HOSTS for queue <%s>"), /* catgets 6218 */
	    fname, qp->queue);
	return -1;
    }
    askedHosts = (char **) my_calloc(numAskedHosts, sizeof(char *), fname);
    sp = hosts;
    numAskedHosts = 0;
    while ((word=getNextWord_(&sp)) != NULL) {
	askedHosts[numAskedHosts++] = safeSave(word);
    }
    
    returnErr = chkAskedHosts(numAskedHosts, askedHosts, numofprocs,
                      &numReturnHosts, &returnHosts, &badReqIndx, &others, 0);
    if (returnErr == LSBE_NO_ERROR) {
        if (numReturnHosts > 0) {
            qp->askedPtr = (struct askedHost *) my_calloc (numReturnHosts,
                                      sizeof(struct askedHost), fname);
            for (i = 0; i < numReturnHosts; i++) {
                qp->askedPtr[i].hData = returnHosts[i].hData;
                qp->askedPtr[i].priority = returnHosts[i].priority;
                if (qp->askedPtr[i].priority > 0)
                    qp->qAttrib |= Q_ATTRIB_HOST_PREFER;
            }

            qp->numAskedPtr = numReturnHosts;
            qp->askedOthPrio = others;
	    FREEUP (returnHosts);
	}
    } else if (parseFirstHostErr(returnErr, fname, hosts, qp, returnHosts, numReturnHosts)) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6219,
	    "%s: No valid host used by the batch system is specified in HOSTS <%s> for queue <%s>"), /* catgets 6219 */
	    fname, hosts, qp->queue);
    }
    for (i = 0; i <numAskedHosts; i++)
	 FREEUP (askedHosts[i]);
    FREEUP (askedHosts);
    
    if( returnErr == LSBE_NO_ERROR ) {
        return numReturnHosts;
    } else {
        return -1;
    }
}     

static void 
fillClusterConf (struct clusterConf *clusterConf)
{
    
    clusterConf->clinfo = clusterInfo;           
    clusterConf->numHosts = numLsfHosts;           
    clusterConf->hosts = lsfHostInfo;           

 } 

static void
fillSharedConf(struct sharedConf *sConf)
{
    static char     fname[] = "fillSharedConf";

    sConf->lsinfo = allLsInfo;
    sConf->servers = NULL;
    
    sConf->clusterName = (char *) my_malloc (sizeof (char *), fname);
    
} 

static void 
freeGrp (struct gData *grpPtr)
{
    if (grpPtr == NULL) 
	return;
    h_delTab_(&grpPtr->memberTab);
    if (grpPtr->group && grpPtr->group[0] != '\0')
        FREEUP (grpPtr->group);
    FREEUP (grpPtr);

}  
    
static  int 
validHostSpec (char *hostSpec)
{

    if (hostSpec == NULL)
	return (FALSE);

    if (getModelFactor (hostSpec) == NULL) {
        if (getHostFactor (hostSpec) == NULL) 
            return (FALSE);
    }
    return (TRUE);

} 

static void
getMaxCpufactor()
{
    int i;

    for (i = 1; i <= numofhosts; i++) {
	if (hDataPtrTb[i]->cpuFactor > maxCpuFactor) {
	    maxCpuFactor = hDataPtrTb[i]->cpuFactor;
	}
    }
} 



void
mbdReConf(int mbdInitFlags)
{
    char fname[] = "mbdReConf";
    struct hData *hPtr;
    int i, j;
    struct hostConf tempHostConf;

    ls_syslog(LOG_ERR, I18N(6255,"mbdReConf: start")); /*catgets 6255 */
    reconfig = TRUE;

    
    TIMEIT(0, allLsInfo = ls_info(), "minit_ls_info");
    if (allLsInfo == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "ls_info");
	mbdDie(MASTER_FATAL);
    } else {
        
        for(i=allLsInfo->nModels; i < MAXMODELS; i++)
            allLsInfo->cpuFactor[i] = 1.0;
    }

     
    initParse(allLsInfo);
    tclLsInfo = getTclLsInfo();
    initTcl(tclLsInfo);                

    TIMEIT(0, getLsfHostInfo (TRUE), "mbdReConf_getLsfHostInfo");
    if (lsfHostInfo == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_MM, fname, "minit_getLsfHostInfo");
	mbdDie(MASTER_FATAL);
    }

    getClusterData();

    
    cleanup(mbdInitFlags);

    
    if (mbdInitFlags != WINDOW_CONF) {
        ls_freeconf(paramFileConf);
        paramFileConf = NULL;
    }
    TIMEIT(0, readParamConf(mbdInitFlags), "mbdReConf_readParamConf");

    
    checkAcctLog();

    
    uDataTableFree(uDataPtrTb);   
    uDataPtrTbInitialize();

    if (mbdInitFlags != WINDOW_CONF) {
        ls_freeconf(userFileConf);
        userFileConf = NULL;
    }
    freeUConf(userConf, FALSE);
    
    freeWorkUser(TRUE);
    defUser = FALSE;

    
    allUsersSet = setCreate(MAX_GROUPS, getIndexByuData, getuDataByIndex,
			    "All User Set");	    
    if (allUsersSet == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6200,
					 "%s: failed to create all user set: %s"), /* catgets 6200 */
		  fname, setPerror(bitseterrno));
	mbdDie(MASTER_MEM);
    }
    
    
    setAllowObservers(allUsersSet);
    
    uGrpAllSet = setCreate(MAX_GROUPS,
			   getIndexByuData,
			   getuDataByIndex,
			   "\
Set of user groups containing 'all' users");

    
    uGrpAllAncestorSet = setCreate(MAX_GROUPS, 
				   getIndexByuData, 
				   getuDataByIndex, 
				   "\
Ancestor user groups of 'all' user groups");
    if (uGrpAllAncestorSet == NULL
	|| uGrpAllSet == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6201, "%s: failed to create the set of  'all;' user groups: %s"), /* catgets 6201 */
		  fname, setPerror(bitseterrno));
	mbdDie(MASTER_MEM);
    }

    TIMEIT(0, readUserConf(mbdInitFlags), "mbdReConf_readUserConf");

    

    if (mbdInitFlags != WINDOW_CONF) {
        ls_freeconf(hostFileConf);
        hostFileConf = NULL;
    }

    

    tempHostConf.numHosts = hostConf->numHosts;
    if ((tempHostConf.hosts = (struct hostInfoEnt *) malloc 
		 (hostConf->numHosts*sizeof(struct hostInfoEnt))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname,  
		"malloc",
		hostConf->numHosts*sizeof(struct hostInfoEnt));
        mbdDie(MASTER_RESIGN);
    }
    memcpy((void *)tempHostConf.hosts, (void *)hostConf->hosts,
		hostConf->numHosts*sizeof(struct hostInfoEnt));
    for ( i = 0; i <  hostConf->numHosts; i ++ ) {
	tempHostConf.hosts[i].host = putstr_(hostConf->hosts[i].host);
	if( (hPtr = getHostData(hostConf->hosts[i].host)) ){
	    tempHostConf.hosts[i].hStatus = hPtr->hStatus;
        }
    }

    freeWorkHost(FALSE);

    TIMEIT(0, readHostConf(mbdInitFlags), "mbdReConf_readHostConf");

    for ( i = 0; i < hostConf->numHosts; i ++ ) {
	for ( j = 0; j < tempHostConf.numHosts; j ++ ) {
	    if (equalHost_(tempHostConf.hosts[j].host, 
	        hostConf->hosts[i].host)) {
		if (tempHostConf.hosts[j].hStatus & HOST_STAT_DISABLED) {
		    
		    if( (hPtr = getHostData(tempHostConf.hosts[j].host)) ){
			hPtr->hStatus = tempHostConf.hosts[j].hStatus;
		    }
		    break;
		}
    	    }
	}
    }
    
    for ( i = 0; i <  tempHostConf.numHosts; i ++ ) {
	FREEUP (tempHostConf.hosts[i].host); 
    }
    FREEUP (tempHostConf.hosts);

    if (numResources > 0) 
        freeSharedResource();

    getLsbResourceInfo();

    
    if ((hPtr = getHostData (masterHost)) == NULL
                    || !(hPtr->flags & HOST_UPDATE)) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6114,
	    "%s: Master host <%s> is not defined in the Host section of the lsb.hosts file"), fname, masterHost);
	mbdDie(MASTER_FATAL);
    }

    
    mbdReconfPRMO();

    
    getLsbHostLoad();
    getLsbHostInfo(); 
    copyGroups(TRUE);

    for (i = 0; i < numLsfHosts; i++) {
        if (equalHost_(masterHost, lsfHostInfo[i].hostName)) {
            if (lsfHostInfo[i].isServer != TRUE) {
                ls_syslog(LOG_ERR, I18N(6107,
                        "%s: Master host <%s> is not a server"),  /* catgets 6107 */
                        fname,lsfHostInfo[i].hostName);
                mbdDie(MASTER_RESIGN);
            }
            break;
        }
    }
    
    if (defaultHostSpec != NULL && !validHostSpec (defaultHostSpec)) {
        ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 6230,
	    "%s: Invalid system defined DEFAULT_HOST_SPEC <%s>; ignored"), fname, defaultHostSpec); /* catgets 6230 */
        FREEUP (defaultHostSpec);
        FREEUP (paramConf->param->defaultHostSpec);
        lsb_CheckError = WARNING_ERR;
    }

    
    if (mbdInitFlags != WINDOW_CONF) {
        ls_freeconf(queueFileConf);
        queueFileConf = NULL;
    }
    freeQConf(queueConf, TRUE); 
    freeWorkQueue(FALSE);
    
    fillClusterConf(&clusterConf);
    updateClusterConf(&clusterConf);
    TIMEIT(0, readQueueConf(mbdInitFlags), "minit_readQueueConf");
    
    
    
    if (hReasonTb != NULL) {
        FREEUP (hReasonTb[0]);
        FREEUP (hReasonTb[1]);
        FREEUP (hReasonTb);
    }
    hReasonTb = (int **) my_calloc(2, sizeof(int *), fname);
    hReasonTb[0] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    hReasonTb[1] = (int *) my_calloc(numLsfHosts+2, sizeof(int), fname);
    
    for (i = 0; i <= numLsfHosts + 1; i++) {
        hReasonTb[0][i] = 0;
        hReasonTb[1][i] = 0;
    }

    copyGroups(FALSE);
    updUserList (mbdInitFlags);
    updHostList ();
    updQueueList ();

    getMaxCpufactor();

    rebuildUsersSets();

    rebuildCounters();

    
    reconfig = FALSE;
    mSchedStage = 0 ; 

    ls_syslog(LOG_DEBUG, "mbdReConf: done");
    return; 
} 


void 
cleanup(int mbdInitFlags)
{
    struct qData  *pqData;
    char *          key;
    hEnt *          hashTableEnt; 

    
    resetStaticSchedVariables();
    
    

    
    for (pqData = qDataList->forw; pqData != qDataList; pqData = pqData->forw) {
	
	pqData->numPEND = 0;
	pqData->numRUN  = 0;	
	pqData->numSSUSP= 0;	
	pqData->numUSUSP= 0;
	pqData->numJobs = 0;
	pqData->numRESERVE=0;

	

	
        if ( pqData->uAcct) {
	    if ( pqData->uAcct->slotPtr)
		h_delTab_(pqData->uAcct);

	    FREEUP(pqData->uAcct);
	}

	 
    	if (pqData->hAcct) {
	    if ( pqData->hAcct->slotPtr)
                h_delTab_(pqData->hAcct);

	    FREEUP(pqData->hAcct);
	}

    }

    
     setDestroy(allUsersSet);

    
    FOR_EACH_HTAB_ENTRY(key, hashTableEnt, &uDataList) {
        struct uData               *uPtr;
	char                       strBuf[512];

        uPtr = (struct uData *)hashTableEnt->hData;
	
	

	if ( ! (uPtr->flags & USER_ALL)) {

	    if (uPtr->children != NULL) {
		setDestroy(uPtr->children);
		sprintf(strBuf, "%s's children set", uPtr->user);
		uPtr->children = setCreate(MAX_GROUPS, 
					   getIndexByuData,
					   getuDataByIndex, 
					   strBuf);

	    }

	    if (uPtr->descendants != NULL) {
		setDestroy(uPtr->descendants);
		sprintf(strBuf, "%s's descendant set", uPtr->user);
		uPtr->descendants = setCreate(MAX_GROUPS, 
					      getIndexByuData,
					      getuDataByIndex, 
					      strBuf);

	    }
	}
        else {
            
            uPtr->children = NULL;
            uPtr->descendants = NULL;
        }
	
	
	if (uPtr->parents != NULL) {
	    setDestroy(uPtr->parents);
	    sprintf(strBuf, "%s's parent set", uPtr->user);
	    uPtr->parents = setCreate(MAX_GROUPS, 
				      getIndexByuData,
				      getuDataByIndex, 
				      strBuf);
	    
	}

	if (uPtr->ancestors != NULL) {
	    setDestroy(uPtr->ancestors);
	    sprintf(strBuf, "%s's descendant set", uPtr->user);
	    uPtr->ancestors = setCreate(MAX_GROUPS, 
					getIndexByuData,
                                        getuDataByIndex, 
					strBuf);
	}

	
	uPtr->numPEND = 0;
	uPtr->numRUN  = 0;	
	uPtr->numSSUSP= 0;	
	uPtr->numUSUSP= 0;
	uPtr->numJobs = 0;
	uPtr->numRESERVE=0;
	uPtr->maxJobs = INFINIT_INT;
	uPtr->pJobLimit = INFINIT_FLOAT;


    	if (uPtr->hAcct) {
	    if ( uPtr->hAcct->slotPtr)
            	h_delTab_(uPtr->hAcct);
	    FREEUP(uPtr->hAcct);
	}

	
	uPtr->flags  = 0;

    } END_FOR_EACH_HTAB_ENTRY;

    setDestroy(uGrpAllAncestorSet);
    setDestroy(uGrpAllSet);
    
    
    FOR_EACH_HTAB_ENTRY(key, hashTableEnt, &hDataList) {
        struct hData *phData;

        phData = (struct hData *)hashTableEnt->hData;
	phData->numRUN  = 0;	
	phData->numSSUSP= 0;	
	phData->numUSUSP= 0;
	phData->numRESERVE = 0;
	phData->numJobs = 0;

	if (phData->uAcct) {
	    if ( phData->uAcct->slotPtr)
		h_delTab_(phData->uAcct);
	    FREEUP(phData->uAcct);
	}

    } END_FOR_EACH_HTAB_ENTRY;

    
    if ( mbdInitFlags == RECONFIG_CONF) {
	FOR_EACH_HTAB_ENTRY(key, hashTableEnt, &condDataList) {
	    struct condData *pCondNode;
	    pCondNode = (struct condData *)hashTableEnt->hData; 
	    FREEUP(pCondNode->name);
	    freeDepCond(pCondNode->rootNode);
	}  END_FOR_EACH_HTAB_ENTRY;
	h_delTab_(&condDataList);
	h_initTab_(&condDataList, 20); 
    }

    
    resetJData( );

    return;
} 


void 
rebuildCounters( )
{
    static char      fname[] = "rebuildCounters";
    int list, i;
    hEnt   *hostEnt;
    struct hData *lostAndFoundHost;
    int jobList[3];
    PROXY_LIST_ENTRY_T      *pxy;

    
    hostEnt = h_getEnt_(&hDataList, LOST_AND_FOUND);
    if (hostEnt != NULL) {
	lostAndFoundHost = (struct hData *) hostEnt->hData;
    } else { 
	lostAndFoundHost = lostFoundHost();
    }

    
    jobList[0] = SJL;
    jobList[1] = PJL;
    jobList[2] = MJL;
  
    for ( i=0; i < 3; i++) {
	struct jData*  jp;
	list = jobList[i];

	for (jp = jDataList[list]->back; jp != jDataList[list];
	    jp = jp->back) {
	    int svJStatus = jp->jStatus;
	    int i, num;

	    
	    if ( (jp->shared->jobBill.options2 & SUB2_USE_DEF_PROCLIMIT)
		&& (list == PJL)) {
		jp->shared->jobBill.numProcessors =
		jp->shared->jobBill.maxNumProcessors =
		(jp->qPtr->defProcLimit > 0 ? jp->qPtr->defProcLimit : 1);
	    }

	    if ( jp->shared->jobBill.options & SUB_HOST) {
		checkAskedHostList(jp);
	    }

	    
	    if ( jp->hPtr) { 
		for(i=0; i < jp->numHostPtr; i++) {
		    if ( h_getEnt_(&hDataList, jp->hPtr[i]->host) == NULL) {
			
			jp->hPtr[i] = lostAndFoundHost;
			if ( i== 0 ) { 
			    
			    cleanSbdNode(jp);
			}
		    }
		}
	    }

	   
	   if ( jp->reqHistory) { 
	       checkReqHistory(jp);
	   }
	   
           num = jp->shared->jobBill.maxNumProcessors;
	   jp->jStatus = JOB_STAT_PEND;

	   updQaccount (jp, num, num, 0, 0, 0, 0);
	   updUserData (jp, num, num, 0, 0, 0, 0);
	   jp->jStatus = svJStatus; 

	   if (jp->jStatus & JOB_STAT_RESERVE) {

	       
	       if (pxyRsvJL != NULL) {
		   pxy =  (PROXY_LIST_ENTRY_T *)
		       listSearchEntry(pxyRsvJL,
				       (LIST_ENTRY_T *)jp,
				       (LIST_ENTRY_EQUALITY_OP_T)
				       proxyListEntryEqual,
				       0);
		   if (pxy != NULL) {
		       
		       if (!(jp->jStatus & JOB_STAT_PEND)) {
			   ls_syslog (LOG_ERR, I18N(6100,
"%s: job <%s> status=0x%x is still in pxyRsvJL"),/* catgets 6100 */
				      fname, lsb_jobid2str(jp->jobId),
				      jp->jStatus);
		       }
		       
		       proxyRsvJLRemoveEntry(jp);
		   }
	       }
	   } 

	   
	   jp->jStatus &= ~JOB_STAT_RESERVE;

	   if (jp->jStatus & JOB_STAT_PEND)
	       continue;
	   updCounters (jp, JOB_STAT_PEND, !LOG_IT);
	   if ((jp->shared->jobBill.options & SUB_EXCLUSIVE)
	       && IS_START (jp->jStatus)) {
	       for (i = 0; i < jp->numHostPtr; i ++)
		   jp->hPtr[i]->hStatus |= HOST_STAT_EXCLUSIVE;
	   }
       }
   }

   
   if ( removedHDataArray) { 
       for(i=0; removedHDataArray[i]; i++) {
	   if (logclass & LC_TRACE) {
	       ls_syslog(LOG_DEBUG, "remove host=<%s> hData=<%x>",
			 removedHDataArray[i]->host, removedHDataArray[i]);
	   }
	   freeHData(removedHDataArray[i], TRUE);
       }
       FREEUP(removedHDataArray);
   }

   return;
} 

static int
parseFirstHostErr(int returnErr, char *fname, char *hosts, struct qData *qp, struct askedHost *returnHosts, int numReturnHosts)
{
    int i;

    if (   ( returnErr == LSBE_MULTI_FIRST_HOST )
        || ( returnErr == LSBE_HG_FIRST_HOST )
        || ( returnErr == LSBE_HP_FIRST_HOST )
        || ( returnErr == LSBE_OTHERS_FIRST_HOST )) {
        
        if (returnErr == LSBE_MULTI_FIRST_HOST )
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 6242, "%s: Multiple first execution hosts specified <%s> in HOSTS section for queue <%s>. Ignored."),/*catgets 6242 */
	        fname, hosts, qp->queue); 
        else if ( returnErr == LSBE_HG_FIRST_HOST )
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 6243, "%s: host group <%s> specified as first execution host in HOSTS section for queue <%s>. Ignored."), /* catgets 6243 */
                fname, hosts, qp->queue);
        else if ( returnErr == LSBE_HP_FIRST_HOST )
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 6244, "%s: host partition <%s> specified as first execution hosts specified <%s> in HOSTS section for queue <%s>. Ignored."), /* catgets 6244 */
		fname, hosts, qp->queue);
        else if ( returnErr == LSBE_OTHERS_FIRST_HOST )
            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd, NL_SETN, 6245, "%s: \"others\" specified as first execution host specified in HOSTS section for queue <%s>. Ignored."), /* catgets 6245 */
		fname, qp->queue);
        
        if (returnHosts) FREEUP (returnHosts);
        qp->numAskedPtr = numReturnHosts;
        if (numReturnHosts > 0 ) {
            qp->askedPtr = (struct askedHost *) my_calloc (numReturnHosts,
                                        sizeof(struct askedHost), fname);
            for (i = 0; i < numReturnHosts; i++) {
                qp->askedPtr[i].hData = returnHosts[i].hData;
                qp->askedPtr[i].priority = returnHosts[i].priority;
                if (qp->askedPtr[i].priority > 0)
                    qp->qAttrib |= Q_ATTRIB_HOST_PREFER;
            }
        } else {
            qp->askedPtr = NULL;
            qp->askedOthPrio = 0;
            (*hosts) = '\0';
        }
        return 0;
    } else 
	return 1;
} 

static int
resetJData() 
{
    struct jData *jp;
    struct submitReq *jobBill; 

    for (jp = jDataList[PJL]->back; jp != jDataList[PJL];
	 jp = jp->back) {
	
	jobBill = &(jp->shared->jobBill);	

	
	jp->priority = -1;  
	jp->jFlags  &= ~(JFLAG_READY | JFLAG_DEPCOND_REJECT 
	    | JFLAG_READY1 | JFLAG_READY2 | JFLAG_WILL_BE_PREEMPTED); 
	jp->oldReason = PEND_SYS_NOT_READY;
 	if (jp->jStatus & JOB_STAT_PSUSP) {  
	    jp->newReason = PEND_USER_STOP;          
        } else {
	    jp->newReason = PEND_SYS_NOT_READY;
        }
	jp->subreasons = 0;
	FREEUP(jp->reasonTb);
	jp->numReasons = 0;
	FREE_CAND_PTR(jp); 
	jp->numCandPtr = 0;
	if ( jp->numExecCandPtr > 0) {
	    int j;
	    for (j = 0; j < jp->numExecCandPtr; j++) { 
		DESTROY_BACKFILLEE_LIST(jp->execCandPtr[j].backfilleeList); 
	    } 
	    FREEUP(jp->execCandPtr);
	    jp->numExecCandPtr = 0;
	}

	
        FREEUP (jp->hPtr);
        jp->numHostPtr = 0;

	jp->numEligProc = 0 ;
	jp->numAvailEligProc = 0;
	jp->numSlots = 0;
	jp->numAvailSlots = 0;
	jp->usePeerCand = FALSE;
	jp->reserveTime  = 0;
	jp->slotHoldTime = -1;
	jp->processed   = 0 ;
	jp->predictedStartTime = 0;
	jp->dispTime = 0;
	jp->jStatus &= ~JOB_STAT_RSRC_PREEMPT_WAIT;
	if (jp->numRsrcPreemptHPtr > 0) {
	    FREEUP (jp->rsrcPreemptHPtr);
	}
	jp->numRsrcPreemptHPtr = 0;
    } 
    return 0;
} 

static int 
resetQData(struct qData *qp) 
{   
    char description[MAXLINELEN];

    if (qp == NULL) {
	return 0;
    }

    sprintf(description, "This queue <%s> becomes LOST_AND_FOUND because it is removed from the configuration by LSF Administrator.", qp->queue);
    if (qp->description) {
	FREEUP(qp->description);
    }
    qp->description = safeSave(description);
    if (qp->queue) {
	FREEUP(qp->queue);
    }
    qp->queue = safeSave (LOST_AND_FOUND);
    qp->uJobLimit = 0;
    qp->pJobLimit = 0.0;
    qp->acceptIntvl = DEF_ACCEPT_INTVL;
    qp->qStatus = (!QUEUE_STAT_OPEN | !QUEUE_STAT_ACTIVE);
    freeGrp(qp->uGPtr);
    qp->uGPtr = (struct gData *) my_malloc
                                (sizeof (struct gData), "resetQueueStatus");
    qp->uGPtr->group = "";
    h_initTab_(&qp->uGPtr->memberTab, 16);
    qp->uGPtr->numGroups = 0;
    h_addEnt_(&qp->uGPtr->memberTab, "nobody", 0);
    
    FREEUP(qp->hostSpec);
    qp->hostSpec = safeSave (masterHost);
    qp->numRESERVE = 0;

    FREEUP(qp->askedPtr);
    qp->numAskedPtr = 0; 

    qp->queueId = 0;
    qp->priority = DEF_PRIO;

    return 0;
} 

static void 
checkAskedHostList(struct jData *jp)
{
    static char fname[]="checkAskedHostList";
    struct askedHost *askedHosts = NULL; 
    int numAskedHosts = 0, askedOthPrio, returnErr, badReqIndx, i;
    struct submitReq *subReq = &(jp->shared->jobBill);	

    
    if ( jp == NULL || !(subReq->options & SUB_HOST)) { 
	return;
    }
	
    
    returnErr = chkAskedHosts (subReq->numAskedHosts,
			       subReq->askedHosts,
			       subReq->numProcessors, &numAskedHosts,
			       &askedHosts, &badReqIndx,
			       &askedOthPrio, 1);

    FREEUP(jp->askedPtr);
    jp->numAskedPtr = 0;
    if ( (returnErr == LSBE_NO_ERROR) && (numAskedHosts > 0) ) {
	jp->askedPtr = (struct askedHost *) my_calloc (numAskedHosts,
						       sizeof(struct askedHost), fname);
	for (i = 0; i < numAskedHosts; i++) {
	    jp->askedPtr[i].hData = askedHosts[i].hData;
	    jp->askedPtr[i].priority = askedHosts[i].priority;
	}
	jp->numAskedPtr = numAskedHosts;
	jp->askedOthPrio = askedOthPrio;
	FREEUP(askedHosts);
    } else { 
	if ( jp->jStatus & JOB_STAT_PEND) {
	    jp->newReason = PEND_BAD_HOST;
	}
    } 
    
    
    for (i = 0; i < jp->numAskedPtr; i++) {
	if (jp->askedPtr[i].hData != NULL 
	    && !isHostQMember (jp->askedPtr[i].hData, jp->qPtr)
	    && jp->jStatus & JOB_STAT_PEND) {
	    jp->newReason = PEND_QUEUE_HOST;
	    break;
	}
    }

    return;
} 

static void
checkReqHistory(struct jData *jp)
{
    static char fname[]="checkReqHistory";
    struct rqHistory *reqHistory = jp->reqHistory, *tmpReqHistory=NULL;
    int reqHistoryNum=0, i;

    
    if ( jp == NULL || jp->reqHistory == NULL) {
	return;
    }

    tmpReqHistory = (struct rqHistory *)my_calloc(jp->reqHistoryAlloc,
						  sizeof (struct rqHistory), 
						  fname);
    for (i=0; reqHistory[i].host!=NULL; i++) {
	if ( h_getEnt_(&hDataList, reqHistory[i].host->host) != NULL) {
	    tmpReqHistory[reqHistoryNum].host =  reqHistory[i].host;
	    tmpReqHistory[reqHistoryNum].retry_num =  reqHistory[i].retry_num;
	    reqHistoryNum++;
	} 
    } 
    tmpReqHistory[reqHistoryNum].host = NULL;
    FREEUP(reqHistory);
    jp->reqHistory = tmpReqHistory;

    return;
} 

static void
rebuildUsersSets(void)
{
    static char                fname[] = "rebuildUsersSets";
    struct uData               *uData;
    struct uData               *allUserGrp;
    hEnt                       *hashTabEnt;
    char                       *key;
    LS_BITSET_ITERATOR_T       iter;
    
         
    FOR_EACH_HTAB_ENTRY(key, hashTabEnt, &uDataList) {

	uData = (struct uData *)hashTabEnt->hData;

	if (!(uData->flags & USER_GROUP)
	    && (strstr(uData->user, "others")) == NULL) {
	    
	    setAddElement(allUsersSet, (void *)uData);

	    
	    BITSET_ITERATOR_ZERO_OUT(&iter);
	    setIteratorAttach(&iter, uGrpAllSet, fname);
	    for (allUserGrp = (struct uData *)setIteratorBegin(&iter);
		 allUserGrp != NULL;
		 allUserGrp = (struct uData *)
		     setIteratorGetNextElement(&iter)) {

		char strBuf[128];
		
		if (uData->parents == NULL) {
		    sprintf(strBuf, "%s's parents set", uData->user);
		    uData->parents = setCreate(MAX_GROUPS, 
					       getIndexByuData, 
					       getuDataByIndex ,
					       strBuf);
		    if (uData->parents == NULL) {
			ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_EMSG_S, 
				  fname, "\
setCreate", strBuf, setPerror(bitseterrno));
			mbdDie(MASTER_MEM);
		    }
		}
	    
		if (uData->ancestors == NULL) {
		    sprintf(strBuf, "%s's ancestors set", uData->user);
		    uData->ancestors = setCreate(MAX_GROUPS, 
						 getIndexByuData, 
						 getuDataByIndex ,
						 strBuf);
		    if (uData->ancestors == NULL) {
			ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_EMSG_S, 
				  fname, "\
setCreate", strBuf, setPerror(bitseterrno));
			mbdDie(MASTER_MEM);
		    }
		}
		
		setAddElement(uData->parents, (void *)allUserGrp);
		setAddElement(uData->ancestors, (void *)allUserGrp);
		
		
		if (allUserGrp->ancestors != NULL) {
		    setOperate(uData->ancestors, 
			       allUserGrp->ancestors, 
			       LS_SET_UNION);
		}
	    } 

	} 
    } END_FOR_EACH_HTAB_ENTRY;

} 


