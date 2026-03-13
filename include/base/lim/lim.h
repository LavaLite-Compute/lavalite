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

#include "base/lim/lim.proto.h"
#include "base/lim/lim.wire.h"

extern struct ll_list node_list;
extern struct ll_hash node_name_hash;
extern struct ll_hash node_addr_hash;

extern struct lim_node *me;
extern struct current_master current_master;
extern int n_master_candidates;
extern struct lim_node **master_candidates;

extern struct ll_kv lim_params[];
extern struct cluster lim_cluster;

extern int lim_efd;
extern int lim_udp_chan;
extern int tcp_chan;
extern int lim_timer_chan;
extern uint16_t lim_udp_port;
extern uint16_t lim_tcp_port;
extern int lim_debug;

struct cluster {
    char *name;
    char *admin;
};

struct lim_node {
    struct ll_list_entry list;
    struct ll_host *host;
    uint32_t host_no;
    float load_index[NUM_METRICS]; // read /proc every timer
    uint32_t load_report_missing;
    uint32_t status;
    char *machine;     // uname
    char *resources;   // historical string of static resources
    uint16_t tcp_port;
    int16_t is_candidate;
    uint64_t max_mem;
    uint64_t max_swap;
    uint64_t max_tmp;
};

struct current_master {
    struct lim_node *node; // who we think is master, NULL if unknown
    int16_t inactivity; // ticks since last beacon from master
};

// dont collide with the library
enum lim_msg {
    LIM_MASTER_BEACON = 1000,
    LIM_LOAD_REPORT = 1001,
};

#define MISSED_LOAD_REPORT_TOLERANCE 5
#define MISSED_BEACON_TOLERANCE 5

struct master_beacon {
    char     cluster[LL_BUFSIZ_32];
    char     hostname[MAXHOSTNAMELEN];
    uint32_t host_no;
    uint16_t tcp_port;
};

// init
int load_conf(const char *);
int make_cluster(const char *);

// tcp
int tcp_accept(void);
int tcp_message(int);

// udp and beacon
int udp_message(void);

// load
void init_read_proc(void);
void read_proc(void);
int send_load(void);
void rcv_load(XDR *, struct sockaddr_in *, struct protocol_header *);

// mastership
void master(void);
void slave(void);
int is_master_candidate(struct lim_node *);
