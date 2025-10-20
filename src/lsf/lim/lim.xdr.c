/* $Id: lim.xdr.c,v 1.6 2007/08/15 22:18:54 tmizan Exp $
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

#define MAX_FLOAT16 4.227858E+09

static u_short encfloat16_(float );
static float   decfloat16_(u_short);
static void freeResPairs (struct resPair *, int);
static bool_t xdr_resPair (XDR *, struct resPair *, struct LSFHeader *);

bool_t
xdr_loadvector(XDR *xdrs, struct loadVectorStruct *lvp, struct LSFHeader *hdr)
{
    static char fname[] = "xdr_loadvector";
    int i;
    static struct resPair *resPairs = NULL;
    static int numResPairs = 0;

    if (!(xdr_int(xdrs, &lvp->hostNo) &&
          xdr_u_int(xdrs, &lvp->seqNo) &&
          xdr_int(xdrs, &lvp->numResPairs) &&
          xdr_int(xdrs, &lvp->checkSum) &&
          xdr_int(xdrs, &lvp->flags) &&
          xdr_int(xdrs, &lvp->numIndx) &&
          xdr_int(xdrs, &lvp->numUsrIndx)) )
    {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname,
                  "xdr_int/xdr_u_int");
        return false;
    }

    if ( (xdrs->x_op == XDR_DECODE)
         &&
         (limParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL) ) {

        if (myClusterPtr->checkSum != lvp->checkSum) {
            if (limParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL) {
                ls_syslog(LOG_WARNING, _i18n_msg_get(ls_catd , NL_SETN, 6001,
                                                     "%s: Sender has a different configuration"), fname); /* catgets 6001 */
            }
        }


        if (allInfo.numIndx != lvp->numIndx
            || allInfo.numUsrIndx != lvp->numUsrIndx) {

            ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 6002,
                                             "%s: Sender has a different number of load index vectors. It will be rejected from the cluster by the master host."), fname); /* catgets 6002 */
            return false;
        }
    }

    for (i = 0; i < 1+GET_INTNUM(lvp->numIndx); i++) {
        if (!xdr_int(xdrs, (int *) &lvp->status[i])) {
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "xdr_int");
            return false;
        }
    }
    if (!xdr_lvector(xdrs, lvp->li, lvp->numIndx)) {
        ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "xdr_lvector");
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        freeResPairs (resPairs, numResPairs);
        resPairs = NULL;
        numResPairs = 0;
        if (lvp->numResPairs > 0) {
            resPairs = (struct resPair *)
                malloc (lvp->numResPairs * sizeof (struct resPair));
            if (resPairs == NULL) {
                lvp->numResPairs = 0;
                ls_syslog(LOG_ERR, I18N_FUNC_D_FAIL_M, fname, "malloc",
                          lvp->numResPairs * sizeof (struct resPair));
                return false;
            }
            lvp->resPairs = resPairs;
        } else
            lvp->resPairs = NULL;
    }
    for (i = 0; i < lvp->numResPairs; i++) {
        if (!xdr_arrayElement(xdrs, (char *) &lvp->resPairs[i], hdr, xdr_resPair)) {
            if (xdrs->x_op == XDR_DECODE) {
                freeResPairs (lvp->resPairs, i);
                resPairs = NULL;
                numResPairs = 0;
            }
            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "xdr_arrayElement");
            return false;
        }
    }
    if (xdrs->x_op == XDR_DECODE)
        numResPairs = lvp->numResPairs;
    return true;

}


static void
freeResPairs (struct resPair *resPairs, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        FREEUP (resPairs[i].name);
        FREEUP (resPairs[i].value);
    }
    FREEUP (resPairs);
}


static bool_t
xdr_resPair (XDR *xdrs, struct resPair *resPair, struct LSFHeader *hdr)
{
    if (!(xdr_var_string(xdrs, &resPair->name)
          && xdr_var_string(xdrs, &resPair->value)))
        return false;
    return true;

}


bool_t
xdr_loadmatrix(XDR *xdrs, int len, struct loadVectorStruct *lmp,
               struct LSFHeader *hdr)
{
    return true;
}


