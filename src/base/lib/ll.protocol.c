/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <string.h>
#include <stdint.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/ll.bufsiz.h"

bool_t ll_encode_msg(XDR *xdrs, void *payload, bool_t (*xdr_func)(),
                     struct protocol_header *hdr)
{
    xdr_setpos(xdrs, PACKET_HEADER_SIZE);

    if (payload && xdr_func) {
        if (!xdr_func(xdrs, payload))
            return false;
    }

    hdr->length = (int) (xdr_getpos(xdrs) - PACKET_HEADER_SIZE);

    xdr_setpos(xdrs, 0);
    if (!xdr_pack_hdr(xdrs, hdr))
        return false;

    xdr_setpos(xdrs, hdr->length + PACKET_HEADER_SIZE);
    return true;
}

bool_t ll_encode_msg2(XDR *xdrs, struct protocol_header *hdr, void *payload,
                      bool_t (*xdr_func)(), void *payload2,
                      bool_t (*xdr_func2)())
{
    xdr_setpos(xdrs, PACKET_HEADER_SIZE);

    if (payload && xdr_func) {
        if (!xdr_func(xdrs, payload))
            return false;
    }

    if (payload2 && xdr_func2) {
        if (!xdr_func2(xdrs, payload2))
            return false;
    }

    hdr->length = (int) (xdr_getpos(xdrs) - PACKET_HEADER_SIZE);

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
    if (!xdr_uint32_t(xdrs, &hdr->uid))
        return false;
    if (!xdr_uint32_t(xdrs, &hdr->gid))
        return false;
    if (!xdr_uint32_t(xdrs, &hdr->timestamp))
        return false;
    if (!xdr_opaque(xdrs, (char *) hdr->hmac, sizeof(hdr->hmac)))
        return false;
    return true;
}
