/* Copyright (C) LavaLite Contributors
 * GPL v2
 */
#pragma once

#include "base/lib/ll.protocol.h"

/*
 * auth - HMAC-SHA256 authentication for the lavalite protocol.
 *
 * The sender signs the whole encoded message, header and payload,
 * after encoding it. The receiver verifies the message as received,
 * before dispatching to any handler.
 * Responses from mbd to clients are not signed.
 *
 * Key file: $LL_CONF_DIR/auth.key
 *   32 raw random bytes, mode 0640, group server uid.
 *
 * Replay protection: server rejects headers older than AUTH_MAX_AGE seconds.
 */

#define AUTH_KEY_SIZE 32
#define AUTH_MAX_AGE 60

int auth_init(uint8_t, uint32_t);
void auth_set_required(int);
int auth_load_key(void);
int auth_generate_key(void);
int auth_sign_msg(char *, size_t);
int auth_verify_msg(char *, size_t);
