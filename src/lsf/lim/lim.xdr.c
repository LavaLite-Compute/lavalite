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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lim/lim.h"

static bool_t xdr_resPair(XDR *xdrs, struct resPair *resPair, void *ctx)
{
    if (!(xdr_var_string(xdrs, &resPair->name) &&
          xdr_var_string(xdrs, &resPair->value)))
        return false;
    return true;
}

static inline void freeResPairs(struct resPair *resPairs, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        free(resPairs[i].name);
        free(resPairs[i].value);
    }
    free(resPairs);
}

bool_t xdr_loadvector(XDR *xdrs, struct loadVectorStruct *lvp,
                      struct packet_header *hdr)
{
    static char fname[] = "xdr_loadvector";
    int i;
    static struct resPair *resPairs = NULL;
    static int numResPairs = 0;

    if (!(xdr_int(xdrs, &lvp->hostNo) && xdr_u_int(xdrs, &lvp->seqNo) &&
          xdr_int(xdrs, &lvp->numResPairs) && xdr_int(xdrs, &lvp->checkSum) &&
          xdr_int(xdrs, &lvp->flags) && xdr_int(xdrs, &lvp->numIndx) &&
          xdr_int(xdrs, &lvp->numUsrIndx))) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_int/xdr_u_int");
        return false;
    }

    if ((xdrs->x_op == XDR_DECODE) &&
        (genParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL)) {
        if (myClusterPtr->checkSum != lvp->checkSum) {
            if (genParams[LSF_LIM_IGNORE_CHECKSUM].paramValue == NULL) {
                ls_syslog(LOG_WARNING,
                          "%s: Sender has a different configuration", fname);
            }
        }

        if (allInfo.numIndx != lvp->numIndx ||
            allInfo.numUsrIndx != lvp->numUsrIndx) {
            ls_syslog(
                LOG_ERR,
                "%s: Sender has a different number of load index vectors. It "
                "will be rejected from the cluster by the master host.",
                fname);
            return false;
        }
    }

    for (i = 0; i < 1 + GET_INTNUM(lvp->numIndx); i++) {
        if (!xdr_int(xdrs, (int *) &lvp->status[i])) {
            ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_int");
            return false;
        }
    }

    if (!xdr_lvector(xdrs, lvp->li, lvp->numIndx)) {
        ls_syslog(LOG_ERR, "%s: %s failed: %m", fname, "xdr_lvector");
        return false;
    }

    if (xdrs->x_op == XDR_DECODE) {
        freeResPairs(resPairs, numResPairs);
        resPairs = NULL;
        numResPairs = 0;
        if (lvp->numResPairs > 0) {
            resPairs = (struct resPair *) malloc(lvp->numResPairs *
                                                 sizeof(struct resPair));
            if (resPairs == NULL) {
                lvp->numResPairs = 0;
                ls_syslog(LOG_ERR, "%s: %s(%d) failed: %m", fname, "malloc",
                          lvp->numResPairs * sizeof(struct resPair));
                return false;
            }
            lvp->resPairs = resPairs;
        } else
            lvp->resPairs = NULL;
    }
    for (i = 0; i < lvp->numResPairs; i++) {
        if (!xdr_array_element(xdrs, &lvp->resPairs[i], NULL, xdr_resPair)) {
            if (xdrs->x_op == XDR_DECODE) {
                freeResPairs(lvp->resPairs, i);
                resPairs = NULL;
                numResPairs = 0;
            }
            return false;
        }
    }
    if (xdrs->x_op == XDR_DECODE)
        numResPairs = lvp->numResPairs;

    return true;
}

bool_t xdr_loadmatrix(XDR *xdrs, int len, struct loadVectorStruct *lmp,
                      struct packet_header *hdr)
{
    return true;
}

bool_t xdr_masterReg(XDR *xdrs, struct masterReg *masterRegPtr,
                     struct packet_header *hdr)
{
    char *sp1;
    char *sp2;

    sp1 = masterRegPtr->clName;
    sp2 = masterRegPtr->hostName;

    if (xdrs->x_op == XDR_DECODE) {
        sp1[0] = 0;
        sp2[0] = 0;
    }

    if (!xdr_string(xdrs, &sp1, MAXLSFNAMELEN) ||
        !xdr_string(xdrs, &sp2, MAXHOSTNAMELEN) ||
        !xdr_int(xdrs, &masterRegPtr->flags) ||
        !xdr_u_int(xdrs, &masterRegPtr->seqNo) ||
        !xdr_int(xdrs, &masterRegPtr->checkSum) ||
        !xdr_portno(xdrs, &masterRegPtr->portno)) {
        return false;
    }

#if 0
    if (xdrs->x_op == XDR_DECODE) {
        size = masterRegPtr->maxResIndex;
        masterRegPtr->resBitArray = (int *)malloc(size*sizeof(int));
    }
    for (i=0; i < masterRegPtr->maxResIndex; i++){
        if (!xdr_int(xdrs, &(masterRegPtr->resBitArray[i])))
            return false;
    }
#endif
    return true;
}

bool_t xdr_masterAnnSLIMConf(XDR *xdrs,
                             struct masterAnnSLIMConf *masterAnnSLIMConfPtr,
                             struct packet_header *hdr)
{
    if (!(xdr_int(xdrs, &masterAnnSLIMConfPtr->flags) &&
          xdr_short(xdrs, &(masterAnnSLIMConfPtr->hostNo)))) {
        return false;
    }

    return true;
}

bool_t xdr_statInfo(XDR *xdrs, struct statInfo *sip, struct packet_header *hdr)
{
    char *sp1, *sp2;

    sp1 = sip->hostType;
    sp2 = sip->hostArch;

    if (!(xdr_int(xdrs, &(sip->maxCpus)) && xdr_int(xdrs, &(sip->maxMem)) &&
          xdr_int(xdrs, &(sip->nDisks)) && xdr_portno(xdrs, &(sip->portno)) &&
          xdr_short(xdrs, &(sip->hostNo)) && xdr_int(xdrs, &(sip->maxSwap)) &&
          xdr_int(xdrs, &(sip->maxTmp))))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        sp1[0] = '\0';
        sp2[0] = '\0';
    }

    if (!(xdr_string(xdrs, &sp1, MAXLSFNAMELEN) &&
          xdr_string(xdrs, &sp2, MAXLSFNAMELEN))) {
        return false;
    }
    return true;
}

bool_t xdr_lvector(XDR *xdrs, float *li, uint32_t nIndices)
{
    if (nIndices < 0)
        return false;
    if (nIndices == 0)
        return true;

    // XDR will encode/decode each float in IEEE754 big-endian, 4 bytes each.
    return xdr_vector(xdrs, (char *) li, (uint32_t) nIndices,
                      (uint32_t) sizeof(*li), (xdrproc_t) xdr_float);
}