bool_t
xdr_masterReg(XDR *xdrs, struct masterReg *masterRegPtr, struct LSFHeader *hdr)
{
    char *sp1, *sp2;
    int i, size;

    sp1 = masterRegPtr->clName;
    sp2 = masterRegPtr->hostName;

    if (xdrs->x_op == XDR_DECODE) {
        sp1[0] = '\0';
        sp2[0] = '\0';
    }

    if ( !(xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
           xdr_string(xdrs, &sp2, MAXHOSTNAMELEN) &&
           xdr_int(xdrs,&masterRegPtr->flags) &&
           xdr_u_int(xdrs,&masterRegPtr->seqNo) &&
           xdr_int(xdrs,&masterRegPtr->checkSum) &&
           xdr_portno(xdrs, &masterRegPtr->portno) &&
           xdr_int(xdrs, &masterRegPtr->licFlag) &&
           xdr_int(xdrs, &masterRegPtr->maxResIndex))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        size = masterRegPtr->maxResIndex;
        masterRegPtr->resBitArray = (int *)malloc(size*sizeof(int));
    }
    for (i=0; i < masterRegPtr->maxResIndex; i++){
        if (!xdr_int(xdrs, &(masterRegPtr->resBitArray[i])))
            return false;
    }

    return true;

}


bool_t
xdr_masterAnnSLIMConf(XDR *xdrs,
                      struct masterAnnSLIMConf *masterAnnSLIMConfPtr,
                      struct LSFHeader *hdr)
{
    if (!(xdr_int(xdrs, &masterAnnSLIMConfPtr->flags) &&
          xdr_short(xdrs, &(masterAnnSLIMConfPtr->hostNo)))) {
        return false;
    }

    return true;

}


bool_t
xdr_statInfo(XDR *xdrs, struct statInfo *sip, struct LSFHeader *hdr)
{
    char *sp1, *sp2;

    sp1 = sip->hostType;
    sp2 = sip->hostArch;

    if (!( xdr_int(xdrs, &(sip->maxCpus)) &&
           xdr_int(xdrs, &(sip->maxMem)) &&
           xdr_int(xdrs, &(sip->nDisks)) &&
           xdr_portno(xdrs, &(sip->portno)) &&
           xdr_short(xdrs, &(sip->hostNo)) &&
           xdr_int(xdrs, &(sip->maxSwap)) &&
           xdr_int(xdrs, &(sip->maxTmp)) ))
        return false;


    if (xdrs->x_op == XDR_DECODE) {
        sp1[0]='\0';
        sp2[0]='\0';
    }

    if (!(xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
          xdr_string(xdrs, &sp2,  MAXLSFNAMELEN))) {
        return false;
    }
    return true;
}

#define MIN_FLOAT16  2.328306E-10
static u_short
encfloat16_(float f)
{
    int expo, mant;
    u_short result;
    double temp,fmant;

    temp = f;
    if (temp <= MIN_FLOAT16)
        temp = MIN_FLOAT16;
    if (temp >= INFINIT_LOAD)
        return((u_short)0x7fff);

    fmant = frexp(temp, &expo);

    if (expo < 0)
        expo =  0x20 | (expo & 0x1F);
    else
        expo = expo & 0x1F;

    fmant -= 0.5;
    fmant *= 2048;
    mant   = fmant;

    result = expo << 10;
    result = result + mant;
    return result;
}

static float
decfloat16_(u_short sf)
{
    int expo, mant;
    double fmant;
    float result;

    if (sf == 0x7fff)
        return INFINIT_LOAD;

    expo = (sf >> 10) & 0x0000003F;
    if (expo & 0x20)
        expo =  (expo & 0x1F) - 32;
    else
        expo = expo & 0x1F;
    mant = sf & 0x000003FF;
    fmant = (double)mant / 2048;
    fmant += 0.5;
    result = (float)ldexp(fmant, expo);
    if (result <= MIN_FLOAT16)
        result=0.0;
    return result;
}

