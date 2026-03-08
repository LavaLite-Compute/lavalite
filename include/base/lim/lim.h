/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

#include "base/lib/ll.sys.h"
#include "base/lib/ll.conf.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.channel.h"
#include "base/lib/ll.signal.h"

extern struct ll_list lim_node_list;
extern struct ll_hash lim_node_hash;
extern struct ll_kv lim_params[];
extern struct cluster lim_cluster;

struct cluster {
    char *name;
    char *admin;
};

struct lim_node {
    struct ll_list_entry list;
    struct ll_host *host;
    int host_no;
    float load_index[NLOAD_INDX];
    int host_state;
    char *type;
    char *model;
    char *resources;
};

int lim_load_conf(const char *);
int lim_make_cluster(const char *);
