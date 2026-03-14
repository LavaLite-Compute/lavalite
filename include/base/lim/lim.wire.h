/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lib/ll.sys.h"
#include "base/lib/ll.wire.h"

struct wire_beacon {
    char cluster[LL_BUFSIZ_32];
    char  hostname[MAXHOSTNAMELEN];
    uint32_t host_no;
    uint16_t tcp_port;
};

struct  wire_load_report {
    char hostname[MAXHOSTNAMELEN];
    uint32_t host_no;
    uint32_t num_metrics;
    float li[NUM_METRICS];
};

bool_t xdr_beacon(XDR *, struct wire_beacon *);
bool_t xdr_wire_load_report(XDR *, struct wire_load_report *);
