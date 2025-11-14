/* $Id: lib.rdwr.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it andor modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"

#define IO_TIMEOUT 2000 // milliseconds

/* Time difference in microseconds
 */
static inline int64_t timespec_diff(struct timespec *a, struct timespec *b)
{
    return (int64_t) (a->tv_sec - b->tv_sec) * 1000000LL +
           (int64_t) (a->tv_nsec - b->tv_nsec) / 1000LL;
}

/* Non blocking read fix
 */
ssize_t nb_read_fix(int s, void *vbuf, size_t len)
{
    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }
    if (len == 0)
        return 0;

    unsigned char *p = (unsigned char *) vbuf;
    size_t remaining = len;

    struct timespec start;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (remaining > 0) {
        // Do read
        ssize_t cc = read(s, p, remaining);
        if (cc > 0) {
            p += (size_t) cc;
            remaining -= (size_t) cc;
            // we could consider reset the timeout every time
            // we read some bytes.
            //  clock_gettime(CLOCK_MONOTONIC, &start);
            continue; // made progress
        }
        if (cc == 0) { // clean EOF before all bytes
            errno = EPIPE;
            return -1;
        }

        /* Here we got  cc < 0
         */
        assert(cc < 0);
        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (timespec_diff(&now, &start) > (int64_t) IO_TIMEOUT * 1000LL) {
                errno = ETIMEDOUT;
                return -1;
            }
            int slice = IO_TIMEOUT / 20; // small backoff
            if (slice < 1)
                slice = 1;
            millisleep_(slice);
            continue;
        }

        return -1; // other error; errno preserved
    }

    return (ssize_t) len; // success: exactly len bytes read
}

// Modern version using clock_gettime() instead of gettimeofday()
ssize_t nb_write_fix(int fd, const void *buf, size_t len)
{
    unsigned const char *buffer = (unsigned const char *) buf;
    size_t remaining = len;
    size_t total_written = 0;
    struct timespec start_time;
    struct timespec current_time;

    // Use CLOCK_MONOTONIC to avoid issues with system clock adjustments
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        return -1;
    }

    while (remaining > 0) {
        ssize_t bytes_written = write(fd, buffer + total_written, remaining);

        if (bytes_written > 0) {
            remaining -= bytes_written;
            total_written += bytes_written;
            continue;
        }

        if (bytes_written < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, retry
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Non-blocking I/O would block, check timeout and continue
                goto check_timeout;
            }

            // Real error occurred
            if (errno == EPIPE) {
                lserrno = LSE_LOSTCON; // Assuming this is still needed
            }
            return -1;
        }

    check_timeout:
        // This could be a compound statement in {}
        if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) {
            return -1;
        }

        if (timespec_diff(&current_time, &start_time) > IO_TIMEOUT * 1000LL) {
            errno = ETIMEDOUT;
            return -1;
        }

        // Sleep briefly to avoid busy-waiting
        struct timespec sleep_time = {
            .tv_sec = 0,
            .tv_nsec = (IO_TIMEOUT * 1000000LL) / 20 // nanoseconds
        };
        nanosleep(&sleep_time, NULL);
    } // while ()

    return (ssize_t) total_written;
}

/* blocking read fixed amount of bytes
 */
ssize_t b_read_fix(int s, void *vbuf, size_t len)
{
    size_t total = 0;
    unsigned char *buf = vbuf;

    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    while (len > 0) {
        ssize_t cc = read(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }

        if (cc == 0) {
            // Peer closed connection
            return -1;
        }

        if (cc == -1 && errno == EINTR)
            continue;

        // Unrecoverable error
        return -1;
    }

    return (ssize_t) total;
}

ssize_t b_write_fix(int s, const void *vbuf, size_t len)
{
    const unsigned char *buf = vbuf;
    size_t total = 0;

    if (len > (size_t) SSIZE_MAX) {
        errno = EOVERFLOW;
        return -1;
    }

    while (len > 0) {
        // Do write
        ssize_t cc = write(s, buf, len);

        if (cc > 0) {
            buf += (size_t) cc;
            len -= (size_t) cc;
            total += cc;
            continue;
        }

        if (cc < 0 && errno == EINTR)
            continue;

        // Unrecoverable error
        lserrno = LSE_SOCK_SYS;
        return -1;
    }

    return (ssize_t) total;
}

void unblocksig(int sig)
{
    sigset_t blockMask;
    sigset_t oldMask;

    sigemptyset(&blockMask);
    sigaddset(&blockMask, sig);
    sigprocmask(SIG_UNBLOCK, &blockMask, &oldMask);
}

/* connect() with timeout. Remove the old school the signal based timout
 * handling.
 *
 * Concise but complete - Handles all the necessary cases in ~30 lines
 * Clear flow - Easy to follow the logic from top to bottom
 * Proper error handling - Covers all the edge cases correctly
 * Use of early returns - Avoids nested conditions
 * Modern C idioms - Designated initializers for the pollfd struct
 */
