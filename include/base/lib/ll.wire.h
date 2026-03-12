/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

/*
 * Protocol Version Encoding
 * =========================
 * Format: 0xMMmmPPbb
 *
 *   MM = Major version   (8 bits)
 *   mm = Minor version   (8 bits)
 *   PP = Patch version   (8 bits)
 *   bb = Build/reserved  (8 bits, usually 0)
 *
 * Examples:
 *   0.1.0  → 0x00010000
 *   0.2.0  → 0x00020000
 *   0.2.3  → 0x00020300
 *   1.0.0  → 0x01000000
 *   1.10.5 → 0x010A0500
 *
 * Update this when you bump the version in AC_INIT.
 */
#define PROTOCOL_VERSION 0x00010000
#define CURRENT_PROTOCOL_VERSION PROTOCOL_VERSION

// For the  wire take this liberty
#define true  1
#define false 0

struct protocol_header {
    int32_t sequence;  // request/response correlation
    int32_t operation; // message type / opcode
    int32_t version;   // e.g. 0x00010000
    int32_t length;    // payload bytes
    int32_t status;   // 0 ok, <0 error
};

/* On the wire: 5 × 4-byte XDR integers = 20 bytes.
 * PACKET_HEADER_SIZE matches wire size; any drift will fail the static assert.
 */
#define PACKET_HEADER_SIZE ((size_t) sizeof(struct protocol_header))

// The procol buffers are marshaled use xdr
// Use these macros to compute the right size of
// the xdr buffer for its alignment ob 4 bytes boundary
void init_pack_hdr(struct protocol_header *);
int send_protocol_header(int, struct protocol_header *);
int recv_protocol_header(int, struct protocol_header *);


// For ls_gethostinfo()
struct wire_host {
    char *host_name;
    char *host_type;
    char *host_model;
    float cpu_factor;
    int max_cpus;
    int max_mem;
    int max_swap;
    int max_tmp;
    int num_disks;
    int is_server;
    int status;
};

struct wire_host_reply {
    int num_hosts;
    struct wire_host *hosts;
};

// wire it the ll naming for bytes on the wire in network format
// host info
bool_t xdr_wire_host(XDR *, struct wire_host *,
                     struct protocol_header *);
bool_t xdr_wire_host_reply(XDR *, struct wire_host_reply *,
                           struct protocol_header *);
// host load info
// For ls_loadinfo()
struct wire_load {
    char *host_name;
    float load_indices[NLOAD_INDX]; // r15s r1m r15m ut pg io ls it tmp swp mem
    int status[NLOAD_INDX];         // status per index
};

struct wire_load_reply {
    int num_hosts;
    struct wire_load *hosts;
};

bool_t xdr_wire_load(XDR *, struct wire_load *);
bool_t xdr_wire_load_reply(XDR *, struct wire_load_reply *);


struct wire_host_type {
    char name[MAXHOSTNAMELEN];
};

// ls_clusterinfo()
// ls_clusterinfo()
struct wire_cluster_info {
    char cluster_name[MAXHOSTNAMELEN];
    int status;
    char master_name[MAXHOSTNAMELEN];
    char manager_name[MAXHOSTNAMELEN];
    int manager_id;
    int num_servers;
    int num_clients;
};
bool_t xdr_wire_cluster_info(XDR *, struct wire_cluster_info *);
bool_t xdr_pack_hdr(XDR *, struct protocol_header *);
