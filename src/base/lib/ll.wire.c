/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"
#include "base/lib/ll.channel.h"

bool_t ll_encode_msg(XDR *xdrs, void *payload,
                     xdrproc_t xdr_func,
                     struct protocol_header *hdr)
{
    xdr_setpos(xdrs, PACKET_HEADER_SIZE);

    if (payload && xdr_func) {
        if (!xdr_func(xdrs, payload))
            return false;
    }

    hdr->length = (int)(xdr_getpos(xdrs) - PACKET_HEADER_SIZE);

    xdr_setpos(xdrs, 0);
    if (!xdr_pack_hdr(xdrs, hdr))
        return false;

    xdr_setpos(xdrs, hdr->length + PACKET_HEADER_SIZE);
    return true;
}

void init_protocol_header(struct protocol_header *hdr)
{
    memset(hdr, 0, sizeof(struct protocol_header));
    hdr->version = CURRENT_PROTOCOL_VERSION;
}

bool_t xdr_pack_hdr(XDR *xdrs, struct protocol_header *hdr)
{
    if (!xdr_int32_t(xdrs, &hdr->sequence))
        return false;
    if (!xdr_int32_t(xdrs, &hdr->version))
        return false;
    if (!xdr_int32_t(xdrs, &hdr->operation))
        return false;
    if (!xdr_int32_t(xdrs, &hdr->length))
        return false;
    if (!xdr_int32_t(xdrs, &hdr->status))
        return false;
    return true;
}

void xdr_payload_free(bool_t (*xdr_func)(),
                      void *payload, struct protocol_header *hdr)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    (*xdr_func)(&xdrs, payload, hdr);

    xdr_destroy(&xdrs);
}

bool_t xdr_wire_hosts_array(XDR *xdrs, struct wire_host **hosts, uint32_t *n)
{
    return xdr_array(xdrs, (char **)hosts, n, INT32_MAX,
                     sizeof(struct wire_host), (xdrproc_t)xdr_wire_host);
}

bool_t xdr_wire_host(XDR *xdrs, struct wire_host *wh)
{
    if (! xdr_opaque(xdrs, wh->hostname, MAXHOSTNAMELEN))
        return false;
    if (! xdr_opaque(xdrs, wh->machine, LL_BUFSIZ_32))
        return false;
    if (! xdr_uint16_t(xdrs, &wh->is_candidate))
        return false;
    if (! xdr_uint64_t(xdrs, &wh->max_mem))
        return false;
    if (! xdr_uint64_t(xdrs, &wh->max_swap))
        return false;
    if (! xdr_uint64_t(xdrs, &wh->max_tmp))
        return false;
    if (! xdr_uint32_t(xdrs, &wh->num_cpus))
        return false;

    return true;
}

bool_t xdr_wire_load_array(XDR *xdrs, struct wire_load **hosts, uint32_t *n)
{
    return xdr_array(xdrs, (char **)hosts, n, INT32_MAX,
                     sizeof(struct wire_load), (xdrproc_t)xdr_wire_load);
}

bool_t xdr_wire_load(XDR *xdrs, struct wire_load *wl)
{
    if (! xdr_opaque(xdrs, wl->hostname, MAXHOSTNAMELEN))
        return false;
    if (! xdr_uint32_t(xdrs, &wl->status))
        return false;
    /* num_metrics is always NUM_METRICS on the wire; xdr_vector encodes
     * a fixed-length array so no count field is needed on the wire.
     */
    if (! xdr_vector(xdrs, (char *)wl->li, NUM_METRICS,
                    sizeof(float), (xdrproc_t) xdr_float))
        return false;

    return true;
}

bool_t xdr_wire_master(XDR *xdrs, struct wire_master *master)
{
    if (! xdr_opaque(xdrs, master->hostname, MAXHOSTNAMELEN))
        return false;

    return true;
}

bool_t xdr_wire_cluster(XDR *xdrs, struct wire_cluster *cl)
{
    if (! xdr_opaque(xdrs, cl->name, MAXHOSTNAMELEN))
        return false;
    if (! xdr_opaque(xdrs, cl->admin, LL_BUFSIZ_64))
        return false;
    return true;
}
