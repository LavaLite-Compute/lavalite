/*
 * Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <assert.h>

#include "base/lib/auth.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.list.h"
#include "batch/lib/rpc.h"
#include "batch/mbd/mbd.h"

int mbd_sbd_register(XDR *xdrs, int32_t chan_id)
{
    struct wire_sbd_register reg;
    memset(&reg, 0, sizeof(struct wire_sbd_register));

    if (!xdr_wire_sbd_register(xdrs, &reg)) {
        LS_ERR("SBD_REGISTER decode failed");
        enqueue_header(chan_id, BATCH_SBD_REGISTER_ACK, EPROTO);
        free(reg.jobs);
        return -1;
    }

    char hostname[MAXHOSTNAMELEN];
    memcpy(hostname, reg.hostname, sizeof(hostname));
    hostname[sizeof(hostname) - 1] = 0;

    struct mbd_host *n = ll_hash_search(&host_name_hash, hostname);
    if (n == NULL) {
        LS_ERRX("register from unknown host %s", hostname);
        free(reg.jobs);
        chan_shutdown(chan_id);
        return -1;
    }

    assert(n->sbd_chan == -1);
    // Register the channel of this sbd
    n->sbd_chan = chan_id;

    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", chan_id);
    ll_hash_insert(&sbd_chan_hash, key, n, 0);

    struct wire_sbd_register reg_ack;
    memset(&reg_ack, 0, sizeof(struct wire_sbd_register));
    //build_sbd_run_list(host_data, &reg_ack);

    // good bye bits
    n->status = HOST_OK;

    LS_INFO("hostname=%s canon=%s addr=%s chan_fd=%d status=%d",
            hostname, n->net.name, n->net.addr, chan_id, n->status);

    struct protocol_header hdr;
    memset(&hdr, 0, sizeof(struct protocol_header));
    hdr.operation = BATCH_SBD_REGISTER_ACK;
    hdr.status = MBD_OK;

    if (auth_sign_header(&hdr) < 0) {
        LS_ERR("failed to sign header failed to register with mbd");
        chan_shutdown(chan_id);
        // unlink the channel with the host
        n->sbd_chan = -1;
        n->status = HOST_UNAVAIL;
        ll_hash_remove(&sbd_chan_hash, key);
        return -1;
    }

    enqueue_payload(chan_id, &hdr, &reg_ack,
                    LL_BUFSIZ_256, xdr_wire_sbd_register);

    free(reg.jobs);

    return 0;
}

int32_t mbd_sbd_route(struct mbd_host *n)
{
    int chan_id = n->sbd_chan;

    if (chan_has_error(chan_id)) {
        LS_DEBUG("channel=%d sbd from=%s closed connection",
                 chan_id, chan_addr_str(chan_id));
        mbd_sbd_disconnect(n);
        return -1;
    }

    return 0;
}

int mbd_sbd_disconnect(struct mbd_host *n)
{
    int chan_id = n->sbd_chan;

    LS_INFO("closing connection with on chan=%d host=%s addr=%s", chan_id,
            n->net.name, n->net.addr);

    n->sbd_chan = -1;
    n->status = HOST_UNAVAIL;
    char key[LL_BUFSIZ_32];
    snprintf(key, sizeof(key), "%d", chan_id);
    ll_hash_remove(&sbd_chan_hash, key);
    chan_shutdown(chan_id);

    // traverse the list of jobs running on this host
    // and set them to state unknown
    return 0;
}
