// Copyright (C) LavaLite Contributors
// GPL V2
#pragma once

#include "base/lib/ll.protocol.h"
#include "batch/lib/proto.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"

#include "llbatch.h"

// mbd_die exit value
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

struct job_res {
    pid_t    pid;
    uint64_t mem_mb;
    double   cpu_time;
};

struct job_data {
    struct ll_list_entry ent;
    int64_t  job_id;
    uid_t    uid;
    int      status;
    int      exit_status;
    int      priority;
    time_t   submit_time;
    time_t   start_time;
    time_t   end_time;
    time_t   susp_time;
    char     name[LL_BUFSIZ_64];
    char     queue[LL_BUFSIZ_64];
    char     from_host[MAXHOSTNAMELEN];
    char     exec_host[MAXHOSTNAMELEN];
    struct job_res res;
};

/* runtime state */
struct mbd_host {
    struct ll_list_entry ent;
    struct ll_host net;    /* resolved network identity */
    int  max_jobs;       /* configured */
    int  total_cpu;      /* configured */
    int  total_gpu;      /* configured */
    uint64_t total_mem_mb;   /* configured */
    int  status;
    int  num_jobs;
    int  num_run;
    int  num_susp;
    int  sbd_chan;           /* -1 if not connected */
    time_t last_heard;
};

struct queue_conf {
    char name[LL_BUFSIZ_64];
    char desc[LL_BUFSIZ_256];
    char hosts[LL_BUFSIZ_64];
    int  priority;
    int status;
};

struct mbd_queue {
    struct ll_list_entry ent;
    char    name[LL_BUFSIZ_64];
    char    description[LL_BUFSIZ_256];
    char    hosts[LL_BUFSIZ_256];    /* host group name */
    int priority;
    int max_jobs;
    int num_pend;
    int num_run;
    int num_susp;
    int status;
};

struct mbd_group {
    struct ll_list_entry ent;
    char name[LL_BUFSIZ_64];
    int num_members;
    char members[LL_BUFSIZ_1K];  /* space-separated */
};

extern struct ll_list host_list;
extern struct ll_hash host_name_hash;
extern struct ll_hash host_addr_hash;
extern struct ll_hash sbd_chan_hash;

extern struct ll_list group_list;
extern struct ll_hash group_name_hash;

extern struct ll_list queue_list;
extern struct ll_hash queue_name_hash;

extern struct mbd_manager *mbd_mgr;
extern int chan_mbd;
extern int mbd_efd;
extern uint16_t mbd_port;
extern int sched_timer;
extern int chan_timer;

// main.c
int is_manager(uid_t);
void mbd_die(enum mbd_exit);

// conf.c
int conf_init(void);

// compact.c
void compact_start(void);
void compact_shutdown(void);
void handle_compact_done(XDR *, int32_t, struct protocol_header *);
void clean_jobs(time_t);
void reopen_job_events(void);

// net.c
int network_init(void);
int mbd_accept(int);
void mbd_message(int);
int enqueue_payload(int, struct protocol_header *,
                        void *, size_t, bool_t (*xdr_func)());
void shutdown_chan(int);

// job.c
void new_job_reply(XDR *, int32_t);

// sched.c
void schedule(void);

// events.c
int events_init(void);

// dispatch.c
void job_submit(XDR *, int);
void job_signal(XDR *, int);
void job_info(XDR *, int);
void host_info(XDR *, int);
void queue_info(XDR *, int);
void host_group_info(XDR *, int);
void sbd_register(XDR *, int);
void compact_done(XDR *, int);
