/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

bool_t xdr_beacon(XDR *xdrs, struct master_beacon *reg)
{
    if (!xdr_opaque(xdrs, reg->cluster, sizeof(reg->cluster)))
        return false;

    if (!xdr_opaque(xdrs, reg->hostname, sizeof(reg->hostname)))
        return false;

    if (! xdr_uint32_t(xdrs, &reg->host_no))
        return false;

    if (!xdr_uint16_t(xdrs, &reg->tcp_port))
        return false;

    return true;
}

bool_t xdr_wire_load_report(XDR *xdrs, struct wire_load *wl)
{
    uint32_t n;

    if (!xdr_uint32_t(xdrs, &wl->host_no))
        return false;

    n = w->nidx;
    if (!xdr_uint32_t(xdrs, &n))
        return false;

    if (xdrs->x_op == XDR_DECODE) {
        if (n > LOAD_NIDX)
            return false;
        w->nidx = n;
    }

    if (! xdr_vector(xdrs, (char *)w->li, w->nidx,
                     sizeof(float), (xdrproc_t)xdr_float))
        return false;

    return true;
}
