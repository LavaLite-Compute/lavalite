// Copyright (C) LavaLite Contributors
// GPL v2

#pragma once

#include <stdint.h>
#include <netinet/in.h>

#include "base/lib/ll.bufsiz.h"

struct snamespace {
    int64_t job_id;

    char name[LL_BUFSIZ_64];       /* svc24 */
    char sbd_if[LL_BUFSIZ_64];     /* ll_sbd24 */
    char svc_if[LL_BUFSIZ_64];     /* ll_svc24 */

    struct in_addr sbd_addr;
    struct in_addr svc_addr;

    uint8_t prefix_len;            /* 30 */
};

int snamespace_create(int64_t job_id, struct snamespace *);
int snamespace_destroy(struct snamespace *);