bool_t
xdr_lvector(XDR *xdrs, float *li, int nIndices)
{
    u_short indx, temps;
    unsigned int *a;
    int i,j;

    if (!(a=(unsigned int *)malloc(allInfo.numIndx*sizeof(unsigned int))))
        return false;
    if (xdrs->x_op == XDR_ENCODE) {
        for (i=0, j=0; i < NBUILTINDEX; i=i+2) {
            indx = encfloat16_(li[i]);
            a[j] = indx << 16;
            if (i == nIndices - 1)
                indx = 0;
            else
                indx = encfloat16_(li[i+1]);
            a[j] = a[j] + indx;
            j++;
        }
    }
    for (i=0; i < (NBUILTINDEX/2+1); i++) {
        if (!xdr_u_int(xdrs, &a[i])) {
            FREEUP (a);
            return false;
        }
    }

    if (xdrs->x_op == XDR_DECODE) {
        for(i=0,j=0; i < NBUILTINDEX; i=i+2) {
            temps = (a[j] >> 16) & 0x0000ffff;
            li[i] = decfloat16_(temps);
            if (i == NBUILTINDEX - 1)
                break;
            a[j]  = a[j] << 16;
            temps = (a[j] >> 16) & 0x0000ffff;
            li[i+1] = decfloat16_(temps);
            j++;
        }
    }

    for (i=NBUILTINDEX; i<nIndices; i++) {
        if (! xdr_float(xdrs, &li[i])) {
            FREEUP (a);
            return false;
        }
    }

    FREEUP (a);
    return true;

}

