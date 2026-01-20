/* $Id: lsb.xdr.h,v 1.11 2007/08/15 22:18:48 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#pragma once

#include "lsbatch/lib/lsb.h"

bool_t xdr_submitReq(XDR *, struct submitReq *, void *);
bool_t xdr_submitMbdReply(XDR *, struct submitMbdReply *, void *);
bool_t xdr_signalReq(XDR *, struct signalReq *, void *);
bool_t xdr_lsbMsg(XDR *, struct lsbMsg *, void *);
bool_t xdr_controlReq(XDR *, struct controlReq *, void *);
bool_t xdr_infoReq(XDR *, struct infoReq *, void *);
bool_t xdr_parameterInfo(XDR *, struct parameterInfo *, void *);
bool_t xdr_userInfoEnt(XDR *, struct userInfoEnt *, void *);
bool_t xdr_userInfoReply(XDR *, struct userInfoReply *, void *);
bool_t xdr_hostInfoEnt(XDR *, struct hostInfoEnt *, void *);
bool_t xdr_hostDataReply(XDR *, struct hostDataReply *, void *);
bool_t xdr_queueInfoEnt(XDR *, struct queueInfoEnt *, void *);
bool_t xdr_queueInfoReply(XDR *, struct queueInfoReply *, void *);
bool_t xdr_jobInfoHead(XDR *, struct jobInfoHead *, void *);
bool_t xdr_jobInfoReply(XDR *, struct jobInfoReply *, void *);
bool_t xdr_jobInfoEnt(XDR *, struct jobInfoEnt *, void *);
bool_t xdr_jobInfoReq(XDR *, struct jobInfoReq *, void *);
bool_t xdr_jobPeekReq(XDR *, struct jobPeekReq *, void *);
bool_t xdr_jobPeekReply(XDR *, struct jobPeekReply *, void *);
bool_t xdr_jobMoveReq(XDR *, struct jobMoveReq *, void *);
bool_t xdr_jobSwitchReq(XDR *, struct jobSwitchReq *, void *);
bool_t xdr_groupInfoReply(XDR *, struct groupInfoReply *, void *);
bool_t xdr_groupInfoEnt(XDR *, struct groupInfoEnt *, void *);
bool_t xdr_migReq(XDR *, struct migReq *, void *);
bool_t xdr_xFile(XDR *, struct xFile *, void *);
bool_t xdr_modifyReq(XDR *, struct modifyReq *, void *);
bool_t xdr_lsbShareResourceInfoReply(XDR *, struct lsbShareResourceInfoReply *,
                                     void *);
bool_t xdr_runJobReq(XDR *, struct runJobRequest *, void *);
bool_t xdr_jobAttrReq(XDR *, struct jobAttrInfoEnt *, void *);

// LavaLite sbd register with mbd
struct wire_sbd_register {
    char hostname[MAXHOSTNAMELEN];
};

bool_t xdr_wire_sbd_register(XDR *, struct wire_sbd_register *);

// Job signal protocol
struct wire_job_sig_req {
    int64_t job_id;
    int32_t sig;
    int32_t flags;
};

struct wire_job_sig_reply {
    int64_t job_id;
    int32_t rc;           // 0 ok, else errno-like or your enum
    int32_t detail_errno; // optional: errno from kill()
};

bool_t xdr_wire_job_sig_req(XDR *, struct wire_job_sig_req *);
bool_t xdr_wire_job_sig_reply(XDR *, struct wire_job_sig_reply *);

/* job file wire representation
 *
 * len: number of bytes on wire / disk (no trailing 0)
 * data: after decode, data[len] is guaranteed 0 for local convenience
 */
struct wire_job_file {
    int32_t len;
    char   *data;
};

bool_t xdr_wire_job_file(XDR *, struct wire_job_file *);
