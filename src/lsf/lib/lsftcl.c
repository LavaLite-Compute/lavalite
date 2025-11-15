/* $Id: lsftcl.c,v 1.5 2007/08/15 22:18:49 tmizan Exp $
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

#include <tcl.h>
#include <netdb.h>
#include "lsf/lib/llsys.h"
#include "lsf/lib/lproto.h"

static struct tclHostData *currHPtr;
static struct Tcl_Interp *globinterp = NULL;
static char overRideFromType;
static char runTimeDataQueried;
static int numIndx;
static int nRes;
static struct tclLsInfo *myTclLsInfo = NULL;

int numericValue(ClientData, Tcl_Interp *, Tcl_Value *, Tcl_Value *);
int booleanValue(ClientData, Tcl_Interp *, Tcl_Value *, Tcl_Value *);
int stringValue(ClientData, Tcl_Interp *, int, const char **);
static int copyTclLsInfo(struct tclLsInfo *);
static char *getResValue(int);
static int definedCmd(ClientData, Tcl_Interp *, int, const char **);

int numericValue(ClientData clientData, Tcl_Interp *interp, Tcl_Value *args,
                 Tcl_Value *resultPtr)
{
    int infoId = (int) clientData;
    float cpuf = currHPtr->cpuFactor;
    char *value;
    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "numericValue: infoId = %d", infoId);
    resultPtr->type = TCL_INT;
    runTimeDataQueried = true;

    if (infoId < numIndx) {
        resultPtr->type = TCL_DOUBLE;
        if (infoId <= R15M)
            resultPtr->doubleValue = currHPtr->loadIndex[infoId] * cpuf - 1;
        else {
            resultPtr->doubleValue = currHPtr->loadIndex[infoId];
            if (currHPtr->loadIndex[infoId] >= (INFINITY - 10.0) &&
                currHPtr->flag != TCL_CHECK_SYNTAX) {
                return TCL_ERROR;
            }
        }
    } else {
        if (infoId == CPUFACTOR) {
            runTimeDataQueried = false;
            resultPtr->type = TCL_DOUBLE;
            resultPtr->doubleValue = cpuf;
        } else if (infoId == NDISK) {
            runTimeDataQueried = false;
            resultPtr->intValue = currHPtr->nDisks;
        } else if (infoId == REXPRI) {
            runTimeDataQueried = false;
            resultPtr->intValue = currHPtr->rexPriority;
        } else if (infoId == MAXCPUS_)
            resultPtr->intValue = currHPtr->maxCpus;
        else if (infoId == MAXMEM)
            resultPtr->intValue = currHPtr->maxMem;
        else if (infoId == MAXSWAP)
            resultPtr->intValue = currHPtr->maxSwap;
        else if (infoId == MAXTMP)
            resultPtr->intValue = currHPtr->maxTmp;
        else if (infoId == SERVER) {
            runTimeDataQueried = false;
            resultPtr->intValue = (currHPtr->hostInactivityCount == -1) ? 0 : 1;
        } else {
            value = getResValue((int) clientData - myTclLsInfo->numIndx);
            if (value == NULL || !strcmp(value, "-")) {
                resultPtr->intValue = 0;
                return TCL_OK;
            }
            resultPtr->doubleValue = atof(value);
            resultPtr->type = TCL_DOUBLE;
            if (logclass & LC_TRACE)
                ls_syslog(LOG_DEBUG3, "numericValue:value = %s, clientData =%d",
                          value, (int) clientData);
        }
    }
    return TCL_OK;
}

int booleanValue(ClientData clientData, Tcl_Interp *interp, Tcl_Value *args,
                 Tcl_Value *resultPtr)
{
    int isSet;
    char *value;

    int ii = (int) clientData;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "booleanValue: ii = %d", ii);
    if (ii < 0)
        return TCL_ERROR;

    overRideFromType = true;

    resultPtr->type = TCL_INT;
    if (currHPtr->resBitMaps == NULL) {
        resultPtr->intValue = 0;
        return TCL_OK;
    }
    TEST_BIT(ii, currHPtr->resBitMaps, isSet);
    if (isSet == 1)
        resultPtr->intValue = isSet;
    else {
        value = getResValue(ii);
        if (value == NULL || value[0] == '-') {
            if (currHPtr->flag == TCL_CHECK_SYNTAX)
                resultPtr->intValue = 1;
            else
                resultPtr->intValue = 0;
        } else
            resultPtr->intValue = atoi(value);
    }
    return TCL_OK;
}

int stringValue(ClientData clientData, Tcl_Interp *interp, int argc,
                const char *argv[])
{
    char *sp, *sp2, *value;
    char *officialName;
    char status[MAXLSFNAMELEN];

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "stringValue: argv[0]=%s argv[1]=%s, argv[2]=%s, "
                  "clientData=%d, host name =%s",
                  argv[0], argv[1], argv[2], (int) clientData,
                  currHPtr->hostName);
    if (argc != 3) {
        interp->result = "wrong # args";
        return TCL_ERROR;
    }

    switch ((int) clientData) {
    case HOSTNAME:
        overRideFromType = true;
        sp = currHPtr->hostName;
        officialName = (char *) getHostOfficialByName_(argv[2]);
        if (officialName != NULL)
            sp2 = officialName;
        else
            sp2 = (char *) argv[2];
        break;
    case HOSTTYPE:
        sp = currHPtr->hostType;
        if (strcmp(argv[2], LOCAL_STR) == 0) {
            sp2 = currHPtr->fromHostType;

            if (strcmp(argv[1], "eq") != 0)
                overRideFromType = true;
        } else {
            overRideFromType = true;
            sp2 = (char *) argv[2];
        }
        break;
    case HOSTMODEL:
        overRideFromType = true;
        sp = currHPtr->hostModel;
        if (strcmp(argv[2], LOCAL_STR) == 0)
            sp2 = currHPtr->fromHostModel;
        else
            sp2 = (char *) argv[2];
        break;
    case HOSTSTATUS:
        status[0] = '\0';
        overRideFromType = true;
        if (LS_ISUNAVAIL(currHPtr->status)) {
            strcpy(status, "unavail");
        } else if (LS_ISLOCKED(currHPtr->status)) {
            strcpy(status, "lock");
            if (LS_ISLOCKEDU(currHPtr->status)) {
                strcat(status, "U");
            }
            if (LS_ISLOCKEDW(currHPtr->status)) {
                strcat(status, "W");
            }
            if (LS_ISLOCKEDM(currHPtr->status)) {
                strcat(status, "M");
            }
        } else if (LS_ISBUSY(currHPtr->status)) {
            strcpy(status, "busy");
        } else {
            strcpy(status, "ok");
        }
        sp = status;
        sp2 = (char *) argv[2];
        break;
    default:

        value = getResValue((int) clientData - LAST_STRING);
        if (value == NULL || value[0] == '-') {
            if (currHPtr->flag == TCL_CHECK_SYNTAX) {
                interp->result = "1";
                return TCL_OK;
            } else {
                return TCL_ERROR;
            }
        }
        overRideFromType = true;
        sp = value;
        sp2 = (char *) argv[2];
        break;
    }

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "stringValue: sp = %s, sp2 = %s", sp, sp2);

    if (strcmp(sp2, WILDCARD_STR) == 0) {
        interp->result = "1";
        return TCL_OK;
    }

    if (strcmp(argv[1], "eq") == 0) {
        if (strcmp(sp2, sp) == 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else if (strcmp(argv[1], "ne") == 0) {
        if (strcmp(sp2, sp) != 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else if (strcmp(argv[1], "ge") == 0) {
        if (strcmp(sp2, sp) <= 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else if (strcmp(argv[1], "le") == 0) {
        if (strcmp(sp2, sp) >= 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else if (strcmp(argv[1], "gt") == 0) {
        if (strcmp(sp2, sp) < 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else if (strcmp(argv[1], "lt") == 0) {
        if (strcmp(sp2, sp) > 0)
            interp->result = "1";
        else
            interp->result = "0";
    } else {
        return TCL_ERROR;
    }

    return TCL_OK;
}

static int definedCmd(ClientData clientData, Tcl_Interp *interp, int argc,
                      const char *argv[])
{
    int resNo, hasRes = false, isSet;
    char *value;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "definedCmd: argv[0]=%s argv[1]=%s, clientData=%d", argv[0],
                  argv[1], (int) clientData);
    if (argc != 2) {
        interp->result = "wrong # args";
        return TCL_ERROR;
    }

    overRideFromType = true;
    for (resNo = 0; resNo < myTclLsInfo->nRes; resNo++) {
        if (strcmp(myTclLsInfo->resName[resNo], argv[1]) == 0) {
            hasRes = true;
            break;
        }
    }
    if (hasRes == false)
        return TCL_ERROR;

    if (currHPtr->resBitMaps == NULL) {
        interp->result = "0";
        return TCL_OK;
    }
    TEST_BIT(resNo, currHPtr->resBitMaps, isSet);
    if (isSet == 1)
        interp->result = "1";
    else {
        value = getResValue(resNo);
        if (value == NULL) {
            if (currHPtr->flag == TCL_CHECK_SYNTAX)
                interp->result = "1";
            else
                interp->result = "0";
        } else
            interp->result = "1";
    }

    return TCL_OK;
}

int initTcl(struct tclLsInfo *tclLsInfo)
{
    int i, resNo, isSet;
    attribFunc *funcPtr;
    static attribFunc attrFuncTable[] = {

        {"cpu", R1M},  {"login", LS}, {"idle", IT},  {"swap", SWP},

        {"cpuf", 0},   {"ndisks", 0}, {"rexpri", 0}, {"ncpus", 0},
        {"maxmem", 0}, {"maxswp", 0}, {"maxtmp", 0}, {"server", 0},
        {NULL, -1}};

    if (myTclLsInfo) {
        freeTclLsInfo(myTclLsInfo, 1);
    }
    if (globinterp) {
        Tcl_DeleteInterp(globinterp);
    }

    if (copyTclLsInfo(tclLsInfo) < 0)
        return -1;
    numIndx = tclLsInfo->numIndx;
    nRes = tclLsInfo->nRes;

    attrFuncTable[4].clientData = CPUFACTOR;
    attrFuncTable[5].clientData = NDISK;
    attrFuncTable[6].clientData = REXPRI;
    attrFuncTable[7].clientData = MAXCPUS_;
    attrFuncTable[8].clientData = MAXMEM;
    attrFuncTable[9].clientData = MAXSWAP;
    attrFuncTable[10].clientData = MAXTMP;
    attrFuncTable[11].clientData = SERVER;

    globinterp = Tcl_CreateInterp();

    for (i = 0; i < tclLsInfo->numIndx; i++) {
        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG3, "initTcl:indexNames=%s, i =%d",
                      tclLsInfo->indexNames[i], i);
        Tcl_CreateMathFunc(globinterp, tclLsInfo->indexNames[i], 0, NULL,
                           numericValue, (ClientData) i);
    }

    for (resNo = 0; resNo < tclLsInfo->nRes; resNo++) {
        TEST_BIT(resNo, tclLsInfo->numericResBitMaps, isSet);
        if (isSet == 0)
            continue;
        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG3, "initTcl:Name=%s, i =%d",
                      tclLsInfo->resName[resNo], i);

        Tcl_CreateMathFunc(globinterp, tclLsInfo->resName[resNo], 0, NULL,
                           numericValue,
                           (ClientData) (resNo + tclLsInfo->numIndx));
        i++;
    }

    for (funcPtr = attrFuncTable; funcPtr->name != NULL; funcPtr++) {
        if (logclass & LC_TRACE)
            ls_syslog(LOG_DEBUG3, "initTcl:indexNames=%s, i =%d", funcPtr->name,
                      funcPtr->clientData);
        Tcl_CreateMathFunc(globinterp, funcPtr->name, 0, NULL, numericValue,
                           (ClientData) funcPtr->clientData);
    }

    for (resNo = 0; resNo < tclLsInfo->nRes; resNo++) {
        TEST_BIT(resNo, tclLsInfo->numericResBitMaps, isSet);
        if (isSet == true)
            continue;
        TEST_BIT(resNo, tclLsInfo->stringResBitMaps, isSet);
        if (isSet == true)
            continue;
        Tcl_CreateMathFunc(globinterp, tclLsInfo->resName[resNo], 0, NULL,
                           booleanValue, (ClientData) resNo);
    }

    Tcl_CreateCommand(globinterp, "type", stringValue, (ClientData) HOSTTYPE,
                      (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(globinterp, "model", stringValue, (ClientData) HOSTMODEL,
                      (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(globinterp, "status", stringValue,
                      (ClientData) HOSTSTATUS, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(globinterp, "hname", stringValue, (ClientData) HOSTNAME,
                      (Tcl_CmdDeleteProc *) NULL);
    for (resNo = 0; resNo < tclLsInfo->nRes; resNo++) {
        TEST_BIT(resNo, tclLsInfo->stringResBitMaps, isSet);
        if (isSet == false)
            continue;

        Tcl_CreateCommand(globinterp, tclLsInfo->resName[resNo], stringValue,
                          (ClientData) (resNo + LAST_STRING),
                          (Tcl_CmdDeleteProc *) NULL);
    }

    Tcl_CreateCommand(globinterp, "defined", definedCmd,
                      (ClientData) DEFINEDFUNCTION, (Tcl_CmdDeleteProc *) NULL);

    return 0;
}

int evalResReq(char *resReq, struct tclHostData *hPtr, char useFromType)
{
    int code, i, resBits;

    currHPtr = hPtr;

    overRideFromType = false;
    runTimeDataQueried = false;

    currHPtr->overRideFromType = false;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3, "evalResReq: resReq=%s, host = %s", resReq,
                  hPtr->hostName);

    code = Tcl_Eval(globinterp, resReq);
    if (code != TCL_OK || *globinterp->result == 0) {
        return -1;
    }
    currHPtr->overRideFromType = overRideFromType;

    resBits = 0;
    if (!hPtr->ignDedicatedResource && hPtr->DResBitMaps != NULL) {
        for (i = 0; i < GET_INTNUM(nRes); i++)
            resBits += hPtr->DResBitMaps[i];

        if (resBits != 0 && hPtr->resBitMaps) {
            resBits = 0;
            for (i = 0; i < GET_INTNUM(nRes); i++)
                resBits += hPtr->resBitMaps[i] & hPtr->DResBitMaps[i];
            if (resBits == 0)
                return 0;
        }
    }

    if (!overRideFromType && useFromType)
        if (strcmp(hPtr->hostType, hPtr->fromHostType) != 0)
            return 0;

    if (runTimeDataQueried && LS_ISUNAVAIL(hPtr->status))
        return 0;

    if (strcmp(globinterp->result, "0") == 0)
        return 0;
    else
        return 1;
}

static char *getResValue(int resNo)
{
    int i;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG3,
                  "getResValue:resNo=%d, resName =%s, nRes=%d, "
                  "currHPtr->numResPairs=%d",
                  resNo, myTclLsInfo->resName[resNo], myTclLsInfo->nRes,
                  currHPtr->numResPairs);
    if (resNo > myTclLsInfo->nRes || currHPtr->numResPairs <= 0)
        return NULL;

    for (i = 0; i < currHPtr->numResPairs; i++) {
        if (strcmp(currHPtr->resPairs[i].name, myTclLsInfo->resName[resNo]) ==
            0) {
            return (currHPtr->resPairs[i].value);
        }
    }
    return NULL;
}

static int copyTclLsInfo(struct tclLsInfo *tclLsInfo)
{
    int i;
    static char fname[] = "copyTclLsInfo";

    if ((myTclLsInfo = (struct tclLsInfo *) malloc(sizeof(struct tclLsInfo))) ==
        NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }

    myTclLsInfo->nRes = tclLsInfo->nRes;
    myTclLsInfo->numIndx = tclLsInfo->numIndx;

    if ((myTclLsInfo->resName =
             (char **) malloc(myTclLsInfo->nRes * sizeof(char *))) == NULL ||
        (myTclLsInfo->stringResBitMaps = (int *) malloc(
             GET_INTNUM(myTclLsInfo->nRes) * sizeof(int))) == NULL ||
        (myTclLsInfo->indexNames =
             (char **) malloc(myTclLsInfo->numIndx * sizeof(char *))) == NULL ||
        (myTclLsInfo->numericResBitMaps = (int *) malloc(
             GET_INTNUM(myTclLsInfo->nRes) * sizeof(int))) == NULL) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "malloc");
        return -1;
    }

    for (i = 0; i < myTclLsInfo->nRes; i++) {
        if ((myTclLsInfo->resName[i] = putstr_(tclLsInfo->resName[i])) ==
            NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "putstr_");
            return -1;
        }
    }

    for (i = 0; i < myTclLsInfo->numIndx; i++) {
        if ((myTclLsInfo->indexNames[i] = putstr_(tclLsInfo->indexNames[i])) ==
            NULL) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "putstr_");
            return -1;
        }
    }

    for (i = 0; i < GET_INTNUM(myTclLsInfo->nRes); i++) {
        myTclLsInfo->stringResBitMaps[i] = tclLsInfo->stringResBitMaps[i];
        myTclLsInfo->numericResBitMaps[i] = tclLsInfo->numericResBitMaps[i];
    }

    return 0;
}

void freeTclLsInfo(struct tclLsInfo *tclLsInfo, int mode)
{
    int i;

    if (tclLsInfo) {
        if (mode == 1) {
            for (i = 0; i < tclLsInfo->numIndx; i++) {
                FREEUP(tclLsInfo->indexNames[i])
            }
            for (i = 0; i < tclLsInfo->nRes; i++) {
                FREEUP(tclLsInfo->resName[i])
            }
        }
        FREEUP(tclLsInfo->indexNames);
        FREEUP(tclLsInfo->resName);
        FREEUP(tclLsInfo->stringResBitMaps);
        FREEUP(tclLsInfo->numericResBitMaps);
        FREEUP(tclLsInfo);
    }
}
