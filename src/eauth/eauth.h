/*
 *  Copyright (C) LavaLite Contributors
 *
 * External authentication header.
 */

#ifndef _EAUTH_
#define _EAUTH_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/random.h>
#include <openssl/hmac.h>

#define EAUTH_LBUFSIZ 256 // Large buffer size for signatures
#define EAUTH_MBUFSIZ 64  // Medium buffer size for user/host fields

// Number of items in the auth_token + the signature
#define EAUTH_NUM_TOKENS 7

/**
 * struct auth_token - Represents a parsed authentication token.
 * @user: Username string.
 * @uid: User ID.
 * @gid: Group ID.
 * @host: Hostname of origin.
 * @timestamp: Time the token was issued.
 * @nonce: Random nonce to prevent replay attacks.
 */
struct auth_token {
    char user[EAUTH_MBUFSIZ];   // For logging and audit
    uid_t uid;                  // For identity enforcement
    gid_t gid;                  // For group-based access
    char host[EAUTH_MBUFSIZ];   // For traceability
    time_t timestamp;           // For freshness
    char nonce[2 * EAUTH_MBUFSIZ + 1]; // Nonce for uniqueness and replay protection
};

// Signature (detached from token)
struct auth_signature {
    char signature[EAUTH_LBUFSIZ]; // For integrity
};

// API Prototypes


/**
 * fill_auth_token - Populate an auth_token with current process context.
 * @token: Pointer to an auth_token structure to be filled.
 *
 * This function sets the user, UID, GID, host, timestamp, and nonce fields
 * based on the calling process and system state.
 *
 * Returns:
 *   0 on success.
 *  -1 on failure.
 */
extern int fill_auth_token(struct auth_token *);

/**
 * sign_auth_token - Generate a detached HMAC signature for a token.
 * @token: Pointer to a populated auth_token structure.
 * @sig: Pointer to an auth_signature structure to receive the signature.
 *
 * This function computes an HMAC signature over the token fields using
 * a shared secret and stores the result in sig.
 *
 * Returns:
 *   0 on success.
 *  -1 on failure.
 */
extern int sign_auth_token(const struct auth_token *,
                           struct auth_signature *);

/**
 * serialize_auth - Format token and signature into a single payload string.
 * @payload: Output buffer to receive the serialized string.
 * @token: Pointer to a populated auth_token structure.
 * @sig: Pointer to a populated auth_signature structure.
 *
 * This function serializes the token and its signature into a colon-delimited
 * string suitable for transmission or storage.
 *
 * Returns:
 *   0 on success.
 *  -1 on failure.
 */
extern int serialize_auth(char *,
                          const struct auth_token *,
                          const struct auth_signature *);

/**
 * verify_auth_token - Validate a serialized token and its signature.
 * @payload: Input string containing the serialized token and signature.
 *
 * This function parses the payload, reconstructs the token, recomputes the
 * expected signature, and compares it to the received one. It also checks
 * token structure and optionally expiration.
 *
 * Returns:
 *   0 if the token is valid.
 *  -1 if verification fails.
 */
extern int verify_auth_token(char *);

/**
 * sanitize_payload - Remove trailing newline from a string.
 * @payload: Pointer to a null-terminated string, typically read via fgets().
 *
 * This function trims the newline character (if present) from the end of the string.
 * It is used to clean up input before parsing or signature verification.
 */
extern void sanitize_payload(char *);

/**
 * verify_user - Check if UID and GID in token exist on the host system.
 * @token: Pointer to a populated auth_token structure.
 *
 * This function verifies that the UID maps to a valid user, the username matches
 * the UID, and the GID maps to a valid group. It ensures the token refers to a
 * legitimate local identity.
 *
 * Returns:
 *   0 if UID and GID are valid.
 *  -1 if any check fails.
 */
extern int verify_user(struct auth_token *);

#endif // _EAUTH_
