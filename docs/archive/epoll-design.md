# epoll Implementation in LavaLite

## Overview

LavaLite is replacing `select(2)` with `epoll(7)` for scalable event-driven I/O in the LIM and MBD daemons. The channel library provides async buffered I/O; daemons pump the event loop.

## Current Architecture (select-based)

### Channel Library
- **Channel array**: Fixed-size array indexed by channel ID (`ch_id`)
- **Per-channel state**: Socket fd, type (TCP/UDP/PASSIVE), state (CONN/WAIT/DISC), send/recv buffer queues
- **Async I/O**: Non-blocking sockets with circular buffer lists
  - `doread()`: Read as much as possible into recv queue, set READY when packet complete
  - `dowrite()`: Write from send queue, dequeue when complete

### Daemon Event Loop
```c
for (;;) {
    chanSelect_(&sockmask, &chanmask, &timeout);

    // Check listening sockets
    if (FD_ISSET(lim_udp_sock, &chanmask.rmask))
        process_udp_request();
    if (FD_ISSET(lim_tcp_sock, &chanmask.rmask))
        accept_connection();

    // Process ready client channels
    handle_tcp_client(&chanmask);
}
```

**Problem**: `select()` scans all fds on every call. Doesn't scale beyond ~1024 fds.

## epoll Design

### Goals
1. Replace `select()` with `epoll()` - level-triggered, same semantics
2. Keep channel abstraction unchanged - library handles epoll internally
3. Maintain async I/O model - read/write as much as possible
4. Direct fd→channel mapping via `epoll_event.data.u32`

### Data Structures

#### Channel Extension
```c
struct chan_data {
    int handle;          // socket fd
    enum chanType type;
    enum chanState state;
    struct Buffer *send;
    struct Buffer *recv;
    int epoll_state;     // CHAN_EPOLL_READY, CHAN_EPOLL_WAIT, etc
};
```

#### Daemon State
```c
static int epoll_fd = -1;          // per-daemon epoll instance
static struct epoll_event events[MAX_EVENTS];  // event array
```

### Interface

#### Initialization
```c
int chan_epoll_init(void)
{
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    return epoll_fd;
}
```
Called once at daemon startup.

#### Channel Registration
Channels added/removed via:
```c
int chan_epoll_add(int ch_id, uint32_t events)
{
    struct epoll_event ev = {
        .events = events,       // EPOLLIN | EPOLLOUT
        .data.u32 = ch_id       // direct channel index
    };
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD,
                     channels[ch_id].handle, &ev);
}

int chan_epoll_del(int ch_id)
{
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL,
                     channels[ch_id].handle, NULL);
}

int chan_epoll_mod(int ch_id, uint32_t events)
{
    struct epoll_event ev = {
        .events = events,
        .data.u32 = ch_id
    };
    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD,
                     channels[ch_id].handle, &ev);
}
```

#### Event Loop
```c
int chan_epoll(struct Masks *chanmask, int timeout_ms)
{
    int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
    if (nfds < 0)
        return -1;

    FD_ZERO(&chanmask->rmask);
    FD_ZERO(&chanmask->wmask);
    FD_ZERO(&chanmask->emask);

    for (int i = 0; i < nfds; i++) {
        int ch_id = events[i].data.u32;
        uint32_t ev = events[i].events;

        if (ev & EPOLLIN)
            doread(ch_id, chanmask);
        if (ev & EPOLLOUT)
            dowrite(ch_id, chanmask);
        if (ev & (EPOLLERR | EPOLLHUP))
            FD_SET(ch_id, &chanmask->emask);
    }

    return nfds;
}
```

Replaces `chanSelect_()`. Returns ready channels in `chanmask` for compatibility.

### Channel Lifecycle

#### Listen Socket
```c
lim_tcp_sock = chan_listen_socket(SOCK_STREAM, 0, SOMAXCONN, CHAN_OP_SOREUSE);
chan_epoll_add(lim_tcp_sock, EPOLLIN);  // wait for connections
```

#### Accept Connection
```c
ch_id = chan_accept(lim_tcp_sock, &from);
chan_epoll_add(ch_id, EPOLLIN);  // start reading
```

#### Send Data
```c
chan_enqueue(ch_id, buffer);
chan_epoll_mod(ch_id, EPOLLIN | EPOLLOUT);  // enable write events
```
After `dowrite()` drains queue:
```c
if (send_queue_empty)
    chan_epoll_mod(ch_id, EPOLLIN);  // disable write events
```

#### Close
```c
chan_epoll_del(ch_id);
chan_close(ch_id);
```

### Edge Cases

**Level-triggered**: Keep same semantics as `select()`. epoll will re-notify if data remains.

**EAGAIN/EWOULDBLOCK**: `doread()/dowrite()` already handle this - return and wait for next event.

**Partial I/O**: Buffer queues track position. Read/write from `buf->pos` to `buf->len`.

**EPOLLHUP**: Treat as channel error, set `emask`, daemon handles cleanup.

## Migration Plan

1. **Library changes** (few days):
   - Add `chan_epoll_init/add/del/mod()`
   - Replace `chanSelect_()` internals with `chan_epoll()`
   - Keep masks for backward compat
   - Test with LIM first

2. **Daemon changes**:
   - LIM: Replace `chanSelect_()` call with `chan_epoll()`
   - MBD: Same replacement
   - No other daemon changes needed

3. **Remove select**:
   - Delete `chanSelect_()` after both daemons converted
   - Remove `struct Masks` eventually (or keep as thin wrapper)

## Performance Notes

- **Scalability**: O(1) per event vs O(nfds) for select
- **No fd limits**: Works beyond 1024 connections
- **Reduced syscalls**: epoll_ctl separate from epoll_wait
- **Cache locality**: Kernel only wakes for active fds

## Why Not Edge-Triggered?

**Level-triggered** is simpler:
- No state synchronization bugs
- Partial I/O handled naturally
- Same semantics as select
- Easier to debug

Edge-triggered adds complexity for minimal gain in this workload (job scheduling, not high-frequency trading).

## Code Clarity

**Direct mapping**: `epoll_event.data.u32 = ch_id` gives instant channel lookup. No hash tables, no search.

**Minimal abstraction**: `chan_epoll()` is thin wrapper over `epoll_wait()`. Library owns epoll mechanics, daemon just pumps events.

**No magic**: Channel registration explicit via `add/del/mod`. No hidden state.

---

**Author**: System implementation, 2025
**Status**: Design approved, implementation in progress
