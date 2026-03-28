/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "batch/daemons/mbd.h"

static int sbd_disconnect(struct mbd_host *n)
{
    n->status = HOST_STAT_UNREACH;
    LS_INFO("closing connection with host=%s addr=%s state=%s", n->net.name,
            n->net.addr, hstat_to_str(n->status));

    struct jData *job;
    for (job = jDataList[SJL]->back; job != jDataList[SJL]; job = job->back) {

        if (job->hPtr[0] != host_node)
            continue;

         job->jStatus |= JOB_STAT_UNKNOWN;
         job->newReason = PEND_SBD_UNREACH;
         LS_DEBUG("job=%ld set to JOB_STAT_UNKNOW on addr=%s", job->jobId,
                  host_node->sbd_node->host.addr);
         log_newstatus(job);
    }

    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", client->chanfd);
    ll_hash_remove(&sbd_chan_hash, key);

    // hose the client
    chan_close(n->sbd_chan);
    n->sbd_chan = -1;

    return 0;
}

void sbd_register(XDR *xdrs, int32_t ch_id)
{
    struct wire_sbd_register reg;
    memset(&reg, 0, sizeof(struct wire_sbd_register));

    if (!xdr_wire_sbd_register(xdrs, &reg)) {
        LS_ERR("SBD_REGISTER decode failed");
        enqueue_header_reply(client->chanfd, LSBE_XDR);
        free(reg.jobs);
        return -1;
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, reg.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    struct mbd_host *n = ll_hash_search(&host_name_hash, hostname);
    if (host_data == NULL) {
        LS_ERRX("register from unknown host %s", hostname);
        free(reg.jobs);
        shutdown_chan(ch_id);
        return -1;
    }

    assert(n->sbd_chan == -1);
    n->sbd_chan = ch_id;

    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", ch_id);
    ll_hash_insert(&sbd_chan_hash, key, n, 0);

    struct wire_sbd_register reg_ack;
    memset(&reg_ack, 0, sizeof(struct wire_sbd_register));
    build_sbd_run_list(host_data, &reg_ack);

    // good bye bits
    n->status = HOST_STAT_OK;

    LS_INFO("hostname=%s canon=%s addr=%s chan_fd=%d status=%s",
            hostname, n->net.name, n->net.host.addr, hstat_to_str(n->status));

    struct protocol_header hdr;
    memset(&hdr, 0, sizeof(struct protocol_header));
    hdr.operation = BATCH_SBD_REGISTER_ACK;
    hdr.status = MBD_OK;
    enqueue_payload(ch_id, &hdr, &reg_ack, xdr_wire_sbd_register);

    free(reg.jobs);
}

int32_t sbd_route(struct mbd_host *n)
{
    if (n->sbd_chan < 0) {
        LS_ERR("mbd_dispatch_sbd called with NULL host_node (chanfd=%d)",
               client->chanfd);
        abort();
    }

    int ch_id = n->sbd_chan;
    // handle the exception on the channel
    if (channels[ch_id].chan_events == CHAN_EPOLLERR) {
        int err;
        socklen_t len = sizeof(err);
        int cc = getsockopt(channels[ch_id].sock, SOL_SOCKET,
                            SO_ERROR, &err, &len);
        LS_ERR("sbd shutdown chan=%d sock=%d so_error=%d ret=%d",
               ch_id, channels[ch_id].sock, err, cc);
        sbd_disconnect(n);
        return -1;
    }

    struct chan_buffer *buf;
    if (chan_dequeue(ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue failed for SBD chanfd=%d", ch_id);
        sbd_disconnect(n);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);

    struct protocol_header sbd_hdr;
    if (!xdr_pack_hdr(&xdrs, &sbd_hdr)) {
        LS_ERR("xdr_pack_hdr failed for SBD chanfd=%d", ch_id);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        mbd_sbd_disconnect(client);
        return -1;
    }

    LS_INFO("routing operation %s from %s", mbd_op_str(sbd_hdr.operation),
            host_node->host);

    // route to the appropriate handler
    switch (sbd_hdr.operation) {
    case BATCH_NEW_JOB_REPLY:
        new_job_reply(&xdrs, n);
        break;
    case BATCH_JOB_EXECUTE:
        set_job_status_execute(&xdrs, n);
        break;
    case BATCH_JOB_FINISH:
        set_job_status_finish(&xdrs, n);
        break;
    case BATCH_RUSAGE_JOB:
        // No ack for rusage
        set_rusage_update(&xdrs, n);
        break;
    case BATCH_JOB_SIGNAL_REPLY:
        job_signal_reply(&xdrs, n);
        break;
    case BATCH_JOB_UNKNOWN:
        job_state_unknown(&xdrs, n);
        break;
    default:
        LS_ERR("unexpected SBD protocol code=%d from host %s",
               sbd_hdr.operation, client->host.name);
        break;
    }

    xdr_destroy(&xdrs);
    chan_free_buf(buf);

    return 0;
}
