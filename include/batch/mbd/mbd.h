// Copyright (C) LavaLite Contributors
// GPL V2
#pragma once

#include "base/lib/ll.protocol.h"
#include "batch/lib/rpc.h"
#include "batch/lib/wire.h"
#include "base/lib/ll.bufsiz.h"
#include "base/lib/ll.sys.h"
#include "base/lib/ll.hash.h"
#include "base/lib/ll.host.h"
#include "base/lib/ll.syslog.h"
#include "base/lib/ll.list.h"
#include "base/lib/ll.channel.h"

#include "llbatch.h"

enum job_list_id {
    JOB_LIST_PEND   = 0,
    JOB_LIST_RUN    = 1,
    JOB_LIST_FINISH = 2,
};

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

struct job_dep {
    struct ll_list_entry ent;
    int type;
    int64_t job_id;
};

struct job_data {
    struct ll_list_entry ent;
    int64_t job_id;
    uid_t uid;
    gid_t gid;
    char user[LL_BUFSIZ_64];
    int status;
    int exit_status;
    int priority;
    time_t submit_time;
    time_t start_time;
    time_t end_time;
    time_t susp_time;
    time_t requeue_time;
    time_t begin_time;
    time_t term_time;
    struct mbd_queue *queue;
    char project[LL_BUFSIZ_256];
    char gpu_type[LL_BUFSIZ_256];
    char machines[LL_BUFSIZ_4K];
    char exec_host[MAXHOSTNAMELEN];
    char name[LL_BUFSIZ_64];
    char comment[LL_BUFSIZ_1K];
    char from_host[MAXHOSTNAMELEN];
    int num_cpus;
    int num_nhosts;
    int num_gpus;
    uint64_t mem_mb;
    uint32_t flags;
    int pend_sig;
    enum job_list_id list_id;
    struct ll_list deps;
    struct job_res res;
};

/* runtime state */
struct mbd_host {
    struct ll_list_entry ent;
    struct ll_host net;    /* resolved network identity */
    int  max_jobs;       /* configured */
    int  total_cpu;      /* configured */
    int  total_gpu;      /* configured */
    char gpu_type[LL_BUFSIZ_64];     /* empty if no GPU */
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

#define JOB_BUCKETS 10

extern int64_t job_id_seq;
extern struct ll_hash job_id_hash;

extern struct ll_list pend_jobs_list;
extern struct ll_list run_jobs_list;
extern struct ll_list finish_jobs_list;

extern struct ll_list host_list;
extern struct ll_hash host_name_hash;
extern struct ll_hash host_addr_hash;

extern struct ll_hash sbd_chan_hash;

extern struct ll_list group_list;
extern struct ll_hash group_name_hash;

extern struct ll_list queue_list;
extern struct ll_hash queue_name_hash;

extern struct mbd_manager mbd_mgr;
extern int chan_mbd;
extern int mbd_efd;
extern uint16_t mbd_port;
extern int sched_timer;
extern int chan_timer;
extern  char jobs_dir[];

// main.c
void mbd_die(enum mbd_exit);

// conf.c
int conf_init(void);
int init_manager(void);
int is_manager(uid_t);

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
int32_t enqueue_header(int, int, int);
void chan_shutdown(int);

// job.c


// sched.c
void schedule(void);

// events.c
int events_init(void);
void reopen_job_events(void);
void event_job_new(const struct job_data *, const struct wire_job_submit *);
void event_job_start(const struct job_data *);
void event_job_accept(const struct job_data *);
void event_job_execute(const struct job_data *, const char *);
void event_job_signal(const struct job_data *, const struct wire_job_sig *);
void event_job_finish(const struct job_data *);
void event_job_pend_susp(const struct job_data *);
void event_job_pend_resume(const struct job_data *);
void event_job_susp(const struct job_data *);

// dispatch.c
int job_signal(XDR *, int);
int job_info(XDR *, int);
int host_info(XDR *, int);
int queue_info(XDR *, int);
int host_group_info(XDR *, int);
int sbd_register(XDR *, int);
int compact_done(XDR *, int);

// job.c
int jobs_replay(void);
void new_job_reply(XDR *, int32_t);
int job_init(void);
int job_accept(XDR *, int);
struct job_data *job_find(int64_t);
void job_set_list(struct job_data *, struct ll_list *, enum job_list_id);
void job_move_list(struct job_data *, struct ll_list *,
                   struct ll_list *, enum job_list_id);
