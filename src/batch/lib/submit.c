/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rpc/xdr.h>
#include <time.h>
#include <limits.h>

#include "base/lib/auth.h"
#include "base/lib/ll.protocol.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "batch/lib/rpc.h"
#include "batch/lib/jobscript.h"
#include "batch/lib/wire.h"
#include "llbatch.h"

static int fill_wire(const struct job_submit *js, struct wire_job_submit *w)
{
    if (js->command == NULL) {
        errno = EINVAL;
        return -1;
    }

    struct passwd *pw;
    char hostname[MAXHOSTNAMELEN];
    memset(w, 0, sizeof(*w));

    if (js->name != NULL)
        ll_strlcpy(w->name, js->name, sizeof(w->name));
    if (js->queue != NULL)
        ll_strlcpy(w->queue, js->queue, sizeof(w->queue));
    if (js->project != NULL)
        ll_strlcpy(w->project, js->project, sizeof(w->project));
    if (js->comment != NULL)
        ll_strlcpy(w->comment, js->comment, sizeof(w->comment));
    if (js->machines != NULL)
        ll_strlcpy(w->machines, js->machines, sizeof(w->machines));
    if (js->in_file != NULL)
        ll_strlcpy(w->in_file, js->in_file, sizeof(w->in_file));
    if (js->out_file != NULL)
        ll_strlcpy(w->out_file, js->out_file, sizeof(w->out_file));
    if (js->err_file != NULL)
        ll_strlcpy(w->err_file, js->err_file, sizeof(w->err_file));
    if (js->depend_cond != NULL)
        ll_strlcpy(w->depend_cond, js->depend_cond, sizeof(w->depend_cond));
    if (js->gpu_type != NULL)
        ll_strlcpy(w->gpu_type, js->gpu_type, sizeof(w->gpu_type));

    ll_strlcpy(w->command, js->command, sizeof(w->command));

    w->num_cpus = js->num_cpus;
    if (w->num_cpus == 0)
        w->num_cpus = 1;

    w->num_nhosts = js->num_nhosts;
    if (w->num_nhosts == 0)
        w->num_nhosts = 1;

    w->num_gpus     = js->num_gpus;
    w->wall_seconds = js->wall_seconds;
    w->mem_mb       = js->mem_mb;
    w->storage_mb   = js->storage_mb;
    if (js->tokenpool != NULL)
        ll_strlcpy(w->tokenpool, js->tokenpool, sizeof(w->tokenpool));
    w->begin_time   = (int64_t)js->begin_time;
    w->term_time    = (int64_t)js->term_time;
    w->flags        = js->flags;

    w->uid   = (uint32_t)getuid();
    w->gid   = (uint32_t)getgid();
    w->umask = (uint32_t)umask(0);
    umask((mode_t)w->umask);

    pw = getpwuid2(w->uid);
    if (pw == NULL) {
        return -1;
    }
    ll_strlcpy(w->username, pw->pw_name, sizeof(w->username));
    ll_strlcpy(w->home_dir, pw->pw_dir, sizeof(w->home_dir));

    if (gethostname(hostname, sizeof(hostname)) < 0)
        return -1;
    ll_strlcpy(w->from_host, hostname, sizeof(w->from_host));

    if (getcwd(w->cwd, sizeof(w->cwd)) == NULL)
        return -1;

    return 0;
}

int32_t llb_submit(const struct job_submit *js, int64_t *job_id)
{
    struct wire_job_submit w;
    if (fill_wire(js, &w) < 0)
        return -1;

    struct wire_job_script script;
    memset(&script, 0, sizeof(script));
    if (create_jobscript(js, &script) < 0)
        return -1;
    /*
     * XDR encode buffer: wire_job_submit fixed fields + script payload.
     * Each xdr_opaque field carries a 4-byte length prefix [len][payload].
     * ~14 string fields * 4 = 56 bytes; 64 gives a small margin.
     */
#define XDR_OPAQUE_OVERHEAD 64
    size_t bufsz = PACKET_HEADER_SIZE + sizeof(struct wire_job_submit)
        + XDR_OPAQUE_OVERHEAD + script.len;
    char *buf = calloc(bufsz, 1);
    if (buf == NULL) {
        free(script.data);
        return -1;
    }
    struct protocol_header hdr;
    init_protocol_header(&hdr);
    hdr.operation = BATCH_JOB_SUBMIT;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        free(script.data);
        free(buf);
        errno = EPROTO;
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, (u_int)bufsz, XDR_ENCODE);
    if (!ll_encode_msg2(&xdrs, &hdr,
                        &w, (bool_t (*)())xdr_wire_job_submit,
                        &script, (bool_t (*)())xdr_wire_job_script)) {
        xdr_destroy(&xdrs);
        free(buf);
        free(script.data);
        errno = EPROTO;
        return -1;
    }
    size_t encoded = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);
    free(script.data);
    void *rbuf = NULL;
    struct protocol_header rhdr;
    memset(&rhdr, 0, sizeof(rhdr));
    if (call_mbd(buf, encoded, &rbuf, &rhdr) < 0) {
        free(buf);
        return -1;
    }
    free(buf);
    struct wire_job_submit_reply rep;
    memset(&rep, 0, sizeof(rep));
    XDR rxdrs;
    xdrmem_create(&rxdrs, rbuf, rhdr.length, XDR_DECODE);
    if (!xdr_wire_job_submit_reply(&rxdrs, &rep)) {
        xdr_destroy(&rxdrs);
        free(rbuf);
        errno = EPROTO;
        return -1;
    }
    xdr_destroy(&rxdrs);
    free(rbuf);
    if (rep.job_id <= 0) {
        errno = EPROTO;
        return -1;
    }
    *job_id = rep.job_id;
    return 0;
}
