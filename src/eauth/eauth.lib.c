/*
 * Copyright (C) LavaLite Contributors
 */

#include "eauth.h"

// Load the key from the environment
static const char *load_secret_key(void);
static const char *cached_secret_key;

/**
 * load_secret_key - Retrieves the secret key used for signing tokens.
 *
 * This function checks for the EAUTH_SECRET_KEY environment variable.
 * If found, it caches the key for future use. The key must be base64-encoded
 * and at least 44 characters long (32 bytes encoded).
 *
 * Returns a pointer to the key string on success, or NULL on failure.
 */
static const char *
load_secret_key(void)
{
    // Return cached key if already loaded
    if (cached_secret_key)
        return cached_secret_key;

    // Retrieve key from environment
    const char *key = getenv("EAUTH_SECRET_KEY");
    if (!key || strlen(key) == 0) {
        fprintf(stderr, "EAUTH_SECRET_KEY not set\n");
        return NULL;
    }

    // Optional: validate base64 format or length
    if (strlen(key) < 44) { // 32 bytes base64-encoded = ~44 chars
        fprintf(stderr, "Secret key too short or malformed\n");
        return NULL;
    }

    cached_secret_key = key;
    return key;
}

// Trims newline from input string read via fgets()
// Ensures clean payload before signature verification
void
sanitize_payload(char *payload)
{
    if (!payload)
        return;

    // Find first newline character and replace it with null terminator
    payload[strcspn(payload, "\n")] = '\0';
}

/**
 * fill_auth_token - Populates an auth_token with system identity and randomness.
 * @token: Pointer to the auth_token structure to fill.
 *
 * This function gathers user and host information, sets UID/GID,
 * captures the current timestamp, and generates a secure random nonce.
 *
 * Returns 0 on success, -1 on failure.
 */
int
fill_auth_token(struct auth_token *token)
{
    if (!token)
        return -1;

    // Zero out the structure and the array it contains
    memset(token, 0, sizeof(struct auth_token));

    // Get username
    struct passwd *pw = getpwuid(getuid());
    if (!pw) {
        return -1;
    }
    // Get the username
    strncpy(token->user, pw->pw_name, sizeof(token->user) - 1);
    token->user[sizeof(token->user) - 1] = 0;
    // Get UID and GID
    token->uid = pw->pw_uid;
    token->gid = pw->pw_gid;

    // Get hostname
    if (gethostname(token->host, sizeof(token->host)) < 0) {
        // Dont have hostname?
        return -1;
    }

    // Get timestamp
    token->timestamp = time(NULL);

    // Generate nonce using getrandom()
    unsigned char raw_nonce[EAUTH_MBUFSIZ / 2];
    if (getrandom(raw_nonce, sizeof(raw_nonce), 0) != sizeof(raw_nonce)) {
        perror("getrandom");
        return -1;
    }

    // Convert nonce to hex string
    for (size_t i = 0; i < sizeof(raw_nonce); i++) {
        sprintf(&token->nonce[i * 2], "%02x", raw_nonce[i]);
    }

    return 0;
}

/**
 * sign_auth_token - Signs an auth_token using HMAC-SHA256.
 * @token: Pointer to the filled auth_token structure.
 * @sig: Pointer to the auth_signature structure to populate.
 *
 * This function serializes the token fields, loads the secret key,
 * and computes an HMAC-SHA256 signature. The result is stored as a hex string.
 *
 * Returns 0 on success, -1 on failure.
 */
int
sign_auth_token(const struct auth_token *token,
                struct auth_signature *sig)
{
    if (!token || !sig)
        return -1;

    // Step 1: Serialize token fields (excluding signature)
    // The structure fields are separated by :
    char payload[2 * EAUTH_LBUFSIZ];
    memset(payload, 0, sizeof(payload));
    sprintf(payload, "%s:%d:%d:%s:%ld:%s",
            token->user,
            token->uid,
            token->gid,
            token->host,
            token->timestamp,
            token->nonce);
    printf("payload to sign: %s\n", payload);
    // Get the key which is common to all servers.
    const char *key = load_secret_key();
    if (!key)
        return -1;

    // Step 2: Generate HMAC-SHA256
    // Bug. make it thread safe.
    unsigned int len;
    unsigned char *digest = HMAC(EVP_sha256(),  // Hash function (SHA-256)
                                 key,  // Secret key
                                 strlen(key),
                                 (unsigned char *)payload, // Data to sign
                                 strlen(payload),
                                 NULL,
                                 &len);
    if (!digest || len == 0) {
        fprintf(stderr, "HMAC failed\n");
        return -1;
    }

    // Step 3: Convert to hex string
    for (unsigned int i = 0; i < len && i * 2 < sizeof(sig->signature) - 1; i++) {
        sprintf(&sig->signature[i * 2], "%02x", digest[i]);
    }
    sig->signature[len * 2] = 0;

    return 0;
}


/**
 * serialize_auth - Combines an auth_token and its signature into a single string.
 * @payload: Output buffer to hold the serialized result.
 * @token: Pointer to the filled auth_token structure.
 * @sig: Pointer to the generated auth_signature structure.
 *
 * Returns 0 on success, -1 on failure.
 */
