
/* $Id: lib.xdrlim.c,v 1.8 2007/08/15 22:18:51 tmizan Exp $
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

static bool_t xdr_hostLoad (XDR *, struct hostLoad *, struct packet_header *, char *);
static bool_t xdr_placeInfo (XDR *, struct placeInfo *, struct packet_header *);
static bool_t xdr_shortLsInfo(XDR *, struct shortLsInfo *, struct packet_header *);
static bool_t xdr_resItem(XDR *, struct resItem *, struct packet_header *);
static void freeUpMemp (char *, int);
static bool_t xdr_lsResourceInfo (XDR *, struct  lsSharedResourceInfo *,
						 struct packet_header *);
static bool_t xdr_lsResourceInstance (XDR *, struct  lsSharedResourceInstance *,
					    struct packet_header *);
int sharedResConfigured_ = false;
static bool_t
xdr_placeInfo(XDR *xdrs, struct placeInfo *placeInfo, struct packet_header *hdr)
{
    char *sp;

    sp = placeInfo->hostName;
    if (xdrs->x_op == XDR_DECODE)
        sp[0] = '\0';
    if (!(xdr_string(xdrs, &sp, MAXHOSTNAMELEN) &&
          xdr_int(xdrs, &placeInfo->numtask))) {
            return false;
    }
    return true;
}

static bool_t
xdr_hostLoad (XDR *xdrs, struct hostLoad *loadVec, struct packet_header *hdr, char *cp)
{
    char *sp;
    int i;
    int *nIndicies = (int *)cp;

    sp = loadVec->hostName;
    if (xdrs->x_op == XDR_DECODE)
	    sp[0] = '\0';
    if (!xdr_string(xdrs, &sp, MAXHOSTNAMELEN)) {
            return false;
    }
    for (i = 0; i < 1 + GET_INTNUM (*nIndicies); i++) {
        if (!xdr_int(xdrs, (int *) &loadVec->status[i]))
            return false;
    }
    for (i=0; i<*nIndicies; i++) {
	if ( !xdr_float(xdrs, &loadVec->li[i]))
	    return false;
    }

    return true;
}

bool_t
xdr_decisionReq(XDR *xdrs, struct decisionReq *decisionReqPtr,
		struct packet_header *hdr)
{
    char *sp1 = decisionReqPtr->hostType;
    char *sp2 = decisionReqPtr->resReq;

    if (xdrs->x_op == XDR_DECODE) {
	decisionReqPtr->resReq[0] = '\0';
	decisionReqPtr->hostType[0] = '\0';
    }

    if (!(xdr_enum(xdrs, (int *) &decisionReqPtr->ofWhat) &&
        xdr_int(xdrs,&decisionReqPtr->options) &&
        xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
        xdr_int(xdrs, &decisionReqPtr->numHosts) &&
        xdr_string(xdrs, &sp2, MAXLINELEN) &&
        xdr_int(xdrs, &decisionReqPtr->numPrefs))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
	decisionReqPtr->preferredHosts =
	      (char **) calloc(decisionReqPtr->numPrefs, sizeof (char *));
        if (decisionReqPtr->preferredHosts == NULL)
	    return false;
    }

    if (! xdr_array_string(xdrs, decisionReqPtr->preferredHosts,
	       MAXHOSTNAMELEN, decisionReqPtr->numPrefs)) {
        if (xdrs->x_op == XDR_DECODE)
	    FREEUP(decisionReqPtr->preferredHosts);
	return false;
    }

    return true;

}

bool_t
xdr_placeReply(XDR *xdrs, struct placeReply *placeRepPtr, struct packet_header *hdr)
{
    int i, status=false;
    static  char  *memp;

    if (!xdr_int(xdrs,&placeRepPtr->numHosts)) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (memp)
            FREEUP(memp);
        placeRepPtr->placeInfo = (struct placeInfo *)
            malloc((int)placeRepPtr->numHosts * sizeof(struct placeInfo));
        if (!placeRepPtr->placeInfo)
            return false;
        memp = (char *) placeRepPtr->placeInfo;
    }

    for (i = 0; i < placeRepPtr->numHosts; i++) {
        status= xdr_arrayElement(xdrs,
			 (char *)&placeRepPtr->placeInfo[i],
			 hdr,
			 xdr_placeInfo);
        if (!status) {
            if (xdrs->x_op == XDR_DECODE)
                FREEUP(memp);
            return false;
        }
    }
    return true;
}

bool_t
xdr_loadReply(XDR *xdrs, struct loadReply *loadReplyPtr, struct packet_header *hdr)
{
    char *sp;
    int  i, status=true;
    static char *memp;

    if (!(xdr_int(xdrs, &loadReplyPtr->nEntry)
        && xdr_int(xdrs, &loadReplyPtr->nIndex))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        int matSize, hlSize, nameSize, vecSize, staSize;
        char *currp;

        FREEUP(memp);
        if (loadReplyPtr->indicies==NULL)
            loadReplyPtr->indicies=
              (char **)malloc(sizeof(char *)*(loadReplyPtr->nIndex+1));

        hlSize   = ALIGNWORD_(loadReplyPtr->nEntry * sizeof(struct hostLoad));
        vecSize  = loadReplyPtr->nIndex * sizeof(float);
        matSize  = ALIGNWORD_((loadReplyPtr->nEntry + 1) * vecSize);
        nameSize = ALIGNWORD_(loadReplyPtr->nIndex * MAXLSFNAMELEN);
        staSize = ALIGNWORD_((1 + GET_INTNUM(loadReplyPtr->nIndex))
				       * sizeof (int));
        loadReplyPtr->loadMatrix = (struct hostLoad *) malloc
	  (hlSize + matSize + nameSize + loadReplyPtr->nEntry * staSize);
        if (!loadReplyPtr->loadMatrix) {
            memp = NULL;
            return false;
        }

        memp = (char *) loadReplyPtr->loadMatrix;
        currp = (char *)(memp + hlSize);
        for (i=0; i < loadReplyPtr->nEntry; i++, currp += vecSize)
            loadReplyPtr->loadMatrix[i].li = (float *)currp;
        currp = (char *)(memp + hlSize + matSize);
        for (i=0; i < loadReplyPtr->nIndex; i++, currp += MAXLSFNAMELEN)
            loadReplyPtr->indicies[i] = currp;
        for (i=0; i < loadReplyPtr->nEntry; i++, currp += staSize)
            loadReplyPtr->loadMatrix[i].status = (int *) currp;
    }

    for (i=0; i < loadReplyPtr->nIndex; i++) {
        sp = loadReplyPtr->indicies[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN))
            return false;
    }
    loadReplyPtr->indicies[i] = NULL;

    for (i = 0; i < loadReplyPtr->nEntry; i++) {
        status = xdr_arrayElement(xdrs,
                                  (char *)&loadReplyPtr->loadMatrix[i],
                                  hdr,
			          xdr_hostLoad,
                                  (char *)&loadReplyPtr->nIndex);

        if (!status) {
            if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
        }
    }

    if (!xdr_int(xdrs, &loadReplyPtr->flags))
        return false;

    return true;
}

bool_t
xdr_jobXfer(XDR *xdrs, struct jobXfer *jobXferPtr, struct packet_header *hdr)
{
    int i, status=false;
    char *sp;
    static  char  *memp;

    sp = jobXferPtr->resReq;
    if (xdrs->x_op == XDR_DECODE)
	sp[0] = '\0';
    if (!(xdr_int(xdrs,&jobXferPtr->numHosts) &&
         xdr_string(xdrs, &sp, MAXLINELEN))) {
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (memp)
            FREEUP(memp);
        jobXferPtr->placeInfo = (struct placeInfo *)
            malloc((int)jobXferPtr->numHosts * sizeof(struct placeInfo));
        if (!jobXferPtr->placeInfo) {
            lserrno = LSE_MALLOC;
            return false;
        }
        memp = (char *) jobXferPtr->placeInfo;
    }

    for (i = 0; i < jobXferPtr->numHosts; i++) {
        status= xdr_arrayElement(xdrs,
			 (char *)&jobXferPtr->placeInfo[i],
                         hdr,
			 xdr_placeInfo);
        if (!status) {
            if (xdrs->x_op == XDR_DECODE)
                FREEUP(memp);
            return false;
        }
    }

    return true;
}

bool_t
xdr_hostInfoReply(XDR *xdrs, struct hostInfoReply *hostInfoReply,
			struct packet_header *hdr)
{
    int i, status;
    static struct shortHInfo *memp = NULL;

    if (!(xdr_int(xdrs, &hostInfoReply->nHost) &&
          xdr_int(xdrs, &hostInfoReply->nIndex)))
	return false;

    if (!xdr_shortLsInfo(xdrs, hostInfoReply->shortLsInfo, hdr)) {
        return false;
    }
    if (xdrs->x_op == XDR_DECODE) {
        int shISize, matSize, vecSize, resSize;
        char *currp;

	if (memp)
	    FREEUP(memp);

        shISize = ALIGNWORD_(hostInfoReply->nHost * sizeof(struct shortHInfo));
        vecSize = ALIGNWORD_(hostInfoReply->nIndex * sizeof(float));
        resSize = (GET_INTNUM(hostInfoReply->shortLsInfo->nRes)
			 + GET_INTNUM(hostInfoReply->nIndex)) * sizeof(int);
        matSize = ALIGNWORD_(hostInfoReply->nHost * (vecSize + resSize
			       + MAXLINELEN + 100));

	hostInfoReply->hostMatrix = (struct shortHInfo *) malloc(shISize + matSize);
	if (hostInfoReply->hostMatrix == NULL)
	    return false;

	memp =  hostInfoReply->hostMatrix;
	currp = (char*) memp + shISize;
        for (i=0; i < hostInfoReply->nHost; i++) {
            hostInfoReply->hostMatrix[i].busyThreshold =(float *) currp;
	    currp += vecSize;
            hostInfoReply->hostMatrix[i].resBitMaps =(int *) currp;
	    currp += resSize;
	    hostInfoReply->hostMatrix[i].windows = (char*) currp;
	    currp += MAXLINELEN;
        }
    }
    for (i=0; i<hostInfoReply->nHost;i++)  {
        status = xdr_arrayElement(xdrs,
		 (char *)&hostInfoReply->hostMatrix[i],
		 hdr, xdr_shortHInfo, (char *)&hostInfoReply->nIndex);
	if (!status) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
	    return false;
	}
    }

    for (i=0; i<hostInfoReply->nHost;i++)  {
        if (!xdr_int(xdrs, &(hostInfoReply->hostMatrix[i].maxMem))) {
            if (xdrs->x_op == XDR_DECODE)
                FREEUP(memp);
            return false;
        }
    }

    return true;

}

bool_t
xdr_shortHInfo(XDR *xdrs, struct shortHInfo *shortHInfo, struct packet_header *hdr,
char *nIndex)
{
    char window[MAXLINELEN];
    char *sp;
    char *sp1 = window;
    u_short tIndx, mIndx, mem, cpus;
    unsigned  int  a, b;
    int i;
    int *nIndicies = (int *)nIndex;

    sp = shortHInfo->hostName;
    if (xdrs->x_op == XDR_DECODE) {
	sp[0] = '\0';
	sp1[0] = '\0';
    }

    if (xdrs->x_op == XDR_ENCODE) {
        if (shortHInfo->hTypeIndx >= 0) {

            tIndx = MIN( LL_HOSTTYPE_MAX, shortHInfo->hTypeIndx);
        } else {
            tIndx = LL_HOSTTYPE_MAX;
        }
        tIndx &= 0x7FFF;
        if (shortHInfo->windows[0] != '-')
            tIndx |= 0x8000;
        if (shortHInfo->hModelIndx >= 0) {

            mIndx = MIN(LL_HOSTMODEL_MAX, shortHInfo->hModelIndx);
        } else {
            mIndx = LL_HOSTMODEL_MAX;
        }
        a = tIndx << 16;
	a &= 0xffffffff;
        a = a + mIndx;
        mem = shortHInfo->maxMem;
        cpus = shortHInfo->maxCpus;
        b = mem << 16;
	b &= 0xffffffff;
        b = b + cpus;
	if (logclass & LC_TRACE)
	    ls_syslog(LOG_DEBUG2, "xdr_shortHInfo: host <%s>  type = %d, model = %d",
		      shortHInfo->hostName, tIndx, mIndx);
    }

    if (!xdr_string(xdrs, &sp, MAXHOSTNAMELEN) ||
            !xdr_u_int(xdrs, &a) ||
            !xdr_u_int(xdrs, &b) ||
            !xdr_int(xdrs, &shortHInfo->resClass))
        return false;

    if ((xdrs->x_op == XDR_ENCODE) && (shortHInfo->windows[0] != '-')) {
        sp1 = shortHInfo->windows;
        if (!xdr_string(xdrs, &sp1, MAXLINELEN))
            return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if ( (a >> 16) & 0x8000) {
            if (!xdr_string(xdrs, &sp1, MAXLINELEN))
                return false;
        } else {
            sp1[0]='-';
            sp1[1]='\0';
        }
        strcpy (shortHInfo->windows, sp1);
        shortHInfo->hTypeIndx = (int) (a >> 16) & 0x7FFF;
        a = a << 16;

        a &= 0xffffffff;
        shortHInfo->hModelIndx = (int) (a >> 16);
        shortHInfo->maxMem     = (int) (b >> 16);
        b = b << 16;

        b &= 0xffffffff;
        shortHInfo->maxCpus     = (int) (b >> 16);
    }

    for(i=0; i < *nIndicies; i++)
        if (!xdr_float(xdrs, &shortHInfo->busyThreshold[i]))
            return false;

    if ( !xdr_int(xdrs, &shortHInfo->flags) ||
        !xdr_int(xdrs, &shortHInfo->rexPriority) ||
        !xdr_int(xdrs, &shortHInfo->nDisks) ||
        !xdr_int(xdrs, &shortHInfo->maxSwap) ||
        !xdr_int(xdrs, &shortHInfo->maxTmp) )
        return false;
    if (!xdr_int(xdrs, &shortHInfo->nRInt))
	return false;
    for (i = 0; i < shortHInfo->nRInt; i++) {
        if (!xdr_int(xdrs, &shortHInfo->resBitMaps[i]))
            return false;
    }
    if (shortHInfo->flags & HINFO_SHARED_RESOURCE) {
        sharedResConfigured_ = true;
    }

    return true;
}

static bool_t
xdr_shortLsInfo( XDR *xdrs, struct shortLsInfo *shortLInfo, struct packet_header *hdr)
{
    int i;
    static char *memp, *currp;
    char *sp;

    if (!xdr_int(xdrs, &shortLInfo->nRes) ||
        !xdr_int(xdrs, &shortLInfo->nTypes) ||
        !xdr_int(xdrs, &shortLInfo->nModels))
	return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (memp)
            free(memp);

        memp = malloc((shortLInfo->nRes + shortLInfo->nTypes +
                       shortLInfo->nModels) * MAXLSFNAMELEN +
		       shortLInfo->nRes *sizeof (char *));
        if (!memp) {
            return false;
        }
        currp = memp;
        shortLInfo->resName = (char **)currp;
	currp += shortLInfo->nRes *sizeof (char *);
        for (i=0; i < shortLInfo->nRes; i++, currp += MAXLSFNAMELEN)
            shortLInfo->resName[i] = currp;
        for (i=0; i < shortLInfo->nTypes; i++, currp += MAXLSFNAMELEN)
            shortLInfo->hostTypes[i] = currp;
        for (i=0; i < shortLInfo->nModels; i++, currp += MAXLSFNAMELEN)
            shortLInfo->hostModels[i] = currp;
    }

    for(i=0; i < shortLInfo->nRes; i++) {
        sp = shortLInfo->resName[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN))
            return false;
    }
    for(i=0; i < shortLInfo->nTypes; i++) {
        sp = shortLInfo->hostTypes[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN))
            return false;
    }

    for(i=0; i < shortLInfo->nModels; i++) {
        sp = shortLInfo->hostModels[i];
        if (xdrs->x_op == XDR_DECODE)
            sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN))
            return false;
    }

    for(i=0; i < shortLInfo->nModels; i++)
        if (!xdr_float(xdrs, &shortLInfo->cpuFactors[i]))
            return false;
    return true;
}

bool_t
xdr_limLock(XDR *xdrs, struct limLock *limLockPtr, struct packet_header *hdr)
{
    char *sp;

    sp = limLockPtr->lsfUserName;
    if (xdrs->x_op == XDR_DECODE) {
        sp[0] = '\0';
    }

    if (!xdr_int(xdrs, &limLockPtr->on)
          || !xdr_time_t(xdrs, &limLockPtr->time)
          || !xdr_int(xdrs, &limLockPtr->uid))
        return false;

    if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN)) {
        return false;
    }

    return true;
}

static bool_t
xdr_resItem(XDR *xdrs, struct resItem *resItem, struct packet_header *hdr)
{
    char *sp, *sp1;

    sp = resItem->des;
    sp1= resItem->name;
    if (xdrs->x_op == XDR_DECODE) {
	    sp[0] = '\0';
	    sp1[0] = '\0';
    }
    if (!xdr_string(xdrs, &sp1, MAXLSFNAMELEN) ||
        !xdr_string(xdrs, &sp, MAXRESDESLEN) ||
        !xdr_enum(xdrs, (int *)&resItem->valueType) ||
        !xdr_enum(xdrs, (int *)&resItem->orderType) ||
        !xdr_int(xdrs, &resItem->flags) ||
        !xdr_int(xdrs, &resItem->interval))
           return false;
   return true;

}

bool_t
xdr_lsInfo(XDR *xdrs, struct lsInfo *lsInfoPtr, struct packet_header *hdr)
{
    int i;
    char *sp = (char*)0;
    static char *memp;

    if (!xdr_int(xdrs, &lsInfoPtr->nRes))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
	FREEUP(memp);
        lsInfoPtr->resTable = (struct resItem *)
			     malloc(lsInfoPtr->nRes * sizeof(struct resItem));
        if(!lsInfoPtr->resTable)
            return false;
        memp = (char *)lsInfoPtr->resTable;
    }

    for (i=0;i<lsInfoPtr->nRes;i++) {
        if (!xdr_arrayElement(xdrs, (char *)&lsInfoPtr->resTable[i],
 			      hdr, xdr_resItem)) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}
    }

    if (!xdr_int(xdrs, &lsInfoPtr->nTypes)) {
	if (xdrs->x_op == XDR_DECODE)
	    FREEUP(memp);
        return false;
    }

    for (i=0;i<lsInfoPtr->nTypes;i++) {
        sp = lsInfoPtr->hostTypes[i];
	if (xdrs->x_op == XDR_DECODE)
	    sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN))  {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}
    }

    if (!xdr_int(xdrs, &lsInfoPtr->nModels)) {
	if (xdrs->x_op == XDR_DECODE)
	    FREEUP(memp);
        return false;
    }

    for (i=0;i<lsInfoPtr->nModels;i++) {
        sp = lsInfoPtr->hostModels[i];
	if (xdrs->x_op == XDR_DECODE)
	    sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN)) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}
    }

    if (!xdr_int(xdrs, &lsInfoPtr->numUsrIndx) ||
        !xdr_int(xdrs, &lsInfoPtr->numIndx)) {
	if (xdrs->x_op == XDR_DECODE)
	    FREEUP(memp);
	return false;
    }

    for (i=0; i < lsInfoPtr->nModels; i++)
        if (!xdr_float(xdrs, &lsInfoPtr->cpuFactor[i])) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}

    for (i = 0; i < lsInfoPtr->nModels; ++i) {
        sp = lsInfoPtr->hostArchs[i];
	if (xdrs->x_op == XDR_DECODE)
	    sp[0] = '\0';
        if (!xdr_string(xdrs, &sp, MAXLSFNAMELEN)) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}
    }

    for (i=0; i < lsInfoPtr->nModels; ++i) {
        if (!xdr_int(xdrs, &lsInfoPtr->modelRefs[i])) {
	    if (xdrs->x_op == XDR_DECODE)
		FREEUP(memp);
            return false;
	}
    }

    return true;

}

bool_t
xdr_masterInfo(XDR *xdrs, struct masterInfo *mInfoPtr, struct packet_header *hdr)
{
    char *sp;

    sp = mInfoPtr->hostName;
    if (xdrs->x_op == XDR_DECODE) {
	sp[0] = '\0';
    }

    if (!xdr_string(xdrs, &sp, MAXHOSTNAMELEN))
        return false;

    if (!xdr_address(xdrs, &mInfoPtr->addr))
	return false;

    if (!xdr_portno(xdrs, &mInfoPtr->portno))
	return false;

    return true;
}

bool_t
xdr_clusterInfoReq(XDR *xdrs, struct clusterInfoReq *clusterInfoReq,
	                       struct packet_header *hdr)
{
    char line[MAXLINELEN];
    char *sp = line;

    if (xdrs->x_op == XDR_DECODE) {
	if (! xdr_string(xdrs, &sp, MAXLINELEN))
	    return false;
	clusterInfoReq->resReq = putstr_(line);
	if (clusterInfoReq->resReq == NULL)
	    return false;
    } else {
	if (! xdr_string(xdrs, &clusterInfoReq->resReq, MAXLINELEN))
	    return false;
    }

    if (! xdr_int(xdrs, &clusterInfoReq->listsize)
	   || ! xdr_int(xdrs, &clusterInfoReq->options)) {
        if (xdrs->x_op == XDR_DECODE)
	    free(clusterInfoReq->resReq);
	return false;
    }

    if (clusterInfoReq->listsize && xdrs->x_op == XDR_DECODE) {
	clusterInfoReq->clusters = (char **) calloc(clusterInfoReq->listsize,
				      sizeof (char *));
	if (clusterInfoReq->clusters == NULL) {
	    free(clusterInfoReq->resReq);
	    return false;
        }
    }

    if (! xdr_array_string(xdrs, clusterInfoReq->clusters,
		      MAXLSFNAMELEN, clusterInfoReq->listsize)) {
	if (xdrs->x_op == XDR_DECODE) {
	    FREEUP(clusterInfoReq->resReq);
	    FREEUP(clusterInfoReq->clusters);
	}
	return false;
    }

    return true;

}

bool_t
xdr_clusterInfoReply(XDR *xdrs, struct clusterInfoReply *clusterInfoReply,
			   struct packet_header *hdr)
{
    int i;
    static char *memp = NULL;
    static int nClus = 0;

    if (!xdr_int(xdrs, &clusterInfoReply->nClus))
	return false;

     if (!xdr_arrayElement(xdrs, (char *)clusterInfoReply->shortLsInfo,
	   hdr, xdr_shortLsInfo))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (memp)
	    freeUpMemp(memp, nClus);
	memp = calloc(clusterInfoReply->nClus, sizeof (struct shortCInfo));
	if (memp == NULL) {
            nClus = 0;
	    return false;
        }
	clusterInfoReply->clusterMatrix = (struct shortCInfo *) memp;
    }

    for (i=0; i<clusterInfoReply->nClus; i++) {
	bool_t status = 0;
	status = xdr_arrayElement(xdrs,
		    (char *)&clusterInfoReply->clusterMatrix[i],
		    hdr,
		    xdr_shortCInfo);
	if (! status) {
	    if (xdrs->x_op == XDR_DECODE) {
                nClus = 0;
		freeUpMemp(memp,  i -1);
            }
            return false;
	}
    }
    if (xdrs->x_op == XDR_DECODE) {
        nClus = clusterInfoReply->nClus;
    }
    return true;

}

static void
freeUpMemp (char *memp, int nClus)
{
    int i, j;
    struct shortCInfo *clusterMatrix;

    clusterMatrix = (struct shortCInfo *) memp;
    for (i = 0; i < nClus; i++) {
	FREEUP (clusterMatrix[i].resBitMaps);
	FREEUP (clusterMatrix[i].hostTypeBitMaps);
	FREEUP (clusterMatrix[i].hostModelBitMaps);
        if (clusterMatrix[i].nAdmins > 0) {
            for (j = 0; j < clusterMatrix[i].nAdmins; j++)
                FREEUP (clusterMatrix[i].admins[j]);
            FREEUP (clusterMatrix[i].adminIds);
            FREEUP (clusterMatrix[i].admins);
        }
    }
    FREEUP (memp);

}

bool_t
xdr_shortCInfo(XDR *xdrs, struct shortCInfo *clustInfoPtr, struct packet_header *hdr)
{
    char *sp1, *sp2, *sp3;
    int i;

    sp1 = clustInfoPtr->clName;
    sp2 = clustInfoPtr->masterName;
    sp3 = clustInfoPtr->managerName;

    if (xdrs->x_op == XDR_DECODE) {
        sp1[0] = '\0';
        sp2[0] = '\0';
        sp3[0] = '\0';
        clustInfoPtr->nAdmins = 0;
        clustInfoPtr->adminIds = NULL;
        clustInfoPtr->admins = NULL;
    }

    if (! (xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
           xdr_string(xdrs, &sp2, MAXHOSTNAMELEN) &&
           xdr_string(xdrs, &sp3, MAXLSFNAMELEN) &&
           xdr_int(xdrs, &clustInfoPtr->status) &&
           xdr_int(xdrs, &clustInfoPtr->numServers) &&
           xdr_int(xdrs, &clustInfoPtr->numClients) &&
           xdr_int(xdrs, &clustInfoPtr->managerId) &&
           xdr_int(xdrs, &clustInfoPtr->resClass) &&
           xdr_int(xdrs, &clustInfoPtr->typeClass) &&
           xdr_int(xdrs, &clustInfoPtr->modelClass) &&
           xdr_int(xdrs, &clustInfoPtr->numIndx) &&
           xdr_int(xdrs, &clustInfoPtr->numUsrIndx) &&
           xdr_int(xdrs, &clustInfoPtr->usrIndxClass)))
        return false;

    if (!xdr_int(xdrs, &clustInfoPtr->nAdmins))
        return false;
    if (xdrs->x_op == XDR_DECODE && clustInfoPtr->nAdmins > 0) {
        clustInfoPtr->admins
               = (char **) malloc (clustInfoPtr->nAdmins * sizeof (char *));
        clustInfoPtr->adminIds
               = (int *)  malloc (clustInfoPtr->nAdmins * sizeof (int));
        if (clustInfoPtr->admins == NULL ||
                       clustInfoPtr->adminIds == NULL) {
            FREEUP (clustInfoPtr->adminIds);
            FREEUP (clustInfoPtr->admins);
            clustInfoPtr->nAdmins = 0;
            return false;
        }
    }
    for (i = 0; i < clustInfoPtr->nAdmins; i++) {
        if (!(xdr_var_string (xdrs, &clustInfoPtr->admins[i]) &&
              xdr_int(xdrs, &clustInfoPtr->adminIds[i])))
            return false;
    }
    if (!xdr_int(xdrs, &clustInfoPtr->nRes))
	return false;
    if (xdrs->x_op == XDR_DECODE && clustInfoPtr->nRes) {
	clustInfoPtr->resBitMaps = (int *)
	    malloc (GET_INTNUM(clustInfoPtr->nRes) *sizeof(int));
	if (clustInfoPtr->resBitMaps == NULL) {
	    clustInfoPtr->nRes = 0;
	    return false;
        }
    }
    for (i = 0; (i < clustInfoPtr->nRes && i < GET_INTNUM(clustInfoPtr->nRes)); i++) {
	if (!(xdr_int(xdrs, &clustInfoPtr->resBitMaps[i])))
	    return false;
    }

    if (!xdr_int(xdrs, &clustInfoPtr->nTypes))
        return false;
    if (xdrs->x_op == XDR_DECODE && clustInfoPtr->nTypes) {
        clustInfoPtr->hostTypeBitMaps = (int *)
           malloc (GET_INTNUM(clustInfoPtr->nTypes) *sizeof(int));
        if (clustInfoPtr->hostTypeBitMaps == NULL) {
      	    clustInfoPtr->nTypes = 0;
	    return false;
        }
    }

    for (i = 0;  (i < clustInfoPtr->nTypes && i < GET_INTNUM(clustInfoPtr->nTypes)); i++) {
        if (!(xdr_int(xdrs, &clustInfoPtr->hostTypeBitMaps[i])))
	    return false;
    }

    if (!xdr_int(xdrs, &clustInfoPtr->nModels))
        return false;
    if (xdrs->x_op == XDR_DECODE && clustInfoPtr->nModels) {
        clustInfoPtr->hostModelBitMaps = (int *)
	       malloc (GET_INTNUM(clustInfoPtr->nModels) *sizeof(int));
        if (clustInfoPtr->hostModelBitMaps == NULL) {
   	    clustInfoPtr->nModels = 0;
	    return false;
        }
    }
    for (i = 0; (i < clustInfoPtr->nModels && i < GET_INTNUM(clustInfoPtr->nModels)); i++) {
        if (!(xdr_int(xdrs, &clustInfoPtr->hostModelBitMaps[i])))
	    return false;
    }

    return true;

}

bool_t
xdr_cInfo(XDR *xdrs, struct cInfo *cInfo, struct packet_header *hdr)
{

    char *sp1, *sp2, *sp3;
    int i;

    sp1 = cInfo->clName;
    sp2 = cInfo->masterName;
    sp3 = cInfo->managerName;

    if (xdrs->x_op == XDR_DECODE) {
        sp1[0] = '\0';
        sp2[0] = '\0';
        sp3[0] = '\0';
        cInfo->nAdmins = 0;
        cInfo->adminIds = NULL;
        cInfo->admins = NULL;
    }

    if (! (xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
           xdr_string(xdrs, &sp2, MAXHOSTNAMELEN) &&
           xdr_string(xdrs, &sp3, MAXLSFNAMELEN) &&
           xdr_int(xdrs, &cInfo->status) &&
           xdr_int(xdrs, &cInfo->numServers) &&
           xdr_int(xdrs, &cInfo->numClients) &&
           xdr_int(xdrs, &cInfo->managerId) &&
           xdr_int(xdrs, &cInfo->resClass) &&
           xdr_int(xdrs, &cInfo->typeClass) &&
           xdr_int(xdrs, &cInfo->modelClass) &&
           xdr_int(xdrs, &cInfo->numIndx) &&
           xdr_int(xdrs, &cInfo->numUsrIndx) &&
           xdr_int(xdrs, &cInfo->usrIndxClass)))
        return false;

    if (!xdr_int(xdrs, &cInfo->nAdmins))
        return false;
    if (xdrs->x_op == XDR_DECODE && cInfo->nAdmins > 0) {
        cInfo->admins
               = (char **) malloc (cInfo->nAdmins * sizeof (char *));
        cInfo->adminIds
               = (int *)  malloc (cInfo->nAdmins * sizeof (int));
        if (cInfo->admins == NULL ||
                           cInfo->adminIds == NULL) {
            FREEUP (cInfo->adminIds);
            FREEUP (cInfo->admins);
            cInfo->nAdmins = 0;
            return false;
        }
    }
    for (i = 0; i < cInfo->nAdmins; i++) {
        if (!(xdr_var_string (xdrs, &cInfo->admins[i]) &&
              xdr_int(xdrs, &cInfo->adminIds[i])))
            return false;
    }

    if (!xdr_int(xdrs, &cInfo->nRes))
	return false;
    if (xdrs->x_op == XDR_DECODE && cInfo->nRes) {
	cInfo->resBitMaps = (int *)
	       malloc (GET_INTNUM(cInfo->nRes) *sizeof(int));
	if (cInfo->resBitMaps == NULL) {
	    cInfo->nRes = 0;
	    return false;
        }
    }
    for (i = 0; (i < cInfo->nRes && i < GET_INTNUM(cInfo->nRes)); i++) {
	if (!(xdr_int(xdrs, &cInfo->resBitMaps[i])))
	    return false;
    }

    if (cInfo->numIndx > 0 && xdrs->x_op == XDR_DECODE) {
	cInfo->loadIndxNames = (char **)calloc(cInfo->numIndx,
					       sizeof(char *));
	if (cInfo->loadIndxNames == NULL)
	    return false;
    }

    if (! xdr_shortLsInfo(xdrs, &(cInfo->shortInfo), hdr))
        return false;

    if (cInfo->numIndx > 0) {
	if (! xdr_array_string(xdrs, cInfo->loadIndxNames, MAXLSFNAMELEN,
				   cInfo->numIndx))
	{
	    FREEUP(cInfo->loadIndxNames);
	    return false;
	}
    }

    if (!xdr_int(xdrs, &cInfo->nTypes))
        return false;
    if (xdrs->x_op == XDR_DECODE && cInfo->nTypes) {
        cInfo->hostTypeBitMaps = (int *)
	       malloc (GET_INTNUM(cInfo->nTypes) *sizeof(int));
	if (cInfo->hostTypeBitMaps == NULL) {
	    cInfo->nTypes = 0;
	    return false;
        }
    }

    for (i = 0;  (i < cInfo->nTypes && i < GET_INTNUM(cInfo->nTypes)); i++) {
        if (!(xdr_int(xdrs, &cInfo->hostTypeBitMaps[i])))
	    return false;
    }

    if (!xdr_int(xdrs, &cInfo->nModels))
        return false;
    if (xdrs->x_op == XDR_DECODE && cInfo->nModels) {
	cInfo->hostModelBitMaps = (int *)
	       malloc (GET_INTNUM(cInfo->nModels) *sizeof(int));
	if (cInfo->hostModelBitMaps == NULL) {
	    cInfo->nModels = 0;
	    return false;
        }
    }
    for (i = 0; (i < cInfo->nModels && i < GET_INTNUM(cInfo->nModels)); i++) {
	if (!(xdr_int(xdrs, &cInfo->hostModelBitMaps[i])))
	    return false;
    }

    return true;

}

bool_t
xdr_resourceInfoReq(XDR *xdrs, struct  resourceInfoReq *resourceInfoReq,
			   struct packet_header *hdr)
{
    int i;

    if (xdrs->x_op == XDR_DECODE) {
	resourceInfoReq->hostName = NULL;
	resourceInfoReq->resourceNames = NULL;
	resourceInfoReq->numResourceNames = 0;
    }
    if (!(xdr_int(xdrs, &resourceInfoReq->numResourceNames)
	      && xdr_int(xdrs, &resourceInfoReq->options)))
	return false;

    if (xdrs->x_op == XDR_DECODE && resourceInfoReq->numResourceNames > 0) {
	if ((resourceInfoReq->resourceNames =
	     (char **) malloc (resourceInfoReq->numResourceNames * sizeof (char *))) == NULL) {
            lserrno = LSE_MALLOC;
	    return false;
        }

    }
    for (i = 0; i < resourceInfoReq->numResourceNames; i++) {
	if (!xdr_var_string(xdrs, &resourceInfoReq->resourceNames[i])) {
	    resourceInfoReq->numResourceNames = i;
	    return false;
        }
    }
    if (!xdr_var_string(xdrs, &resourceInfoReq->hostName))
	return false;

    if (xdrs->x_op == XDR_FREE && resourceInfoReq->numResourceNames > 0) {
	FREEUP(resourceInfoReq->resourceNames);
	resourceInfoReq->numResourceNames = 0;
    }
    return true;

}

bool_t
xdr_resourceInfoReply(XDR *xdrs, struct  resourceInfoReply *resourceInfoReply,
			   struct packet_header *hdr)
{
    int i, status;

    if (xdrs->x_op == XDR_DECODE) {
	resourceInfoReply->numResources = 0;
	resourceInfoReply->resources = NULL;
    }
    if (!(xdr_int(xdrs, &resourceInfoReply->numResources)
         && xdr_int(xdrs, &resourceInfoReply->badResource)))
	return false;

    if (xdrs->x_op == XDR_DECODE &&  resourceInfoReply->numResources > 0) {
        if ((resourceInfoReply->resources = (struct lsSharedResourceInfo *)
             malloc (resourceInfoReply->numResources
			* sizeof (struct lsSharedResourceInfo))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    for (i = 0; i < resourceInfoReply->numResources; i++) {
	status = xdr_arrayElement(xdrs,
		    (char *)&resourceInfoReply->resources[i],
		    hdr,
		    xdr_lsResourceInfo);
	if (! status) {
	    resourceInfoReply->numResources = i;
            return false;
        }
    }
    if (xdrs->x_op == XDR_FREE && resourceInfoReply->numResources > 0) {
	FREEUP(resourceInfoReply->resources);
	resourceInfoReply->numResources = 0;
    }
    return true;
}

static bool_t
xdr_lsResourceInfo (XDR *xdrs, struct  lsSharedResourceInfo *lsResourceInfo,
			   struct packet_header *hdr)
{
    int i, status;

    if (xdrs->x_op == XDR_DECODE) {
	lsResourceInfo->resourceName = NULL;
	lsResourceInfo->instances = NULL;
	lsResourceInfo->nInstances = 0;
    }
    if (!(xdr_var_string (xdrs, &lsResourceInfo->resourceName) &&
          xdr_int(xdrs, &lsResourceInfo->nInstances)))
        return false;

    if (xdrs->x_op == XDR_DECODE &&  lsResourceInfo->nInstances > 0) {
        if ((lsResourceInfo->instances = (struct lsSharedResourceInstance *)
             malloc (lsResourceInfo->nInstances
			 * sizeof (struct lsSharedResourceInstance))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    for (i = 0; i < lsResourceInfo->nInstances; i++) {
	status = xdr_arrayElement(xdrs,
		    (char *)&lsResourceInfo->instances[i],
		    hdr,
		    xdr_lsResourceInstance);
	if (! status) {
	    lsResourceInfo->nInstances = i;
            return false;
        }
    }
    if (xdrs->x_op == XDR_FREE && lsResourceInfo->nInstances > 0) {
	FREEUP (lsResourceInfo->instances);
	lsResourceInfo->nInstances = 0;
    }
    return true;
}

static bool_t
xdr_lsResourceInstance (XDR *xdrs, struct  lsSharedResourceInstance *instance,
			   struct packet_header *hdr)
{

    if (xdrs->x_op == XDR_DECODE) {
	instance->value = NULL;
	instance->hostList = NULL;
	instance->nHosts = 0;
    }
    if (!(xdr_var_string (xdrs, &instance->value) &&
          xdr_int(xdrs, &instance->nHosts)))
        return false;

    if (xdrs->x_op == XDR_DECODE &&  instance->nHosts > 0) {
        if ((instance->hostList = (char **)
		   malloc (instance->nHosts * sizeof (char *))) == NULL) {
            lserrno = LSE_MALLOC;
            return false;
        }
    }
    if (! xdr_array_string(xdrs, instance->hostList,
	       MAXHOSTNAMELEN, instance->nHosts)) {
        if (xdrs->x_op == XDR_DECODE) {
	    FREEUP(instance->hostList);
	    instance->nHosts = 0;
        }
	return false;
    }
    if (xdrs->x_op == XDR_FREE && instance->nHosts > 0) {
	FREEUP (instance->hostList);
	instance->nHosts = 0;
    }
    return true;
}
