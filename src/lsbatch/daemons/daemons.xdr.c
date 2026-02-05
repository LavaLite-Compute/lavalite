/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 *
 */

#include "lsbatch/daemons/daemons.h"

#define MAX_USER_NAME_LEN 64

extern void jobId64To32(int64_t, int *, int *);
extern void jobId32To64(int64_t *, int, int);

bool_t xdr_jobSpecs(XDR *xdrs, struct jobSpecs *spec, void *unused)
{
    int jobArrId = 0;
    int jobArrElemId = 0;
    int i;

    (void)unused;

    if (xdrs->x_op == XDR_DECODE) {
        spec->numToHosts = 0;
        spec->toHosts = NULL;

        spec->numEnv = 0;
        spec->env = NULL;

        spec->job_file_data.len = 0;
        spec->job_file_data.data = NULL;

        spec->eexec.len = 0;
        spec->eexec.data = NULL;
    }

    // jobId: 64 bit su struct, due int sul wire
    if (xdrs->x_op == XDR_ENCODE)
        jobId64To32(spec->jobId, &jobArrId, &jobArrElemId);

    if (!xdr_int(xdrs, &jobArrId))
        return false;
    if (!xdr_int(xdrs, &jobArrElemId))
        return false;

    if (xdrs->x_op == XDR_DECODE)
        jobId32To64(&spec->jobId, jobArrId, jobArrElemId);

    if (!xdr_opaque(xdrs, spec->jobName, LL_BUFSIZ_512))
        return false;

    if (!xdr_int(xdrs, &spec->jStatus))
        return false;
    if (!xdr_int(xdrs, &spec->reasons))
        return false;
    if (!xdr_int(xdrs, &spec->subreasons))
        return false;

    if (!xdr_int(xdrs, &spec->userId))
        return false;
    if (!xdr_opaque(xdrs, spec->userName, LL_BUFSIZ_64))
        return false;

    if (!xdr_int(xdrs, &spec->options))
        return false;
    if (!xdr_int(xdrs, &spec->jobPid))
        return false;
    if (!xdr_int(xdrs, &spec->jobPGid))
        return false;

    if (!xdr_opaque(xdrs, spec->queue, LL_BUFSIZ_64))
        return false;
    if (!xdr_int(xdrs, &spec->priority))
        return false;

    if (!xdr_opaque(xdrs, spec->fromHost, MAXHOSTNAMELEN))
        return false;

    if (!xdr_time_t(xdrs, &spec->startTime))
        return false;
    if (!xdr_int(xdrs, &spec->runTime))
        return false;

    // toHosts: numero + array di stringhe
    if (!xdr_int(xdrs, &spec->numToHosts))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (spec->numToHosts > 0) {
            spec->toHosts = calloc((size_t)spec->numToHosts, sizeof(char *));
            if (spec->toHosts == NULL)
                return false;
        } else {
            spec->toHosts = NULL;
        }
    }

    for (i = 0; i < spec->numToHosts; i++) {
        if (!xdr_string_raw(xdrs,
                            &spec->toHosts[i],
                            (uint32_t)(MAXHOSTNAMELEN - 1))) {
            return false;
        }
    }

    if (!xdr_int(xdrs, &spec->jAttrib))
        return false;
    if (!xdr_int(xdrs, &spec->sigValue))
        return false;

    if (!xdr_time_t(xdrs, &spec->termTime))
        return false;

    if (!xdr_opaque(xdrs, spec->subHomeDir, PATH_MAX))
        return false;
    if (!xdr_opaque(xdrs, spec->command, LL_BUFSIZ_512))
        return false;

    // job script: nome + blob inline
    if (!xdr_opaque(xdrs, spec->job_file, PATH_MAX))
        return false;

    if (!xdr_wire_job_file(xdrs, &spec->job_file_data))
        return false;

    // file I/O
    if (!xdr_opaque(xdrs, spec->inFile, PATH_MAX))
        return false;
    if (!xdr_opaque(xdrs, spec->outFile, PATH_MAX))
        return false;
    if (!xdr_opaque(xdrs, spec->errFile, PATH_MAX))
        return false;

    if (!xdr_int(xdrs, &spec->umask))
        return false;
    if (!xdr_opaque(xdrs, spec->cwd, PATH_MAX))
        return false;

    if (!xdr_time_t(xdrs, &spec->submitTime))
        return false;

    if (!xdr_opaque(xdrs, spec->preExecCmd, LL_BUFSIZ_512))
        return false;

    // environment: numEnv + array di stringhe
    if (!xdr_int(xdrs, &spec->numEnv))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (spec->numEnv > 0) {
            spec->env = calloc((size_t)spec->numEnv, sizeof(char *));
            if (spec->env == NULL)
                return false;
        } else {
            spec->env = NULL;
        }
    }

    for (i = 0; i < spec->numEnv; i++) {
        if (!xdr_string_raw(xdrs,
                            &spec->env[i],
                            (uint32_t)(LL_ENVVAR_MAX - 1))) {
            return false;
        }
    }

    // eexec blob
    if (!xdr_lenData(xdrs, &spec->eexec))
        return false;

    if (!xdr_opaque(xdrs, spec->projectName, LL_BUFSIZ_512))
        return false;

    if (!xdr_opaque(xdrs, spec->preCmd, LL_BUFSIZ_512))
        return false;
    if (!xdr_opaque(xdrs, spec->postCmd, LL_BUFSIZ_512))
        return false;

    // per XDR_FREE: a questo punto xdr_string_raw ha già liberato
    // ogni elemento di toHosts/env; qui liberiamo solo gli array.
    if (xdrs->x_op == XDR_FREE) {
        free(spec->toHosts);
        spec->toHosts = NULL;
        spec->numToHosts = 0;

        free(spec->env);
        spec->env = NULL;
        spec->numEnv = 0;
    }

    return true;
}

