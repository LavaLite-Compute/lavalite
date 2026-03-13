/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"

struct wire_beacon {
    char cluster[LL_BUFSIZ_32];
    char  hostname[MAXHOSTNAMELEN];
    uint32_t hostNo;
    uint16_t tcp_port;
};

struct wire_load_report {
    char hostname[MAXHOSTNAMELEN];
    int host_no;
    int nidx;
    float li[LOAD_NIDX];
};

bool_t xdr_beacon(XDR *, struct wire_beacon *);
