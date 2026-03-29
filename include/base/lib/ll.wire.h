/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

struct wire_load {
    char hostname[MAXHOSTNAMELEN];
    uint32_t status;
    float li[NUM_METRICS];
};

struct wire_loads {
    uint32_t nloads;
    struct wire_load *loads;
};

struct wire_master {
    char hostname[MAXHOSTNAMELEN];
};

struct wire_cluster {
    char name[MAXHOSTNAMELEN];
    char admin[LL_BUFSIZ_64];
};

struct wire_host {
    char hostname[MAXHOSTNAMELEN];
    char machine[LL_BUFSIZ_32];
    uint16_t is_candidate;
    uint64_t max_mem;
    uint64_t max_swap;
    uint64_t max_tmp;
    uint32_t num_cpus;
};

struct wire_hosts {
    uint32_t nhosts;
    struct wire_host *hosts;
};

void init_protocol_header(struct protocol_header *);
bool_t ll_encode_msg(XDR *, void *, bool_t (*)(), struct protocol_header *);
bool_t xdr_pack_hdr(XDR *, struct protocol_header *);

bool_t xdr_wire_master(XDR *, struct wire_master *);
bool_t xdr_wire_cluster(XDR *, struct wire_cluster *);

bool_t xdr_wire_load(XDR *, struct wire_load *);
bool_t xdr_wire_load_array(XDR *, struct wire_loads *);

bool_t xdr_wire_host(XDR *, struct wire_host *);
bool_t xdr_wire_host_array(XDR *, struct wire_hosts *);