bool_t xdr_jobSig(XDR *xdrs, struct jobSig *jobSig, struct packet_header *hdr)
{
    static char *actCmd = NULL;
    int jobArrId, jobArrElemId;
    int newJobArrId, newJobArrElemId;

    jobArrId = jobArrElemId = newJobArrId = newJobArrElemId = 0;

    if (xdrs->x_op == XDR_DECODE)
        FREEUP(actCmd);
    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobSig->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) && xdr_int(xdrs, &(jobSig->sigValue)) &&
          xdr_time_t(xdrs, (time_t *) &(jobSig->chkPeriod)) &&
          xdr_int(xdrs, &(jobSig->actFlags)) &&
          xdr_int(xdrs, &(jobSig->reasons)) &&
          xdr_int(xdrs, &(jobSig->subReasons))))
        return FALSE;
    if (!xdr_var_string(xdrs, &jobSig->actCmd))
        return FALSE;
    if (xdrs->x_op == XDR_DECODE)
        actCmd = jobSig->actCmd;

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return FALSE;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobSig->jobId, jobArrId, jobArrElemId);
    }
    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobSig->newJobId, &newJobArrId, &newJobArrElemId);
    }
    if (!xdr_int(xdrs, &newJobArrId)) {
        return FALSE;
    }

    if (!xdr_int(xdrs, &newJobArrElemId)) {
        return FALSE;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobSig->newJobId, newJobArrId, newJobArrElemId);
    }
    return TRUE;
}

bool_t xdr_jobReply(XDR *xdrs, struct jobReply *jobReply,
                    struct packet_header *hdr)
{
    int jobArrId;
    int jobArrElemId;

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(jobReply->jobId, &jobArrId, &jobArrElemId);
    }
    if (!xdr_int(xdrs, &jobArrId))
        return false;

    if (!xdr_int(xdrs, &jobReply->jobPid))
        return false;

    if (!xdr_int(xdrs, &jobReply->jobPGid))
        return false;

    if (!xdr_int(xdrs, &jobReply->actPid))
        return false;

    if (!xdr_int(xdrs, &jobReply->jStatus))
        return false;

    if (!xdr_int(xdrs, &jobReply->reasons))
        return false;

    if (!xdr_int(xdrs, &jobReply->actValue))
        return false;

    if (!xdr_int(xdrs, &jobReply->actStatus))
        return false;

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return false;
    }
    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&jobReply->jobId, jobArrId, jobArrElemId);
    }
    return true;
}

bool_t xdr_statusReq(XDR *xdrs, struct statusReq *statusReq,
                     struct packet_header *hdr)
{
    int i;
    int jobArrId, jobArrElemId;

