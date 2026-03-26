// Copyright (C) LavaLite Contributors
// GPL v2
bool xdr_wire_compact_notify(XDR *xdrs, struct wire_compact_notify *msg)
{
    if (!xdrs || !msg)
        return false;

    if (! xdr_int32_t(xdrs, &msg->status))
        return false;
    if (! xdr_int64_t(xdrs, &msg->compact_time))
        return false;

    return true;
}
