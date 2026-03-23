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

#include "batch/lib/lsb.h"

bool xdr_submitReq(XDR *, struct submitReq *, void *);
bool xdr_submitMbdReply(XDR *, struct submitMbdReply *, void *);
bool xdr_signalReq(XDR *, struct signalReq *, void *);
bool xdr_lsbMsg(XDR *, struct lsbMsg *, void *);
bool xdr_controlReq(XDR *, struct controlReq *, void *);
bool xdr_infoReq(XDR *, struct infoReq *, void *);
bool xdr_parameterInfo(XDR *, struct parameterInfo *, void *);
bool xdr_userInfoEnt(XDR *, struct userInfoEnt *, void *);
bool xdr_userInfoReply(XDR *, struct userInfoReply *, void *);
bool xdr_hostInfoEnt(XDR *, struct hostInfoEnt *, void *);
bool xdr_hostDataReply(XDR *, struct hostDataReply *, void *);
bool xdr_queueInfoEnt(XDR *, struct queueInfoEnt *, void *);
bool xdr_queueInfoReply(XDR *, struct queueInfoReply *, void *);
bool xdr_jobInfoHead(XDR *, struct jobInfoHead *, void *);
bool xdr_jobInfoReply(XDR *, struct jobInfoReply *, void *);
bool xdr_jobInfoEnt(XDR *, struct jobInfoEnt *, void *);
bool xdr_jobInfoReq(XDR *, struct jobInfoReq *, void *);
bool xdr_jobPeekReq(XDR *, struct jobPeekReq *, void *);
bool xdr_jobPeekReply(XDR *, struct jobPeekReply *, void *);
bool xdr_jobMoveReq(XDR *, struct jobMoveReq *, void *);
bool xdr_jobSwitchReq(XDR *, struct jobSwitchReq *, void *);
bool xdr_groupInfoReply(XDR *, struct groupInfoReply *, void *);
bool xdr_groupInfoEnt(XDR *, struct groupInfoEnt *, void *);
bool xdr_migReq(XDR *, struct migReq *, void *);
bool xdr_xFile(XDR *, struct xFile *, void *);
bool xdr_modifyReq(XDR *, struct modifyReq *, void *);
bool xdr_lsbShareResourceInfoReply(XDR *, struct lsbShareResourceInfoReply *,
                                     void *);
bool xdr_runJobReq(XDR *, struct runJobRequest *, void *);
bool xdr_jobAttrReq(XDR *, struct jobAttrInfoEnt *, void *);

struct wire_sbd_register {
    char hostname[MAXHOSTNAMELEN];
    int num_jobs;
    struct wire_sbd_job *jobs;

};

#define WIRE_SBD_REGISTER_MAX_JOBS 4096

// LavaLite sbd register with mbd
struct wire_sbd_job {
    int64_t job_id;
    int32_t pid;
};

bool xdr_wire_sbd_register(XDR *, struct wire_sbd_register *);

// Job signal protocol from mbd to sbd
struct wire_job_sig_req {
    int64_t job_id;
    int32_t sig;
    int32_t flags;
};

bool xdr_wire_job_sig_req(XDR *, struct wire_job_sig_req *);

/* job file wire representation
 *
 * len: number of bytes on wire / disk (no trailing 0)
 * data: after decode, data[len] is guaranteed 0 for local convenience
 */
struct wire_job_file {
    uint32_t len;
    char   *data;
};

bool xdr_wire_job_file(XDR *, struct wire_job_file *);

struct wire_job_state {
    int64_t job_id;
    int state;
};
bool xdr_wire_job_state(XDR *xdrs, struct wire_job_state *);

struct wire_compact_notify {
    int32_t status;
    int64_t compact_time;
};
bool xdr_wire_compact_notify(XDR *, struct wire_compact_notify *);
