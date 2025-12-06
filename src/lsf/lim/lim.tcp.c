/*
 * lim_tcp.c - TCP client handling for LIM
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/lim/lim.h"

static void process_tcp_request(struct client_node *client)
{
    struct Buffer *buf;
    struct packet_header hdr;
    XDR xdrs;

    if (chan_dequeue(client->ch_id, &buf) < 0) {
        LS_ERR("chan_dequeue() failed");
        shutdown_client(client);
        return;
    }

    LS_DEBUG("Received %d bytes on chan %d", buf->len, client->ch_id);

    xdrmem_create(&xdrs, buf->data, buf->len, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        shutdown_client(client);
        return;
    }

    if (logclass & LC_TRACE)
        LS_DEBUG("operation %d", hdr.operation);

    // the client data structure is owned by this
    // layer so the callers should not free it
    switch (hdr.operation) {
    case LIM_LOAD_REQ:
        load_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    case LIM_GET_HOSTINFO:
        host_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    case LIM_GET_RESOUINFO:
        resource_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    case LIM_GET_INFO:
        info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    case LIM_PING:
        // Master takeover ping - no reply needed
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    case LIM_GET_CLUSINFO:
        clus_info_req(&xdrs, client, &hdr);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    default:
        LS_ERR("invalid operation %d", hdr.operation);
        xdr_destroy(&xdrs);
        shutdown_client(client);
        break;
    }
}

// Called from epoll loop when TCP client has data ready
int handle_tcp_client(int ch_id)
{
    // hash/list the ch_id it later if really needed
    for (int i = 0; i < chan_open_max; i++) {
        if (client_map[i] == NULL)
            continue;

        if (client_map[i]->ch_id != ch_id)
            continue;

        if ((channels[i].chan_events) == CHAN_EPOLLERR) {
            struct sockaddr_in addr;
            get_host_addrv4(client_map[i]->from_host->v4_epoint, &addr);
            LS_DEBUG("lost connection client %s closed so it is ok",
                     sockAdd2Str_(&addr));
            shutdown_client(client_map[i]);
            return -1;
        }
        process_tcp_request(client_map[i]);
    }

    return 0;
}
