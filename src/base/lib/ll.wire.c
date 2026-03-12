/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"
#include "base/lib/ll.channel.h"

static bool_t marshal_protocol_header(XDR *xdrs, struct protocol_header *header)
{
    if (!xdr_int32_t(xdrs, &header->sequence))
        return false;
    if (!xdr_int32_t(xdrs, &header->operation))
        return false;
    if (!xdr_int32_t(xdrs, &header->version))
        return false;
    if (!xdr_int32_t(xdrs, &header->length))
        return false;
    if (!xdr_int(xdrs, &header->status))
        return false;

    return true;
}

bool_t xdr_pack_hdr(XDR *xdrs, struct protocol_header *header)
{
    if (xdrs->x_op == XDR_ENCODE) {
        header->version = CURRENT_PROTOCOL_VERSION;
        if (!marshal_protocol_header(xdrs, header))
            return false;
        return true;
    }

    if (xdrs->x_op == XDR_DECODE) {
        if (!marshal_protocol_header(xdrs, header)) {
            return false;
        }
    }

    return true;
}

void init_pack_hdr(struct protocol_header *hdr)
{
    memset(hdr, 0, PACKET_HEADER_SIZE);
    hdr->version = CURRENT_PROTOCOL_VERSION;
}

void xdr_payload_free(bool_t (*xdr_func)(),
                      void *payload,
                      struct protocol_header *hdr)
{
    XDR xdrs;

    xdrmem_create(&xdrs, NULL, 0, XDR_FREE);

    (*xdr_func)(&xdrs, payload, hdr);

    xdr_destroy(&xdrs);
}
