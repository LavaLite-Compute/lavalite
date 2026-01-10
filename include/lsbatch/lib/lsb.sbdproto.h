/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 */

#pragma once

#include "lsf/lib/ll.sys.h"
#include "lsf/lib/ll.sysenv.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/ll.host.h"
#include "lsf/lib/lib.channel.h"
#include "lsbatch/lib/lsb.xdr.h"
#include "lsbatch/lib/lsb.h"      // jobSpecs, sbdReplyType
#include "lsf/lib/ll.list.h"      // struct ll_list, struct ll_list_entry
#include "lsf/lib/ll.hash.h"      // struct ll_hash


#pragma once
/*
 * sbatchd.protocol.h
 *
 * Sbatchd native RPC protocol.
 *
 * This header defines the wire-visible protocol between sbatchd and
 * sbatchd-aware client tools (e.g. sbjobs).
 *
 * Keep this header stable and daemon-agnostic:
 *  - no daemon-private structs
 *  - no pointers to internal objects
 *  - only XDR-safe types
 */


struct packet_header;

/*
 * sbatchd-native operations.
 * Values are in a private range, not shared with mbd/lsbatch protocol.
 */
typedef enum {
    SBD_JOBS_LIST = 200,
    /* future:
     * SBD_JOB_GET,
     * SBD_JOB_SIGNAL,
     */
} sbd_opcode_t;

/*
 * Request: list sbatchd jobs on a host.
 */
struct sbdJobsListReq {
    int32_t flags;   /* reserved, must be 0 */
};

/*
 * Job information returned by sbatchd.
 * This is a snapshot of sbatchd-local state, used for introspection.
 */
struct sbdJobInfo {
    int64_t job_id;

    int32_t pid;
    int32_t pgid;

    int32_t state;   /* enum sbd_job_state */
    int32_t step;    /* enum sbd_job_step */

    int32_t pid_acked;
    int32_t execute_acked;
    int32_t finish_acked;

    int32_t reply_sent;
    int32_t execute_sent;
    int32_t finish_sent;

    int32_t exit_status_valid;
    int32_t exit_status;

    int32_t missing;

    char *job_file;  /* specs->jobFile, optional */
};

/*
 * Reply: list of jobs.
 */
struct sbdJobsListReply {
    uint32_t jobs_len;
    struct sbdJobInfo *jobs_val;
};

/*
 * XDR routines.
 */
bool_t xdr_sbdJobsListReq(XDR *, struct sbdJobsListReq *,
                          struct packet_header *);

bool_t xdr_sbdJobInfo(XDR *, struct sbdJobInfo *,
                      struct packet_header *);

bool_t xdr_sbdJobsListReply(XDR *, struct sbdJobsListReply *,
                            struct packet_header *);

// API
int call_sbd_host(const char *, void *, size_t, char **,
                  struct packet_header *, struct lenData *);

int sbd_job_info(const char *, struct sbdJobInfo **, int *);
void sbd_job_info_free(struct sbdJobInfo *, int);
