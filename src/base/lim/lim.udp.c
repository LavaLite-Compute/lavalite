/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

static void send_beacon(void)
{
    struct wire_beacon wb;

    memset(&wb, 0, sizeof(wb));
    strncpy(wb.cluster,  lim_cluster.name, sizeof(wb.cluster) - 1);
    strncpy(wb.hostname, me->host->name, sizeof(wb.hostname) - 1);
    wb.host_no = me->host_no;
    wb.tcp_port = me->tcp_port;

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_MASTER_BEACON;
    hdr.status = LIM_OK;

    XDR xdrs;
    char buf[LL_BUFSIZ_256];
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (! xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("hdr encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    if (! xdr_beacon(&xdrs, &wb)) {
        LS_ERR("master resigster encode failed");
        xdr_destroy(&xdrs);
        return;
    }

    struct ll_list_entry *e;
    for (e = node_list.head; e; e = e->next) {
        struct lim_node *n = (struct lim_node *)e;

        if (n == me)
            continue;

        if (n->load_report_missing >= MISSED_LOAD_REPORT_TOLERANCE) {
            // this will keep logging
            LS_DEBUG("slave=%s missing load ticks=%d",
                     n->host->name, n->load_report_missing);
            n->status = LIM_STAT_CLOSED;
        }

        // we just increase the counter here because this is master
        // timer operation, we set it to 0 when we got the report
        n->load_report_missing++;

        struct sockaddr_in to;
        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_port   = htons(lim_udp_port);

        get_host_sinaddrv4(n->host, &to);

        LS_DEBUG("sending beacon to=%s", addr_to_str(&to));

        if (chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs), &to) < 0) {
            LS_ERR("chan_send_dgram to=%s failed", addr_to_str(&to));
            xdr_destroy(&xdrs);
            return;
        }
    }

    xdr_destroy(&xdrs);
}

static void rcv_beacon(XDR *xdrs,
                       struct sockaddr_in *from,
                       struct protocol_header *hdr)
{
    struct wire_beacon wb;
    if (!xdr_beacon(xdrs, &wb)) {
        LS_ERR("beacon decode failed");
        return;
    }
    struct lim_node *n = ll_hash_search(&node_name_hash, wb.hostname);
    if (n == NULL) {
        LS_ERR("got beacon from unknown host=%s", wb.hostname);
        return;
    }

    if (!is_master_candidate(n)) {
        LS_ERR("beacon from non-candidate host=%s host_no=%d, ignoring",
               n->host->name, n->host_no);
        return;
    }
    // check the host_no in the beacon is the same host_no we have
    if (n->host_no != wb.host_no) {
        LS_ERRX("host_no=%d of host=%s different from beacon hostno_no=%d",
                n->host_no, n->host->name, wb.host_no);
        return;
    }

    LS_DEBUG("from=%s host=%s host_no=%u port=%u",
             addr_to_str(from), wb.hostname, wb.host_no, wb.tcp_port);

    // sender has lower host_no: it is master, accept it
    if (me->host_no > n->host_no) {
        if (current_master.node != n)
            LS_INFO("new master=%s host_no=%d", n->host->name, n->host_no);
        n->tcp_port = wb.tcp_port;
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


static int send_load_report(void)
{
    if (! current_master.node) {
        LS_WARNING("master unknown, not sending load");
        return 0;
    }

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_LOAD_REPORT;

    char buf[LL_BUFSIZ_1K];
    XDR xdrs;
    xdrmem_create(&xdrs, buf, sizeof(buf), XDR_ENCODE);

    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("encode hdr failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    struct wire_load_report wl;
    memset(&wl, 0, sizeof(struct wire_load_report));
    strcpy(wl.hostname, me->host->name);
    wl.host_no = me->host_no;
    wl.num_metrics = NUM_METRICS;

    for (int i = 0; i < NUM_METRICS; i++)
        wl.li[i] = me->load_index[i];

    if (!xdr_wire_load_report(&xdrs, &wl)) {
        LS_ERR("load update encode failed");
        xdr_destroy(&xdrs);
        return -1;
    }

    size_t len = xdr_getpos(&xdrs);
    struct sockaddr_in to_addr;
    memset(&to_addr, 0, sizeof(to_addr));
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(lim_udp_port);
    get_host_sinaddrv4(current_master.node->host, &to_addr);

    LS_DEBUG("lim=%s len=%lu", current_master.node->host->name, len);

    if (chan_send_dgram(lim_udp_chan, buf, len, &to_addr) < 0) {
        LS_ERR("send to %s failed",  current_master.node->host->name);
        xdr_destroy(&xdrs);
        return -1;
    }

    xdr_destroy(&xdrs);
    return 0;
}

static void rcv_load_report(XDR *xdrs,
                            struct sockaddr_in *from,
                            struct protocol_header *hdr)
{
    struct wire_load_report wl;

    memset(&wl, 0, sizeof(wl));
    if (!xdr_wire_load_report(xdrs, &wl)) {
        LS_ERR("xdr_wire_load_report failed");
        return;
    }

    struct lim_node *n = ll_hash_search(&node_name_hash, wl.hostname);
    if (!n) {
        LS_WARNING("load report from unknown host %s", wl.hostname);
        return;
    }

    if (wl.host_no != n->host_no) {
        LS_WARNING("load report host_no mismatch host=%s wire=%d local=%d",
                   wl.hostname, wl.host_no, n->host_no);
        return;
    }

    if (wl.num_metrics < 0 || wl.num_metrics > NUM_METRICS) {
        LS_WARNING("invalid metric count from %s: %d",
            wl.hostname, wl.num_metrics);
        return;
    }

    for (int i = 0; i < wl.num_metrics; i++)
        n->load_index[i] = wl.li[i];

    n->load_report_missing = 0;
    n->status = LIM_STAT_OK;

    LS_DEBUG("load report updated host=%s metrics=%d",
             n->host->name, wl.num_metrics);
}

static void get_cluster_name(XDR *xdrs, struct sockaddr_in *from,
                             struct protocol_header *request_hdr)
{
    XDR xdrs2;

    struct protocol_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = LIM_REPLY_CLUSTER_NAME;
    hdr.sequence = request_hdr->sequence;
    hdr.status = LIM_OK;

    char buf[2 * LL_BUFSIZ_256];
    memset(&buf, 0, sizeof(buf));
    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs2, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs2);
        return;
    }

    struct wire_cluster wc;
    memset(&wc, 0, sizeof(struct wire_cluster));
    strcpy(wc.name, lim_cluster.name);
    strcpy(wc.admin, lim_cluster.admin);

    if (! xdr_wire_cluster(&xdrs2, &wc)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs2);
        return;
    }

