/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

void master(void)
{
    proc_read();
    beacon_send();
}

void slave(void)
{
    proc_read();
    report_load();

    if (! is_master_candidate(me))
        return;

    current_master.inactivity++;
    if (current_master.inactivity <= me->host_no * MISSED_BEACON_TOLERANCE)
        return;

    LS_INFO("master=%s inactivity=%d promoting self",
            current_master.node ? current_master.node->host->name : "unknown",
            current_master.inactivity);
    current_master.node = me;
    current_master.inactivity = 0;
}

static void cluster_name(XDR *xdrs, struct sockaddr_in *from,
                         struct protocol_header *request_hdr)
{
    XDR xdrs2;

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_REPLY_CLUSTER_NAME;
    hdr.sequence = request_hdr->sequence;
    hdr.status = LIM_OK;

    char buf[2 * LL_BUFSIZ_64];
    memset(&buf, 0, sizeof(buf));
    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs2, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs2);
        return;
    }

    if (! xdr_string(&xdrs2, &lim_cluster.name, UINT_MAX)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs2);
        return;
    }

    if (chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs2), from) < 0) {
        LS_ERR("chan_send_dgram failed", sock_to_str(&from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}

static void master_name(XDR *xdrs, struct sockaddr_in *from,
                        struct protocol_header *request_hdr)
{
    XDR xdrs2;
    char buf[LL_BUFSIZ_64];
    enum limReplyCode limReplyCode;
    struct hostNode *masterPtr;
    struct protocol_header hdr;
    struct masterInfo masterInfo;

    memset((char *) &buf, 0, sizeof(buf));
    init_pack_hdr(&hdr);

    xdrmem_create(&xdrs2, buf, MSGSIZE, XDR_ENCODE);
    hdr.operation = LIM_REPLY_CLUSTER_NAME;
    hdr.sequence = request_hdr->sequence;
    hdr.status = LIM_OK;

    if (!xdr_pack_hdr(&xdrs2, &hdr)) {
        ls_syslog(LOG_ERR, "%s: xdr_pack_hdr() failed: %m", __func__);
        xdr_destroy(&xdrs2);
        return;
    }

    if (limReplyCode == LIME_NO_ERR) {
        masterPtr = myClusterPtr->masterKnown ? myClusterPtr->masterPtr
                                              : myClusterPtr->prevMasterPtr;
        // Fill the masterInfo structure
        strcpy(masterInfo.hostName, masterPtr->hostName);
        get_host_addrv4(masterPtr->v4_epoint, &masterInfo.addr);
        masterInfo.portno = masterPtr->statInfo.portno;

        if (!xdr_masterInfo(&xdrs2, &masterInfo, &hdr)) {
            ls_syslog(LOG_ERR, "%s: xdr_masterInfo() failed: %m", __func__);
            xdr_destroy(&xdrs2);
            return;
        }
    }

    int cc = chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs2), from);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s: chanWrite() failed: %m", __func__,
                  sockAdd2Str_(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}


int udp_message(void)
{
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));
    static char buf[BUFSIZ];
    int cc = chan_recv_dgram(lim_udp_chan, buf, sizeof(buf),
                             (struct sockaddr_storage *) &from, -1);
    if (cc < 0) {
        LS_ERR("error receiving data on lim_udp_chan=%d", lim_udp_chan);
        return -1;
    }

    if (from->sin_port != ntohs(lim_udp_port))
        LS_ERRX("source port is not lim=%s port=%d", addr_to_str(&from),
                ntohs(from.port));
        return;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_DECODE);
    struct protocol_header hdr;

    if (! xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERRX("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    switch (hdr.operation) {
    case LIM_GET_CLUSTER_NAME:
        cluster_name(&xdrs, &from, &hdr);
        break;
    case LIM_GET_MASTER_NAME:
        master_name(&xdrs, &from, &hdr);
        break;
    case LIM_MASTER_BEACON:
        master_beacon(&xdrs, &from, &hdr);
        break;
    case LIM_LOAD_REPORT:
        load_report(&xdrs, &from, &hdr);
        break;
    default:
        LS_ERRX("unknown request code=%d from=%s"
                hdr.operation, addr_to_str(&from));
        break;
    }

    xdr_destroy(&xdrs);

    return 0;
}

