/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

/*
 * llb_sign_msg - have llsign sign an encoded request.
 *
 * Users cannot read the client key, llsign can: it is setgid to the
 * mbd group, see llsign.c. The request goes to llsign on its stdin,
 * the signed request comes back on its stdout, same length, and
 * replaces buf. A socketpair, not pipes: a write to an llsign that
 * died returns EPIPE, it does not kill the caller with SIGPIPE.
 *
 * LL_SIGN in the environment overrides the installed path, for
 * tests from the build tree. It gives nothing away: a program other
 * than the installed llsign cannot read the key.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "batch/lib/rpc.h"

static const char *sign_path(void)
{
    const char *p = getenv("LL_SIGN");
    if (p != NULL && p[0] != 0)
        return p;

    return LL_LIBEXECDIR "/llsign";
}

static int send_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t cc = send(fd, buf + off, len - off, MSG_NOSIGNAL);
        if (cc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t) cc;
    }
    return 0;
}

static int recv_all(int fd, char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t cc = recv(fd, buf + off, len - off, 0);
        if (cc < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (cc == 0) {
            errno = EPROTO;
            return -1;
        }
        off += (size_t) cc;
    }
    return 0;
}

int llb_sign_msg(char *buf, size_t len)
{
    const char *path = sign_path();

    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(sv[0]);
        close(sv[1]);
        return -1;
    }

    if (pid == 0) {
        /* stdin and stdout of llsign are its end of the pair; dup2
         * clears close-on-exec on the copies */
        if (dup2(sv[1], STDIN_FILENO) < 0 || dup2(sv[1], STDOUT_FILENO) < 0)
            _exit(127);
        execl(path, "llsign", (char *) NULL);
        fprintf(stderr, "llsign: exec %s: %s\n", path, strerror(errno));
        _exit(127);
    }

    close(sv[1]);

    int rc = send_all(sv[0], buf, len);
    int saved = errno;
    if (rc == 0) {
        shutdown(sv[0], SHUT_WR);
        rc = recv_all(sv[0], buf, len);
        saved = errno;
    }
    close(sv[0]);

    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            status = -1;
            break;
        }
    }

    /* llsign said why on stderr */
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        errno = EACCES;
        return -1;
    }

    if (rc < 0) {
        errno = saved;
        return -1;
    }

    return 0;
}
