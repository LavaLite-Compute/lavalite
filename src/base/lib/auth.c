/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/random.h>
#include <openssl/crypto.h>
#include <openssl/hmac.h>

#include "base/lib/ll.protocol.h"
#include "base/lib/auth.h"

static uint8_t auth_key[AUTH_KEY_SIZE];
static int auth_key_loaded = 0;
static int auth_required = 1;
static uint32_t auth_allowed_age = AUTH_MAX_AGE;

static const char *key_path(void)
{
    const char *dir = getenv("LL_CONF_DIR");
    if (dir == NULL) {
        errno = ENOENT;
        return NULL;
    }

    static char path[512];
    snprintf(path, sizeof(path), "%s/ll.auth.key", dir);
    return path;
}

int auth_init(uint8_t required, uint32_t allowed_age)
{
    if (auth_load_key() < 0)
        return -1;

    auth_required = 1;
    if (required == 0)
        auth_required = required;

    if (allowed_age > 0)
        auth_allowed_age = allowed_age;

    return 0;
}

void auth_set_required(int required)
{
    auth_required = required;
}

int auth_load_key(void)
{
    if (auth_key_loaded)
        return 0;

    const char *path = key_path();
    if (path == NULL)
        return -1;

    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    ssize_t n = read(fd, auth_key, AUTH_KEY_SIZE);
    close(fd);

    if (n != AUTH_KEY_SIZE) {
        errno = EIO;
        return -1;
    }

    auth_key_loaded = 1;
    return 0;
}

int auth_generate_key(void)
{
    const char *path = key_path();
    if (path == NULL)
        return -1;

    uint8_t key[AUTH_KEY_SIZE];
    if (getrandom(key, sizeof(key), 0) != AUTH_KEY_SIZE) {
        errno = EIO;
        return -1;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0640);
    if (fd < 0) {
        if (errno == EEXIST)
            return 0;
        return -1;
    }

    ssize_t n = write(fd, key, AUTH_KEY_SIZE);
    close(fd);

    if (n != AUTH_KEY_SIZE) {
        errno = EIO;
        return -1;
    }

    return 0;
}

/*
 * The HMAC covers the whole message as it goes on the wire, header and
 * payload, with the header's hmac field zeroed. The header is XDR
 * encoded at a fixed size at the start of the buffer, so it can be
 * decoded, changed and encoded back in place without moving the
 * payload.
 */
static int get_hdr(char *buf, size_t len, struct protocol_header *hdr)
{
    if (len < (size_t) PACKET_HEADER_SIZE) {
        errno = EPROTO;
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_DECODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        errno = EPROTO;
        return -1;
    }
    xdr_destroy(&xdrs);

    if ((size_t) hdr->length + PACKET_HEADER_SIZE != len) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

static int put_hdr(char *buf, struct protocol_header *hdr)
{
    XDR xdrs;
    xdrmem_create(&xdrs, buf, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, hdr)) {
        xdr_destroy(&xdrs);
        errno = EPROTO;
        return -1;
    }
    xdr_destroy(&xdrs);

    return 0;
}

static int compute_hmac(const char *buf, size_t len, uint8_t out[AUTH_KEY_SIZE])
{
    unsigned int n = 0;

    uint8_t *digest = HMAC(EVP_sha256(), auth_key, AUTH_KEY_SIZE,
                           (const unsigned char *) buf, len, NULL, &n);
    if (digest == NULL || n != AUTH_KEY_SIZE) {
        errno = EPROTO;
        return -1;
    }

    memcpy(out, digest, AUTH_KEY_SIZE);
    return 0;
}

/*
 * auth_sign_msg - sign an encoded message in place.
 *
 * buf holds the whole message, len bytes: header then payload. The
 * caller's uid, gid and a timestamp are written into the header, then
 * the HMAC over the whole buffer.
 */
int auth_sign_msg(char *buf, size_t len)
{
    if (auth_load_key() < 0)
        return -1;

    struct protocol_header hdr;
    if (get_hdr(buf, len, &hdr) < 0)
        return -1;

    hdr.uid = (uint32_t) getuid();
    hdr.gid = (uint32_t) getgid();
    hdr.timestamp = (uint32_t) time(NULL);
    memset(hdr.hmac, 0, sizeof(hdr.hmac));
    if (put_hdr(buf, &hdr) < 0)
        return -1;

    if (compute_hmac(buf, len, hdr.hmac) < 0)
        return -1;

    if (put_hdr(buf, &hdr) < 0)
        return -1;

    return 0;
}

/*
 * auth_verify_msg - verify a received message.
 *
 * buf holds the whole message as received, len bytes. The hmac field
 * in buf is zeroed in place to recompute the HMAC; the rest of the
 * buffer is left as it was.
 */
int auth_verify_msg(char *buf, size_t len)
{
    if (!auth_required)
        return 0;

    if (auth_load_key() < 0)
        return -1;

    struct protocol_header hdr;
    if (get_hdr(buf, len, &hdr) < 0)
        return -1;

    uint8_t got[AUTH_KEY_SIZE];
    memcpy(got, hdr.hmac, AUTH_KEY_SIZE);
    memset(hdr.hmac, 0, sizeof(hdr.hmac));
    if (put_hdr(buf, &hdr) < 0)
        return -1;

    uint8_t expected[AUTH_KEY_SIZE];
    if (compute_hmac(buf, len, expected) < 0)
        return -1;

    if (CRYPTO_memcmp(expected, got, AUTH_KEY_SIZE) != 0) {
        errno = EACCES;
        return -1;
    }

    uint32_t now = (uint32_t) time(NULL);
    int32_t age = (int32_t) now - (int32_t) hdr.timestamp;
    if (age < 0)
        age = -age;

    if (age > (int32_t) auth_allowed_age) {
        errno = ETIMEDOUT;
        return -1;
    }

    return 0;
}
