// Copyright (C) LavaLite Contributors
// GPL V2

#include "batch/lib/batch.h"

enum mbd_exit {
    MBD_EXIT_CONF   = 200,
    MBD_EXIT_NET    = 201,
    MBD_EXIT_EVENTS = 202,
    MBD_EXIT_JOBS   = 203,
    MBD_EXIT_MEM    = 204,
    MBD_EXIT_FATAL  = 205,
};

struct mbd_manager {
    uid_t uid;
    gid_t gid;
};

struct job_resources {
    int32_t  job_pid;
};

struct job_data {
    int64_t  job_id;
    uid_t    uid;
    int32_t  status;
    int32_t  exit_status;
    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    double   cpu_time;
    char     from_host[MAXHOSTNAMELEN];
    int32_t  num_exec_hosts;
    char     exec_host[MAXHOSTNAMELEN];  /* primary exec host */
    char     job_name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    int32_t  num_cpu;
    int32_t  num_hosts;
    int32_t  num_gpu;
    uint64_t mem_mb;
    time_t   begin_time;
    time_t   term_time;
    struct job_resources job_res;
};


/* runtime state */
struct mbd_host {
    struct ll_host net;    /* resolved network identity */
    int32_t  max_jobs;       /* configured */
    int32_t  total_cpu;      /* configured */
    int32_t  total_gpu;      /* configured */
    uint64_t total_mem_mb;   /* configured */
    int32_t  status;
    int32_t  num_jobs;
    int32_t  num_run;
    int32_t  num_susp;
    int      sbd_chan;           /* -1 if not connected */
    time_t   last_heard;
};

struct mbd_queue {
    char    queue[LL_BUFSIZ_64];
    char    description[LL_BUFSIZ_256];
    int32_t priority;
    int32_t max_jobs;
    int32_t num_pend;
    int32_t num_run;
    int32_t num_susp;
    int32_t status;
};

struct mbd_group {
    char name[LL_BUFSIZ_64];
    char members[LL_BUFSIZ_1K];  /* space-separated */
    int32_t num_members;
};

extern struct ll_list host_list;
extern struct ll_hash host_name_hash;
extern struct ll_hash host_addr_hash;
extern struct ll_list group_list;
extern struct ll_hash  group_name_hash;
extern struct mbd_manager *mbd_mgr;
extern int mbd_chan;
extern int mbd_efd;
extern uint16_t mbd_port;

int is_manager(uid_t);
void mbd_die(enum mbd_exit);