void beacon_send(void)
{
    struct master_beacon bcn;

    memset(&bcn, 0, sizeof(bcn));
    strncpy(bcn.cluster,  lim_cluster.name, sizeof(bcn.cluster) - 1);
    strncpy(bcn.hostname, me->host->name, sizeof(bcn.hostname) - 1);
    bcn.host_no = me->host_no;
    bcn.tcp_port = me->tcp_port;

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_MASTER_BEACON;
    hdr.sequence  = bcn.seqno;

    XDR xdrs;
    char buf[LL_BUFSIZ_256];
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (! xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("hdr encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    if (! xdr_master_beacon(&xdrs, &bcn)) {
        LS_ERR("master resigster encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    struct ll_list_ent *e;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;

        if (n == me)
            continue;

        if (n->load_report_missing >= MISSED_LOAD_REPORT_TOLLERANCE) {
            LS_INFO("slave=%s missing load ticks=%d",
                    n->host->name, n->load_report_missed);
            n->status = LIM_UNAVAIL;
        }

        // we just increase the counter here because this is master
        // timer operation, we set it to 0 when we got the report
        n->load_report_missing++;

        struct sockaddr_in to;
        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port   = htons(lim_udp_port);

        get_host_sinaddrv4(n->host, &to);

        if (chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs), &to) < 0) {
            LS_ERR("chan_send_dgram to=%s failed", addr_to_str(&to));
            xdr_destroy(&xdrs);
            return;
        }
    }

    xdr_destroy(&xdrs);
}
static void beacon_recv(XDR *xdrs,
                        struct sockaddr_in *from,
                        struct protocol_header *hdr)
{
    struct master_beacon bcn;
    if (!xdr_master_beacon(xdrs, &bcn)) {
        LS_ERR("beacon decode failed");
        return;
    }
    struct lim_node *n = ll_hash_search(&node_name_hash, bcn.hostname);
    if (n == NULL) {
        LS_ERR("got beacon from unknown host=%s", bcn.hostname);
        return;
    }

    if (!is_master_candidate(n)) {
        LS_ERR("beacon from non-candidate host=%s host_no=%d, ignoring",
               n->host->name, n->host_no);
        return;
    }
    LS_DEBUG("from=%s host=%s host_no=%u port=%u",
             addr_to_str(from), bcn.hostname, bcn.host_no, bcn.tcp_port);

    // sender has lower host_no: it is master, accept it
    if (me->host_no > n->host_no) {
        if (current_master.node != n)
            LS_INFO("new master=%s host_no=%d", n->host->name, n->host_no);
        n->tcp_port = bcn.tcp_port;
        current_master.node = n;
        current_master.inactivity = 0;
        return;
    }
    // sender has higher host_no: I should be master, reassert
    if (me->host_no < n->host_no) {
        LS_INFO("reasserting master role host_no=%d over=%s host_no=%d",
                me->host_no, n->host->name, n->host_no);
        current_master.node = me;
        current_master.inactivity = 0;
        return;
    }
    // same host_no: config error
    LS_ERRX("beacon from host_no=%d matching ours host=%s; config error",
            n->host_no, n->host->name);
}

int report_load(void)
{
    if (me == master_lim)
        return 0;

    if (! master_lim)
        LS_WARNING("master unknown, not sending load");
        return 0;
    }

    struct wire_load_report w;
    memset(&w, 0, sizeof(w));
    strcpy(w.hostname, me->host->name);
    w.nidx = LIM_NIDX;

    for (int i = 0; i < LIM_NIDX; i++)
        w.li[i] = myHostPtr->loadIndex[i];

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_LOAD_REPORT;
    hdr.sequence = 0;

    char buf[LL_BUFSIZ_256];
    XDR xdrs;
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("encode hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    if (!xdr_wire_load_update(&xdrs, &w)) {
        LS_ERR("load update encode failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    int len = xdr_getpos(&xdrs);
    struct sockaddr_in to_addr;
    memset(&to_addr, 0, sizeof(to_addr));
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(lim_udp_port);
    get_host_sinaddrv4(myClusterPtr->masterPtr->v4_epoint, &to_addr);

    LS_DEBUG("sending load to %s len=%d", sockAdd2Str_(&to_addr), len);

    if (chan_send_dgram(lim_udp_chan, buf, len, &to_addr) < 0) {
        LS_ERR("send to %s failed", sockAdd2Str_(&to_addr));
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);
    return 0;
}