bool_t
xdr_minSLimConfData(XDR *xdrs, struct minSLimConfData *sLimConfDatap, struct LSFHeader *hdr)
{
    static char fname[] = "xdr_minSLimConfData";
    int i, j, sharedResourceCnt;
    char *sp1;
    windows_t *windowsPtr;
    struct resItem *resTablePtr;
    struct sharedResourceInstance *tmp, *prevPtr;


    if (xdrs->x_op == XDR_FREE) {


        for (i = 0;i < sLimConfDatap->nClusAdmins; i++) {
            FREEUP(sLimConfDatap->clusAdminNames[i]);
        }

        for (i = 0; i < sLimConfDatap->myHost_numInstances; i++) {
            FREEUP(sLimConfDatap->myHost_instances[i]->resName);
            FREEUP(sLimConfDatap->myHost_instances[i]->value);
            FREEUP(sLimConfDatap->myHost_instances[i]);
        }
        FREEUP(sLimConfDatap->myHost_instances);

        while (sLimConfDatap->sharedResHead) {
            tmp = sLimConfDatap->sharedResHead;
            sLimConfDatap->sharedResHead = tmp->nextPtr;
            FREEUP(tmp->resName);
            FREEUP(tmp);
        }


        FREEUP(sLimConfDatap->myCluster_eLimArgs);
        FREEUP(sLimConfDatap->myHost_windows);

        FREEUP(sLimConfDatap->clusAdminIds);
        FREEUP(sLimConfDatap->clusAdminNames);
        FREEUP(sLimConfDatap->allInfo_resTable);

        for (i = 0; i < 8; i++ )
            FREEUP(sLimConfDatap->myHost_week[i]);
        FREEUP(sLimConfDatap->myHost_busyThreshold);
        sLimConfDatap->nClusAdmins = 0;

        return true;
    }


    if (xdrs->x_op == XDR_DECODE) {

        sLimConfDatap->nClusAdmins = 0;
        sLimConfDatap->myCluster_eLimArgs = NULL;
        sLimConfDatap->myHost_windows = NULL;
        sLimConfDatap->allInfo_nRes = 0;
        sLimConfDatap->allInfo_resTable = NULL;

        for (i = 0; i < 8; i++ ) {
            sLimConfDatap->numMyhost_weekpair[i] = 0;
            sLimConfDatap->myHost_week[i] = NULL;
        }

        sLimConfDatap->allInfo_numIndx = 0;
        sLimConfDatap->myHost_busyThreshold = NULL;

        sLimConfDatap->myHost_numInstances = 0;
        sLimConfDatap->myHost_instances = NULL;
        sLimConfDatap->sharedResHead = NULL;
    }

    if (!xdr_int(xdrs, &sLimConfDatap->defaultRunElim)) {
        return false;
    }
    if (!xdr_int(xdrs, &sLimConfDatap->nClusAdmins)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE && sLimConfDatap->nClusAdmins) {
        sLimConfDatap->clusAdminIds = (int *)calloc(sLimConfDatap->nClusAdmins,
                                                    sizeof(int));
        sLimConfDatap->clusAdminNames = (char **)calloc(
            sLimConfDatap->nClusAdmins, sizeof(char *));

        if (!sLimConfDatap->clusAdminIds || !sLimConfDatap->clusAdminNames) {
            sLimConfDatap->nClusAdmins=0;
            return false;
        }
    }
    for (i = 0;i < sLimConfDatap->nClusAdmins; i++) {
        if (!xdr_int(xdrs, &sLimConfDatap->clusAdminIds[i]) ||
            !xdr_var_string(xdrs, &sLimConfDatap->clusAdminNames[i])) {
            return false;
        }
    }
    if (!xdr_float(xdrs, &sLimConfDatap->exchIntvl) ||
        !xdr_float(xdrs, &sLimConfDatap->sampleIntvl) ||
        !xdr_short(xdrs, &sLimConfDatap->hostInactivityLimit) ||
        !xdr_short(xdrs, &sLimConfDatap->masterInactivityLimit) ||
        !xdr_short(xdrs, &sLimConfDatap->retryLimit) ||
        !xdr_short(xdrs, &sLimConfDatap->keepTime) ) {
        return false;
    }

    if (!xdr_int(xdrs, &sLimConfDatap->allInfo_nRes) ||
        !xdr_int(xdrs, &sLimConfDatap->allInfo_numIndx) ||
        !xdr_int(xdrs, &sLimConfDatap->allInfo_numUsrIndx) ) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE && (sLimConfDatap->allInfo_nRes - NBUILTINDEX)) {
        sLimConfDatap->allInfo_resTable = (struct resItem *)calloc(
            sLimConfDatap->allInfo_nRes - NBUILTINDEX, sizeof(struct resItem));
        if (!sLimConfDatap->allInfo_resTable) {
            return false;
        }
    }

    resTablePtr = sLimConfDatap->allInfo_resTable;
    for (i = 0; i < sLimConfDatap->allInfo_nRes - NBUILTINDEX; i++ ) {
        sp1= resTablePtr->name;
        if (xdrs->x_op == XDR_DECODE) {
            sp1[0] = '\0';
        }

        if (!xdr_string(xdrs, &sp1, MAXLSFNAMELEN) ||
            !xdr_int(xdrs, (int *)&resTablePtr->valueType) ||
            !xdr_int(xdrs, (int *)&resTablePtr->orderType) ||
            !xdr_int(xdrs, &resTablePtr->flags) ||
            !xdr_int(xdrs, &resTablePtr->interval) ) {
            return false;
        }

        resTablePtr++;
    }

    if (!xdr_u_short(xdrs, &sLimConfDatap->myCluster_checkSum) ||
        !xdr_var_string(xdrs, &sLimConfDatap->myCluster_eLimArgs) ) {
        return false;
    }

    if (!xdr_var_string(xdrs, &sLimConfDatap->myHost_windows) ||
        !xdr_time_t(xdrs, &sLimConfDatap->myHost_wind_edge) ||
        !xdr_int(xdrs, &sLimConfDatap->myHost_rexPriority) ||
        !xdr_int(xdrs, &sLimConfDatap->myHost_numInstances) ) {
        return false;
    }

    for (i = 0; i < 8; i++) {
        if (!xdr_int(xdrs, &sLimConfDatap->numMyhost_weekpair[i])) {
            return false;
        }
    }
    for (i = 0; i < 8; i++) {
        if (xdrs->x_op == XDR_DECODE && sLimConfDatap->numMyhost_weekpair[i]) {
            sLimConfDatap->myHost_week[i] = (windows_t *)malloc(
                sLimConfDatap->numMyhost_weekpair[i]*sizeof(windows_t));

            if ( sLimConfDatap->myHost_week[i] == NULL ) {
                return false;
            }

            for (j = 0; j < sLimConfDatap->numMyhost_weekpair[i] - 1; j++) {
                sLimConfDatap->myHost_week[i][j].nextwind =
                    &sLimConfDatap->myHost_week[i][j+1];
            }

            sLimConfDatap->myHost_week[i][j].nextwind = NULL;
        }

        windowsPtr = sLimConfDatap->myHost_week[i];
        for (j = 0; j < sLimConfDatap->numMyhost_weekpair[i]; j++) {
            if (windowsPtr == NULL) {
                return false;
            }

            if (!xdr_float(xdrs, &windowsPtr->opentime) ||
                !xdr_float(xdrs, &windowsPtr->closetime)) {
                return false;
            }
            windowsPtr = windowsPtr->nextwind;
        }
    }

    if (xdrs->x_op == XDR_DECODE && sLimConfDatap->allInfo_numIndx) {
        sLimConfDatap->myHost_busyThreshold =
            calloc(sLimConfDatap->allInfo_numIndx, sizeof(float));
        if (!sLimConfDatap->myHost_busyThreshold) {
            sLimConfDatap->allInfo_numIndx = 0;
            return false;
        }
    }
    for (i = 0; i < sLimConfDatap->allInfo_numIndx; i++) {
        if (!xdr_float(xdrs,&sLimConfDatap->myHost_busyThreshold[i]) ) {
            return false;
        }
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (sLimConfDatap->myHost_numInstances) {
            sLimConfDatap->myHost_instances =
                calloc(sLimConfDatap->myHost_numInstances,
                       (sizeof(struct resourceInstance *)));
            if (!sLimConfDatap->myHost_instances) {
                sLimConfDatap->myHost_numInstances = 0;
                return false;
            }
            for (i = 0; i < sLimConfDatap->myHost_numInstances; i++) {
                sLimConfDatap->myHost_instances[i]
                    = calloc(1, sizeof(struct resourceInstance));
                if (sLimConfDatap->myHost_instances[i] == NULL) {
                    return false;
                }
            }
        } else {
            sLimConfDatap->myHost_instances = NULL;
        }
    }

    for (i = 0; i < sLimConfDatap->myHost_numInstances; i++) {
        if (!xdr_var_string(xdrs,
                            &sLimConfDatap->myHost_instances[i]->resName) ||
            !xdr_var_string(xdrs,
                            &sLimConfDatap->myHost_instances[i]->value)) {
            return false;
        }
    }


    if (xdrs->x_op == XDR_ENCODE) {
        sharedResourceCnt = 0;
        for (tmp = sLimConfDatap->sharedResHead; tmp; tmp=tmp->nextPtr) {
            sharedResourceCnt++;
        }


        if (!xdr_int(xdrs, &sharedResourceCnt) ) {
            return false;
        }

        for (tmp = sLimConfDatap->sharedResHead; tmp; tmp=tmp->nextPtr) {
            if (!xdr_var_string(xdrs, &tmp->resName)) {
                return false;
            }
        }
    }

    if (xdrs->x_op == XDR_DECODE) {

        if (!xdr_int(xdrs, &sharedResourceCnt) ) {
            return false;
        }


        sLimConfDatap->sharedResHead = NULL;
        for (i = 0; i < sharedResourceCnt; i++) {
            tmp = (struct sharedResourceInstance *)
                calloc (1, sizeof(sharedResourceInstance));
            if (!tmp) {
                ls_syslog(LOG_ERR, I18N_FUNC_FAIL, fname, "malloc");
                return false;
            }
            if (!xdr_var_string(xdrs, &tmp->resName)) {
                return false;
            }
            tmp->nextPtr = NULL;

            if (i == 0) {
                sLimConfDatap->sharedResHead = tmp;
                prevPtr = tmp;
            } else {
                prevPtr->nextPtr = tmp;
                prevPtr = tmp;
            }
        }
    }

    return true;
}
