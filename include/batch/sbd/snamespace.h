// Copyright (C) LavaLite Contributors
// GPL v2

#pragma once

#include <stdint.h>
#include <netinet/in.h>

#include "base/lib/ll.bufsiz.h"
#include "batch/sbd/sbd.h"

struct snamespace {
    int64_t job_id;

    char name[LL_BUFSIZ_64];       /* svc24 */
    char sbd_if[LL_BUFSIZ_64];     /* ll_sbd24 */
    char svc_if[LL_BUFSIZ_64];     /* ll_svc24 */

    struct in_addr sbd_addr;
    struct in_addr svc_addr;

    uint8_t prefix_len;            /* 30 */
};

int snamespace_setup(struct sbd_job *);
int snamespace_create(const char *);
int snamespace_open(const char *);
int snamespace_enter(int);
int snamespace_destroy(const char *);
int snamespace_enter_job(const struct sbd_job *);
int snamespace_destroy_job(const struct sbd_job *);
