/*
 * Copyright (C) LavaLite Contributors
 */
#include "eauth.h"

int
main(int argc, char **argv)
{
    char payload[EAUTH_LBUFSIZ * 2];

    // Step 1: Receive payload (for now, read from stdin)
    if (fgets(payload, sizeof(payload), stdin) == NULL) {
        fprintf(stderr, "Failed to read payload\n");
        return 1;
    }

    sanitize_payload(payload);
    // Step 2: Verify token
    if (verify_auth_token(payload) != 0) {
        fprintf(stderr, "Invalid or tampered token\n");
        return 1;
    }

    printf("Token verified successfully\n");
    return 0;
}
