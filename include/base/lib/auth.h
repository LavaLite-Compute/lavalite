/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "base/lib/ll.protocol.h"

/*
 * auth - HMAC-SHA256 authentication for the lavalite protocol.
 *
 * Client signs the outgoing protocol_header before sending.
 * server verifies the header before dispatching to any handler.
 * Responses from server to client are not signed.
 *
 * Key file: $LL_CONF_DIR/auth.key
 *   32 raw random bytes, mode 0640, group server uid.
 *
 * Replay protection: server rejects headers older than AUTH_MAX_AGE seconds.
 */

#define AUTH_KEY_SIZE  32
#define AUTH_MAX_AGE   60

void auth_set_required(int);
int auth_load_key(void);
int auth_generate_key(void);
int auth_sign_header(struct protocol_header *);
int auth_verify_header(const struct protocol_header *);