    if (xdrs->x_op == XDR_FREE) {
        for (i = 0; i < statusReq->numExecHosts; i++)
            FREEUP(statusReq->execHosts[i]);
        if (statusReq->numExecHosts > 0)
            FREEUP(statusReq->execHosts);
        statusReq->numExecHosts = 0;
        FREEUP(statusReq->execHome);
        FREEUP(statusReq->execCwd);
        FREEUP(statusReq->queuePreCmd);
        FREEUP(statusReq->queuePostCmd);
        FREEUP(statusReq->execUsername);
        if (statusReq->runRusage.npids > 0)
            FREEUP(statusReq->runRusage.pidInfo);
        if (statusReq->runRusage.npgids > 0)
            FREEUP(statusReq->runRusage.pgid);
        return TRUE;
    }

    if (xdrs->x_op == XDR_ENCODE) {
        jobId64To32(statusReq->jobId, &jobArrId, &jobArrElemId);
    }
    if (!(xdr_int(xdrs, &jobArrId) && xdr_int(xdrs, &statusReq->jobPid) &&
          xdr_int(xdrs, &statusReq->jobPGid) &&
          xdr_int(xdrs, &statusReq->actPid) && xdr_int(xdrs, &statusReq->seq) &&
          xdr_int(xdrs, &statusReq->newStatus) &&
          xdr_int(xdrs, &statusReq->reason) &&
          xdr_int(xdrs, &statusReq->subreasons) &&
          xdr_int(xdrs, (int *) &statusReq->sbdReply)))
        return FALSE;

    if (!xdr_array_element(xdrs, &statusReq->lsfRusage, NULL, xdr_lsfRusage))
        return FALSE;

    if (!(xdr_int(xdrs, &statusReq->execUid) &&
          xdr_int(xdrs, &statusReq->numExecHosts)))
        return FALSE;
    if (xdrs->x_op == XDR_DECODE) {
        if (statusReq->numExecHosts > 0) {
            statusReq->execHosts =
                (char **) calloc(statusReq->numExecHosts, sizeof(char *));
            if (!statusReq->execHosts)
                return FALSE;
        }
    }
    if (!xdr_array_string(xdrs, statusReq->execHosts, MAXHOSTNAMELEN,
                          statusReq->numExecHosts))
        return FALSE;
    if (!(xdr_int(xdrs, &statusReq->exitStatus) &&
          xdr_var_string(xdrs, &statusReq->execHome) &&
          xdr_var_string(xdrs, &statusReq->execUsername) &&
          xdr_var_string(xdrs, &statusReq->execCwd) &&
          xdr_var_string(xdrs, &statusReq->queuePreCmd) &&
          xdr_var_string(xdrs, &statusReq->queuePostCmd)))
        return FALSE;

    if (!xdr_int(xdrs, &statusReq->msgId))
        return FALSE;

    if (!(xdr_jRusage(xdrs, &(statusReq->runRusage), hdr)))
        return FALSE;

    if (!(xdr_int(xdrs, &(statusReq->sigValue)) &&
          xdr_int(xdrs, &(statusReq->actStatus))))
        return FALSE;

    if (!xdr_int(xdrs, &jobArrElemId)) {
        return FALSE;
    }

    if (xdrs->x_op == XDR_DECODE) {
        jobId32To64(&statusReq->jobId, jobArrId, jobArrElemId);
    }

    return TRUE;
}

bool_t xdr_sbdPackage(XDR *xdrs, struct sbdPackage *pkg,
                      struct packet_header *hdr)
{
    int i;

    (void)hdr;

    /* scalar fields in struct order */

    if (!xdr_int(xdrs, &pkg->managerId))
        return false;

    if (!xdr_opaque(xdrs, pkg->lsbManager, MAXLSFNAMELEN))
        return false;

    if (xdrs->x_op == XDR_DECODE)
        pkg->lsbManager[MAXLSFNAMELEN - 1] = 0;

    if (!xdr_int(xdrs, &pkg->mbdPid))
        return false;

    if (!xdr_int(xdrs, &pkg->sbdSleepTime))
        return false;

    if (!xdr_int(xdrs, &pkg->retryIntvl))
        return false;

