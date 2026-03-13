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

#define PACKET_HEADER_SIZE sizeof(struct protocol_header)

struct wire_load {
    char hostname[MAXHOSTNAMELEN];
    uint32_t status;
    uint32_t nidx;
    float li[LOAD_NIDX];
};

struct wire_master {
    char hostname[MAXHOSTNAMELEN];
    uint16_t tcp_port;
};

struct wire_cluster {
    char name[MAXHOSTNAMELEN];
    char admin[LL_BUFSIZ_64];
};

void init_pack_hdr(struct protocol_header *);
bool_t xdr_pack_hdr(XDR *, struct protocol_header *);
bool_t xdr_wire_load(XDR *, struct wire_load *);
bool_t xdr_master(XDR *, struct wire_master *);
bool_t xdr_cluster(XDR *, struct wire_cluster *);
