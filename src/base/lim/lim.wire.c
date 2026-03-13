/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lim/lim.h"

bool_t xdr_beacon(XDR *xdrs, struct wire_beacon *wb)
{
    if (!xdr_opaque(xdrs, wb->cluster, LL_BUFSIZ_32))
        return false;

    if (!xdr_opaque(xdrs, wb->hostname, MAXHOSTNAMELEN))
        return false;

    if (! xdr_uint32_t(xdrs, &wb->host_no))
        return false;

    if (!xdr_uint16_t(xdrs, &wb->tcp_port))
        return false;

    return true;
}

bool_t xdr_wire_load_report(XDR *xdrs, struct wire_load_report *wl)
{
    if (! xdr_opaque(xdrs, wl->hostname, MAXHOSTNAMELEN))
        return false;

    if (! xdr_uint32_t(xdrs, &wl->host_no))
        return false;

    if (! xdr_uint32_t(xdrs, &wl->num_metrics))
        return false;

    if (! xdr_vector(xdrs, (char *)wl->li, wl->num_metrics,
                     sizeof(float), (xdrproc_t)xdr_float))
        return false;

    return true;
}