    if (!xdr_int(xdrs, &pkg->preemPeriod))
        return false;

    if (!xdr_int(xdrs, &pkg->pgSuspIdleT))
        return false;

    if (!xdr_int(xdrs, &pkg->maxJobs))
        return false;

    if (!xdr_int(xdrs, &pkg->numJobs))
        return false;

    /* jobs array */

    switch (xdrs->x_op) {

    case XDR_ENCODE:
        for (i = 0; i < pkg->numJobs; i++) {
            if (!xdr_jobSpecs(xdrs, &pkg->jobs[i], hdr))
                return false;
        }
        break;

    case XDR_DECODE:
        // pkg->jobs is expected to be NULL on entry
        if (pkg->jobs != NULL) {
            LS_ERR("xdr_sbdPackage: DECODE called with non-NULL jobs");
            return false;
        }

        if (pkg->numJobs > 0) {
            pkg->jobs = calloc((size_t)pkg->numJobs,
                               sizeof(struct jobSpecs));
            if (pkg->jobs == NULL) {
                LS_ERR("calloc(%d, jobSpecs) failed", pkg->numJobs);
                pkg->numJobs = 0;
                return false;
            }

            for (i = 0; i < pkg->numJobs; i++) {
                memset(&pkg->jobs[i], 0, sizeof(struct jobSpecs));
                if (!xdr_jobSpecs(xdrs, &pkg->jobs[i], hdr)) {
                    int j;
                    for (j = 0; j < i; j++)
                        freeJobSpecs(&pkg->jobs[j]);
                    FREEUP(pkg->jobs);
                    pkg->jobs = NULL;
                    pkg->numJobs = 0;
                    return false;
                }
            }
        }
        break;

    case XDR_FREE:
        if (pkg->jobs != NULL && pkg->numJobs > 0) {
            for (i = 0; i < pkg->numJobs; i++)
                freeJobSpecs(&pkg->jobs[i]);
            FREEUP(pkg->jobs);
        }
        pkg->jobs = NULL;
        pkg->numJobs = 0;
        break;

    default:
        return false;
    }

    /* tail scalars */

    if (!xdr_int(xdrs, &pkg->uJobLimit))
        return false;

    if (!xdr_int(xdrs, &pkg->rusageUpdateRate))
        return false;

    if (!xdr_int(xdrs, &pkg->rusageUpdatePercent))
        return false;

    if (!xdr_int(xdrs, &pkg->jobTerminateInterval))
        return false;

    return true;
}

// LavaLite
bool_t xdr_job_status_ack(XDR *xdrs,
                          struct job_status_ack *ack,
                          struct packet_header *hdr)
{
    (void)hdr;

    if (xdrs == NULL || ack == NULL)
        return false;

    if (!xdr_int64_t(xdrs, &ack->job_id))
        return false;

    if (!xdr_int32_t(xdrs, &ack->seq))
        return false;

    if (!xdr_int32_t(xdrs, &ack->acked_op))
        return false;

    return true;
}

// mbd-sbd
bool_t xdr_sig_sbd_jobs(XDR *xdrs, struct xdr_sig_sbd_jobs *sj)
{
    if (!xdr_int32_t(xdrs, &sj->sig))
        return false;

    if (!xdr_int32_t(xdrs, &sj->flags))
        return false;

    if (!xdr_uint32_t(xdrs, &sj->n))
        return false;

    for (uint32_t i = 0; i < sj->n; i++) {
        uint64_t v;

        if (xdrs->x_op == XDR_ENCODE)
            v = (uint64_t)sj->job_ids[i];

        if (!xdr_uint64_t(xdrs, &v))
            return false;

        if (xdrs->x_op == XDR_DECODE)
            sj->job_ids[i] = (int64_t)v;
    }

    return true;
}

// sbd-mbd
bool_t xdr_wire_job_sig_reply(XDR *xdrs, struct wire_job_sig_reply *p)
{
    if (!xdr_int64_t(xdrs, &p->job_id))
        return false;
     if (!xdr_int32_t(xdrs, &p->sig))
         return false;
    if (!xdr_int32_t(xdrs, &p->rc))
        return false;
    if (!xdr_int32_t(xdrs, &p->detail_errno))
        return false;
    return true;
}
