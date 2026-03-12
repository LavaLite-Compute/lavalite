/* Copyright (C) LavaLite Contributors
 * GPL v2
 */

enum lim_err {
    LIM_OK,
    LIM_ERROR = -1;
};

enum lim_proto {
    LIM_LOAD,        // ls_load()
    LIM_REPLY_LOAD,

    LIM_GET_CLUSTER_NAME,    // ll_getclustername()
    LIM_REPLY_CLUSTER_NAME,

    LIM_GET_MASTER_NAME,     // ll_getmastername()
    LIM_REPLY_MASTER_NAME,

    LIM_GET_HOST_INFO,  // ll_gethostinfo()
    LIM_REPLY_HOST_INFO,
};

struct host_info *ll_gethostinfo(int *);
void ll_free_hostinfo(struct host_info *, int);
struct cluster_load *ll_load(int *);
void ll_free_load(struct cluster_load *, int);
int ll_getclustername(char *, size_t);
int ll_getmastername(char *, size_t);
