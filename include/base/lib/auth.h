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
 * Two keys, 32 bytes each, in $LL_CONF_DIR:
 *   ll.auth.key    client key: client requests to mbd.
 *                  root:lavalite 0640
 *   ll.daemon.key  daemon key: every mbd <-> sbd message.
 *                  lavalite:lavalite 0600, the mbd user and root only,
 *                  only on hosts running mbd or sbd.
 *
 * Replay protection: messages older than AUTH_MAX_AGE seconds are
 * rejected.
 */

#define AUTH_KEY_SIZE 32
#define AUTH_MAX_AGE 60

enum auth_key { AUTH_KEY_CLIENT = 0, AUTH_KEY_DAEMON = 1 };

int auth_init(uint8_t, uint32_t);
void auth_set_required(int);
int auth_load_key(int);
int auth_generate_key(int);
int auth_sign_msg(char *, size_t, int);
int auth_verify_msg(char *, size_t, int);
