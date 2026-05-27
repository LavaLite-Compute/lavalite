/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.hash.h"

#include "batch/mbd/mbd.h"

int queue_state_init(void)
{
    char dir[PATH_MAX];
    struct dirent *de;
    DIR *d;

    snprintf(dir, sizeof(dir), "%s/mbd/queues", ll_params[LL_STATE_DIR].val);

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        LL_ERR("mkdir %s failed: %m", dir);
        return -1;
    }

    d = opendir(dir);
    if (d == NULL) {
        LL_ERR("opendir %s failed: %m", dir);
        return -1;
    }

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.')
            continue;

        struct mbd_queue *q = ll_hash_search(&queue_name_hash, de->d_name);
        if (q == NULL) {
            char path[2 * PATH_MAX];
            snprintf(path, sizeof(path), "%s/mbd/queues/%s",
                     ll_params[LL_STATE_DIR].val, de->d_name);
            LL_INFO("queue_state_init: stale state file %s, removing", path);
            unlink(path);
            continue;
        }

        q->state = QUEUE_CLOSED;
        LL_INFO("queue_state_init: queue=%s restored closed state", q->name);
    }

    closedir(d);
    return 0;
}

void queue_state_write(const struct mbd_queue *q)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/mbd/queues/%s",
             ll_params[LL_STATE_DIR].val, q->name);

    if (q->state == QUEUE_CLOSED) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            LL_ERR("queue_state_write: open %s failed: %m", path);
        else
            close(fd);
        return;
    }

    if (unlink(path) < 0 && errno != ENOENT)
        LL_ERR("queue_state_write: unlink %s failed: %m", path);
}

void host_state_write(const struct mbd_host *h)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "%s/mbd/hosts/%s", ll_params[LL_STATE_DIR].val,
             h->net.name);

    if (h->state & HOST_CLOSED) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
            LL_ERR("host_state_write: open %s failed: %m", path);
        else
            close(fd);
        return;
    }

    if (unlink(path) < 0 && errno != ENOENT)
        LL_ERR("host_state_write: unlink %s failed: %m", path);
}

int host_state_init(void)
{
    char dir[PATH_MAX];
    struct dirent *de;
    DIR *d;

    snprintf(dir, sizeof(dir), "%s/mbd/hosts", ll_params[LL_STATE_DIR].val);

    if (mkdir(dir, 0755) < 0 && errno != EEXIST) {
        LL_ERR("host_state_init: mkdir %s failed: %m", dir);
        return -1;
    }

    d = opendir(dir);
    if (d == NULL) {
        LL_ERR("host_state_init: opendir %s failed: %m", dir);
        return -1;
    }

    while ((de = readdir(d)) != NULL) {
        char path[PATH_MAX];

        if (de->d_name[0] == '.')
            continue;

        struct mbd_host *h = ll_hash_search(&host_name_hash, de->d_name);
        if (h == NULL) {
            snprintf(path, sizeof(path), "%s/mbd/%s",
                     ll_params[LL_STATE_DIR].val, de->d_name);
            LL_INFO("host_state_init: stale state file %s, removing", path);
            unlink(path);
            continue;
        }

        h->state |= HOST_CLOSED;
        LL_INFO("host_state_init: host=%s restored closed state", h->net.name);
    }

    closedir(d);
    return 0;
}
