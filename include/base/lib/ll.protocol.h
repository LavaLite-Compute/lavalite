/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include <rpc/types.h>
#include <rpc/xdr.h>

#define LL_PROTOCOL_VERSION 2
#define CURRENT_PROTOCOL_VERSION LL_PROTOCOL_VERSION

// For the  wire take this liberty
#define true 1
#define false 0

struct protocol_header {
    int32_t sequence;   /* request/response correlation */
    int32_t operation;  /* message type / opcode */
    int32_t version;    /* e.g. 2 */
    int32_t length;     /* payload bytes */
    int32_t status;     /* 0 ok, errno on error */
    uint32_t uid;       /* caller uid */
    uint32_t gid;       /* caller gid */
    uint32_t timestamp; /* unix time, replay protection */
    uint8_t hmac[32];   /* HMAC-SHA256 over header with hmac zeroed */
};

static const int32_t PACKET_HEADER_SIZE = sizeof(struct protocol_header);

void init_protocol_header(struct protocol_header *);
bool_t ll_encode_msg(XDR *, void *, bool_t (*)(), struct protocol_header *);
bool_t ll_encode_msg2(XDR *, struct protocol_header *, void *, bool_t (*)(),
                      void *, bool_t (*)());
bool_t xdr_pack_hdr(XDR *, struct protocol_header *);