    if (chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs2), from) < 0) {
        LS_ERR("chan_send_dgram to=%s failed", addr_to_str(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}

static void get_master_name(XDR *xdrs, struct sockaddr_in *from,
                            struct protocol_header *request_hdr)
{
    XDR xdrs2;
    char buf[LL_BUFSIZ_256];
    struct protocol_header hdr;

    memset((char *) &buf, 0, sizeof(buf));
    init_pack_hdr(&hdr);

    xdrmem_create(&xdrs2, buf, sizeof(buf), XDR_ENCODE);
    hdr.operation = LIM_REPLY_CLUSTER_NAME;
    hdr.sequence = request_hdr->sequence;
    hdr.status = LIM_OK;

    if (!xdr_pack_hdr(&xdrs2, &hdr)) {
        LS_ERRX("xdr_pack_hdr from=%s failed", addr_to_str(from));
        xdr_destroy(&xdrs2);
        return;
    }

    struct wire_master wm;
    memset(&wm, 0, sizeof(struct wire_master));
    if (current_master.node) {
        strcpy(wm.hostname, current_master.node->host->name);
        wm.tcp_port = current_master.node->tcp_port;
    } else {
        strcpy(wm.hostname, "unknown");
    }

    if (! xdr_wire_master(&xdrs2, &wm)) {
        LS_ERRX("xdr_wire_master from=%s failed", addr_to_str(from));
        xdr_destroy(&xdrs2);
        return;
    }

    int cc = chan_send_dgram(lim_udp_chan, buf, xdr_getpos(&xdrs2), from);
    if (cc < 0) {
        LS_ERR("chan_send_dgram to=%s failed", addr_to_str(from));
        xdr_destroy(&xdrs2);
        return;
    }

    xdr_destroy(&xdrs2);
}

void master(void)
{
    LS_DEBUG("running");

    read_proc();
    send_beacon();
}

void slave(void)
{
    LS_DEBUG("running");
    read_proc();
    send_load_report();

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

int udp_message(void)
{
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));

    char buf[LL_BUFSIZ_4K];
    int cc = chan_recv_dgram(lim_udp_chan, buf, sizeof(buf),
                             (struct sockaddr_storage *) &from, -1);
    if (cc < 0) {
        LS_ERR("error receiving data on lim_udp_chan=%d", lim_udp_chan);
        return -1;
    }

    if (from.sin_port != ntohs(lim_udp_port)) {
        LS_ERRX("source port is from=%s", addr_to_str(&from));
        return -1;
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
        // library
    case LIM_GET_CLUSTER_NAME:
        get_cluster_name(&xdrs, &from, &hdr);
        break;
    case LIM_GET_MASTER_NAME:
        get_master_name(&xdrs, &from, &hdr);
        break;
        // inter lim
    case LIM_LOAD_REPORT:
        rcv_load_report(&xdrs, &from, &hdr);
        break;
    case LIM_MASTER_BEACON:
        rcv_beacon(&xdrs, &from, &hdr);
        break;
    default:
        LS_ERRX("unknown request code=%d from=%s",
                hdr.operation, addr_to_str(&from));
        break;
    }

    xdr_destroy(&xdrs);

    return 0;
}
