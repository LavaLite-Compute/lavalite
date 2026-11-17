/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

/*
 * llsign - sign a client request with the client key.
 *
 * Installed setgid to the mbd group, the group that can read
 * ll.auth.key; users cannot read it. libllbat runs llsign for every
 * request: the encoded message on stdin, header then payload, the
 * signed message on stdout, same length.
 *
 * The uid and gid in the signed header are the real ones of the
 * caller, from the kernel, whatever the request says. Only client
 * opcodes are signed. A user can get a signed request for their own
 * uid, never for another uid or for a daemon operation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <rpc/xdr.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/auth.h"
#include "batch/lib/rpc.h"

/* largest request, a job submission with its script */
enum { LLSIGN_MAX = 16 * 1024 * 1024 };

/* read stdin to EOF, at most LLSIGN_MAX bytes */
static char *read_all(size_t *len)
{
    size_t cap = 4096;
    size_t n = 0;
    char *buf = malloc(cap);
    if (buf == NULL)
        return NULL;

    while (1) {
        if (n == cap) {
            if (cap >= LLSIGN_MAX) {
                free(buf);
                errno = EMSGSIZE;
                return NULL;
            }
            cap = cap * 2;
            char *p = realloc(buf, cap);
            if (p == NULL) {
                free(buf);
                return NULL;
            }
            buf = p;
        }

        ssize_t cc = read(STDIN_FILENO, buf + n, cap - n);
        if (cc < 0) {
            if (errno == EINTR)
                continue;
            free(buf);
            return NULL;
        }
        if (cc == 0)
            break;
        n += (size_t) cc;
    }

    *len = n;
    return buf;
}

static int write_all(const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t cc = write(STDOUT_FILENO, buf + off, len - off);
        if (cc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t) cc;
    }
    return 0;
}

int main(void)
{
    size_t len;
    char *buf = read_all(&len);
    if (buf == NULL) {
        fprintf(stderr, "llsign: read request: %s\n", strerror(errno));
        return 1;
    }

    if (len < (size_t) PACKET_HEADER_SIZE) {
        fprintf(stderr, "llsign: short request len=%zu\n", len);
        free(buf);
        return 1;
    }

    struct protocol_header hdr;
    XDR xdrs;
    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        xdr_destroy(&xdrs);
        fprintf(stderr, "llsign: bad request header\n");
        free(buf);
        return 1;
    }
    xdr_destroy(&xdrs);

    if (!batch_op_is_client((enum batch_lib_op) hdr.operation)) {
        fprintf(stderr, "llsign: operation %d is not a client request\n",
                hdr.operation);
        free(buf);
        return 1;
    }

    /* uid, gid from getuid()/getgid(), the caller's real ones */
    if (auth_sign_msg(buf, len, AUTH_KEY_CLIENT) < 0) {
        fprintf(stderr, "llsign: sign: %s\n", strerror(errno));
        free(buf);
        return 1;
    }

    if (write_all(buf, len) < 0) {
        fprintf(stderr, "llsign: write: %s\n", strerror(errno));
        free(buf);
        return 1;
    }

    free(buf);
    return 0;
}
