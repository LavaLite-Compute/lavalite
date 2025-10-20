/*
 * Copyright (C) LavaLite Contributors
 */

#include "eauth.h"

int
main(int argc, char **argv)
{
    struct auth_token token;
    struct auth_signature sig;
    char payload[EAUTH_LBUFSIZ * 2]; // enough for token + signature

    // Step 1: Fill token
    if (fill_auth_token(&token) != 0) {
        fprintf(stderr, "Failed to fill auth token\n");
        return 1;
    }

    // Step 2: Sign token
    if (sign_auth_token(&token, &sig) != 0) {
        fprintf(stderr, "Failed to sign auth token\n");
        return 1;
    }

    // Step 3: Serialize token + signature
    if (serialize_auth(payload, &token, &sig) != 0) {
        fprintf(stderr, "Failed to serialize auth token\n");
        return 1;
    }

    // Step 4: Send payload (for now, just print it)
    printf("Auth Payload:\n%s\n", payload);

    return 0;
}
