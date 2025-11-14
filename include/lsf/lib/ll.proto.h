#pragma once
/*  Copyright (C) LavaLite Contributors
 */

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

struct packet_header {
    int32_t sequence;  // request/response correlation
    int32_t operation; // message type / opcode
    int32_t version;   // e.g. 0x00010000
    int32_t length;    // payload bytes
    int32_t reserved;  // future use (e.g., flags or error)
};

typedef struct packet_header LSFHeader;

/* On the wire: 5 × 4-byte XDR integers = 20 bytes.
 * PACKET_HEADER_SIZE matches wire size; any drift will fail the static assert.
 */
#define PACKET_HEADER_SIZE ((size_t) sizeof(struct packet_header))

_Static_assert(sizeof(struct packet_header) == 20, "packet_header size drift!");

// The procol buffers are marshaled use xdr
// Use these macros to compute the right size of
// the xdr buffer for its alignment ob 4 bytes boundary