int b_connect_(int s, const struct sockaddr *name, socklen_t namelen,
               int timeout_sec)
{
    int flags = fcntl(s, F_GETFL, 0);
    if (flags < 0)
        return -1;

    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    int rc = connect(s, name, namelen);
    if (rc == 0) {
        // Connected immediately
        fcntl(s, F_SETFL, flags); // Restore flags
        return 0;
    }

    if (errno != EINPROGRESS) {
        // Immediate failure
        fcntl(s, F_SETFL, flags); // Restore flags
        return -1;
    }

    // Initialize poll data structure
    struct pollfd pfd = {.fd = s, .events = POLLOUT};

    rc = poll(&pfd, 1, timeout_sec * 1000);
    if (rc <= 0) {
        // Timeout or poll error
        errno = (rc == 0) ? ETIMEDOUT : errno;
        fcntl(s, F_SETFL, flags); // Restore original flags
        return -1;
    }

    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0) {
        fcntl(s, F_SETFL, flags); // Restore flags
        return -1;
    }

    fcntl(s, F_SETFL, flags); // Restore original flags

    if (err != 0) {
        errno = err;
        return -1;
    }

    return 0;
}

/* Legacy, problematic should we use more then 1024 descriptors
 * in some other select() this will fail.
 */
int rd_select_(int rd, struct timeval *timeout)
{
    int cc;
    fd_set rmask;

    for (;;) {
        FD_ZERO(&rmask);
        FD_SET(rd, &rmask);

        cc = select(rd + 1, &rmask, (fd_set *) 0, (fd_set *) 0, timeout);
        if (cc >= 0)
            return cc;

        if (errno == EINTR)
            continue;
        return -1;
    }
}

/* Modern C version of rd_select() recommended, replacing
 * rd_select_() in deamons with the poll version.
 */
int rd_poll_(int rd, struct timeval *timeout)
{
    struct pollfd pfd = {.fd = rd, .events = POLLIN};

    // Convert timeval to milliseconds
    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    }

    for (;;) {
        int cc = poll(&pfd, 1, timeout_ms);

        if (cc >= 0)
            return cc;

        if (errno == EINTR)
            continue;

        return -1;
    }
}

/* Remove lagacy problematic signal masking.
 * Affects the entire thread, not just the syscall.
 * Can interfere with other subsystems, especially in multi-threaded daemons.
 * Makes debugging and signal handling more complex.
 */
int b_accept_(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    int cc;

    do {
        cc = accept(s, addr, addrlen);
    } while (cc < 0 && errno == EINTR);

    return cc;
}

int blockSigs_(int sig, sigset_t *blockMask, sigset_t *oldMask)
{
    sigfillset(blockMask);

    if (sig)
        sigdelset(blockMask, sig);

    sigdelset(blockMask, SIGHUP);
    sigdelset(blockMask, SIGINT);
    sigdelset(blockMask, SIGQUIT);
    sigdelset(blockMask, SIGILL);
    sigdelset(blockMask, SIGTRAP);
    sigdelset(blockMask, SIGFPE);
    sigdelset(blockMask, SIGBUS);
    sigdelset(blockMask, SIGSEGV);
    sigdelset(blockMask, SIGPIPE);
    sigdelset(blockMask, SIGTERM);

    return sigprocmask(SIG_BLOCK, blockMask, oldMask);
}
ssize_t nb_read_timeout(int fd, void *buf, size_t len, int timeout_sec)
{
    unsigned char *buffer = (unsigned char *) buf;
    size_t remaining = len;
    size_t total_read = 0;
    int timeout_ms = timeout_sec * 1000;

    while (remaining > 0) {
        struct pollfd pfd = {.fd = fd, .events = POLLIN};
        int poll_result = poll(&pfd, 1, timeout_ms);

        // Handle poll errors
        if (poll_result < 0) {
            if (errno == EINTR)
                continue; // Interrupted by signal, retry
            lserrno = LSE_SELECT_SYS;
            return -1;
        }

        // Handle timeout
        if (poll_result == 0) {
            lserrno = LSE_TIME_OUT;
            return -1;
        }

        // Ready to read - attempt the read
        ssize_t bytes_read = recv(fd, buffer + total_read, remaining, 0);

        // Handle successful read
        if (bytes_read > 0) {
            remaining -= bytes_read;
            total_read += bytes_read;
            continue;
        }

        // Handle connection closed
        if (bytes_read == 0) {
            errno = ECONNRESET;
            return -1;
        }

        // Handle read errors (bytes_read < 0)
        if (errno == EINTR)
            continue; // Interrupted, retry
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            continue; // Would block, poll again

        // Real error occurred
        return -1;
    }

    return (ssize_t) total_read;
}