int
serialize_auth(char *payload,
               const struct auth_token *token,
               const struct auth_signature *sig)
{
    if (!payload || !token || !sig)
        return -1;

    // Format the token and signature into a colon-delimited string.
    // This format is easy to parse and avoids ambiguity.
    // Order: user:uid:gid:host:timestamp:nonce:signature
    int written = snprintf(payload, EAUTH_LBUFSIZ * 2, "%s:%d:%d:%s:%ld:%s:%s",
                           token->user,      // Username
                           token->uid,       // User ID
                           token->gid,       // Group ID
                           token->host,      // Hostname
                           token->timestamp, // UNIX timestamp
                           token->nonce,     // Random nonce
                           sig->signature);  // HMAC signature

    printf("serialized signiture %s\n", sig->signature);

    // Check for buffer overflow or formatting error
    if (written <= 0 || written >= EAUTH_LBUFSIZ * 2) {
        fprintf(stderr, "failed to serialize auth payload\n");
        return -1;
    }

    return 0;
}

/**
 * verify_auth_token - Validates a serialized auth payload.
 * @payload: The received string containing token + signature.
 *
 * This function parses the payload, reconstructs the token,
 * recomputes the signature, and compares it to the received one.
 *
 * Returns 0 if valid, -1 if invalid or tampered.
 */
int
verify_auth_token(char *payload)
{
    if (!payload)
        return -1;

    //payload[strcspn(payload, "\n")] = 0;

    // Step 1: Copy payload to a mutable buffer
    char buf[EAUTH_LBUFSIZ * 2];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Step 2: Tokenize the payload
    // Expected format: user:uid:gid:host:timestamp:nonce:signature
    char *parts[EAUTH_NUM_TOKENS];
    char *tok = strtok(buf, ":");
    for (int i = 0; i < EAUTH_NUM_TOKENS && tok; i++) {
        parts[i] = tok;
        tok = strtok(NULL, ":");
    }

    // Validate field count
    if (!parts[6]) {
        fprintf(stderr, "Malformed payload\n");
        return -1;
    }

    // Step 3: Reconstruct token
    struct auth_token token;
    strncpy(token.user, parts[0], EAUTH_MBUFSIZ - 1);
    printf("user: %s\n", token.user);

    token.uid = atoi(parts[1]);
    printf("uid: %d\n", token.uid);

    token.gid = atoi(parts[2]);
    printf("gid: %d\n", token.gid);

    strncpy(token.host, parts[3], EAUTH_MBUFSIZ - 1);
    printf("host: %s\n", token.host);

    token.timestamp = atol(parts[4]);
    printf("timestamp: %ld\n", token.timestamp);
    // Bug. Check the expiration time, ttl.
    strncpy(token.nonce, parts[5], 2 * EAUTH_MBUFSIZ - 1);
    printf("nonce: %s\n", token.nonce);

    char payload2[2 * EAUTH_LBUFSIZ];
    memset(payload2, 0, sizeof(payload2));
    sprintf(payload2, "%s:%d:%d:%s:%ld:%s",
            token.user,
            token.uid,
            token.gid,
            token.host,
            token.timestamp,
            token.nonce);
    printf("payload to verify: %s\n", payload2);

    // Step 4: Recompute signature
    struct auth_signature expected_sig;
    if (sign_auth_token(&token, &expected_sig) != 0) {
        fprintf(stderr, "Failed to recompute signature\n");
        return -1;
    }
    printf("expected:%s\n", expected_sig.signature);
    printf("received:%s\n", parts[6]);
    printf("Expected length: %zu\n", strlen(expected_sig.signature));
    printf("Received length: %zu\n", strlen(parts[6]));

    // Step 5: Compare signatures
    if (strncmp(expected_sig.signature, parts[6],
                sizeof(expected_sig.signature)) != 0) {
        fprintf(stderr, "Signature mismatch\n");
        return -1;
    }

    return 0; // Token is valid
}

/**
 * verify_user - Validates that the UID and GID in the auth_token exist on the system.
 * @token: Pointer to a populated auth_token structure.
 *
 * This function checks:
 *   - That the UID maps to a valid user account.
 *   - That the username matches the UID.
 *   - That the GID maps to a valid group.
 *
 * Returns:
 *   0 if both UID and GID are valid and consistent.
 *  -1 if any check fails.
 */
int
verify_user(struct auth_token *token)
{
    if (!token) {
        fprintf(stderr, "Null token passed to verify_user\n");
        return -1;
    }

    // Lookup user by UID
    struct passwd *pw = getpwuid(token->uid);
    if (!pw) {
        fprintf(stderr, "UID %d not found\n", token->uid);
        return -1;
    }

    // Ensure username matches UID
    if (strncmp(pw->pw_name, token->user, EAUTH_MBUFSIZ) != 0) {
        fprintf(stderr, "Username mismatch for UID %d: expected %s, got %s\n",
                token->uid, pw->pw_name, token->user);
        return -1;
    }

    // Lookup group by GID
    struct group *gr = getgrgid(token->gid);
    if (!gr) {
        fprintf(stderr, "GID %d not found\n", token->gid);
        return -1;
    }

    return 0; // All checks passed
}
