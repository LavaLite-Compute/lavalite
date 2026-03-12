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

#include "base/lim/proto.h"
#include "base/lim/wire.h"

extern struct ll_list node_list;
extern struct ll_hash node_name_hash;
extern struct ll_hash node_addr_hash;
extern struct lim_node *me;
extern struct lim_master current_master;

extern struct ll_kv lim_params[];
extern struct cluster lim_cluster;
extern int lim_efd;

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
    char *machine;     // uname
    char *resources;   // historical string of static resources
    uint16_t tcp_port;
    int is_candidate;
};

struct lim_master {
    struct lim_node *node; // who we think is master, NULL if unknown
    int inactivity; // ticks since last beacon from master
};

enum lim_state {
    LIM_OK,
    LIM_UNAVAIL,
};

enum lim_msg {
    LIM_MASTER_BEACON,
    LIM_LOAD_REPORT,
};

#define MISSED_LOAD_REPORT_TOLERANCE 5
#define MISSED_BEACON_TOLERANCE 5
#define LIM_NIDX 11

struct master_beacon {
    char     cluster[LL_BUFSIZ_32];
    char     hostname[MAXHOSTNAMELEN];
    uint32_t host_no;
    uint16_t tcp_port;
};

// For the  wire take this liberty
#define true  1
#define false 0

struct wire_load_report {
    char hostname[MAXHOSTNAMELEN];
    uint32_t nidx;
    float li[LIM_NIDX];
};

struct wire_master {
    char hostname[MAXHOSTNAMELEN];
};

struct wite_cluster {
    char hostname[MAXHOSTNAMELEN];
    chae admin[LL_BUFSIZ_64];
};

// init
int load_conf(const char *);
int make_cluster(const char *);

// tcp
int tcp_accept(void);
int tcp_message(int);

// udp and beacon
int udp_message(void);
void beacon_send(struct clusterNode *);
void beacon_recv(XDR *, struct sockaddr_in *, struct protocol_header *);

// wire
bool xdr_beacon(XDR *, struct master_beacon *);

// load
void read_load(void);
int send_load_report(void);
void rcv_load_report(XDR *, struct sockaddr_in *, struct protocol_header *);

bool xdr_wire_load_report(XDR *, struct wire_load_update *);
void read_load(void);
