/* $Id: res.h,v 1.7 2007/08/15 22:18:55 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#ifndef _RES_H_
#define _RES_H_

#include "lsf.h"
#include "lsbatch.h"

#include "lsf/intlib/libllcore.h"
#include "lsf/intlib/list.h"
#include "lsf/intlib/listset.h"
#include "lsf/lib/lib.common.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/lib.xdrres.h"
#include "lsf/lib/lsi18n.h"
#include "lsf/res/rescom.h"
#include "lsf/res/resout.h"

#define NICE_LEAST -40
#define NICE_MIDDLE 20
#define DEF_REXPRIORITY 0
#define LIM_PORT 36000
#define RES_PORT 36002
#define LSTMPDIR lsTmpDir_
#define LSDEVNULL "/dev/null"
#define LSETCDIR "/etc"
#define SOCK_READ_FIX  b_read_fix
#define SOCK_WRITE_FIX b_write_fix
#define NB_SOCK_READ_FIX   nb_read_fix
#define NB_SOCK_WRITE_FIX  nb_write_fix
#define MSGSIZE BUFSIZ

extern int rexecPriority;
extern struct client  *clients[];
extern int client_cnt;
extern struct child  **children;
extern int  child_cnt;
extern char *Myhost;
extern char *myHostType;

extern int lastChildExitStatus;

extern int sbdMode;
extern int sbdFlags;
#define SBD_FLAG_STDIN  0x1
#define SBD_FLAG_STDOUT 0x2
#define SBD_FLAG_STDERR 0x4
#define SBD_FLAG_TERM   0x8

extern int accept_sock;
extern char child_res;
extern char child_go;
extern char res_interrupted;
extern char *gobuf;
extern char allow_accept;
extern char magic_str[];
extern int child_res_port;
extern int parent_res_port;
extern fd_set readmask, writemask, exceptmask;

extern int ctrlSock;
extern struct sockaddr_in ctrlAddr;

extern int on ;
extern int off;
extern int debug;
extern int  res_status;

extern char *lsbJobStarter;

extern char res_logfile[];
extern int  res_logop;
extern int restart_argc;
extern char ** restart_argv;
extern char *env_dir;

#define MAXCLIENTS_HIGHWATER_MARK	100
#define MAXCLIENTS_LOWWATER_MARK	1


#define DOREAD  0
#define DOWRITE 1
#define DOEXCEPTION 2
#define DOSTDERR 3

#define PTY_TEMPLATE		"/dev/ptyXX"

#define PTY_SLAVE_INDEX		(sizeof(PTY_TEMPLATE) - 6)
#define PTY_ALPHA_INDEX		(sizeof(PTY_TEMPLATE) - 3)
#define PTY_DIGIT_INDEX		(sizeof(PTY_TEMPLATE) - 2)

#define PTY_FIRST_ALPHA		'p'
# define PTY_LAST_ALPHA		'v'

#define BUFSTART(x)    ((char *) ((x)->buf) + sizeof(struct LSFHeader))

#define CLOSE_IT(fd)     if (fd>=0) {close(fd); fd = INVALID_FD;}

typedef struct ttystruct  {
        struct termios attr;
        struct winsize  ws;
} ttyStruct;


struct client   {
    int             client_sock;
    int             ruid;
    int             gid;
    char            *username;
    char            *clntdir;
	char            *homedir;
    ttyStruct       tty;
    char            **env;
    int             ngroups;
    gid_t	groups[NGROUPS];
	struct           hostent hostent;
	struct lenData   eexec;
};

struct child  {
    struct client   *backClnPtr;
    int             rpid;
    int             pid;
    int             refcnt;
    int             info;
    int             stdio;
    outputChannel   std_out;
    outputChannel   std_err;
    niosChannel     remsock;
    int             rexflag;
    char            server;
    char            c_eof;
    char            running;
    char            sigchild;
    int  wait;
    struct sigStatusUsage *sigStatRu;
    int             endstdin;
    RelayBuf        i_buf;
    int             stdin_up;
    char            slavepty[sizeof(PTY_TEMPLATE)];
    char 		**cmdln;
    time_t		dpTime;
    char            *cwd;
    char            username[MAXLSFNAMELEN];
    char            fromhost[MAXHOSTNAMELEN];
    int    	        sent_eof;
    int    	        sent_status;
};

struct resChildInfo {
    struct resConnect *resConnect;
    struct lsfAuth  *lsfAuth;
    struct passwd *pw;
    struct hostent *host;
    u_short parentPort;
    int currentRESSN;
    int res_status;
};



typedef struct taggedConn {
	niosChannel  sock;
	int          rtag;
	int          wtag;
    int          *task_duped;
	int          num_duped;
} taggedConn_t;

typedef struct resNotice {
    struct resNotice *forw;
    struct resNotice *back;
    int rpid;
    int retsock;
    int opCode;
    resAck ack;
    struct sigStatusUsage sigStatRu;
} resNotice_t;

extern taggedConn_t conn2NIOS;
extern LIST_T *resNotifyList;


extern int currentRESSN;
#define LSF_RES_DEBUG      0
#define LSF_SERVERDIR      1
#define LSF_AUTH           2
#define LSF_LOGDIR         3
#define LSF_ROOT_REX       4
#define LSF_LIM_PORT	   5
#define LSF_RES_PORT	   6
#define LSF_ID_PORT	   7
#define LSF_USE_HOSTEQUIV  8
#define LSF_RES_ACCTDIR    9
#define LSF_RES_ACCT       10
#define LSF_DEBUG_RES      11
#define LSF_TIME_RES       12
#define LSF_LOG_MASK       13
#define LSF_RES_RLIMIT_UNLIM 14
#define LSF_CMD_SHELL      15
#define LSF_ENABLE_PTY     16
#define LSF_TMPDIR         17
#define LSF_BINDIR         18
#define LSF_LIBDIR         19
#define LSF_RES_TIMEOUT    20
#define LSF_RES_NO_LINEBUF 21
#define LSF_MLS_LOG   	   22

#define LSB_UTMP           0

#define SIG_NT_CTRLC        2000
#define SIG_NT_CTRLBREAK    2001

extern struct config_param resParams[];
extern struct config_param resConfParams[];

#define RES_REPLYBUF_LEN   4096

#define RESS_LOGBIT         0x00000001

/* Bug. Redefined.
 */
extern void Signal_(int, void (*f)(int));

extern void init_res(void);
extern void resExit_(int exitCode);
extern int ptymaster(char *);
extern int ptyslave(char *);
extern void doacceptconn(void);
extern void dochild_stdio(struct child *, int);
extern void dochild_remsock(struct child *, int);
extern void dochild_buffer(struct child *, int);
extern void dochild_info(struct child *, int);
extern void doclient(struct client *);
extern void ptyreset(void);
extern void stdout_flush(struct child *chld);
extern void doResParentCtrl(void);
extern resAck sendResParent(struct LSFHeader *, char *, bool_t (*)());
extern int sendReturnCode(int, int);

extern void donios_sock(struct child **, int);
extern int deliver_notifications(LIST_T *);

extern void term_handler(int);
extern void sigHandler(int);
extern void child_handler(int);
extern void child_handler_ext(void);
extern void getMaskReady(fd_set *rm, fd_set *wm, fd_set *em);
extern void display_masks(fd_set *, fd_set *, fd_set *);

extern int lsbJobStart(char **, u_short, char *, int);

extern void childAcceptConn(int, struct passwd *, struct lsfAuth *,
			    struct resConnect *, struct hostent *);

extern void resChild(char *, char *);
extern int  resParent(int, struct passwd *, struct lsfAuth *,
			    struct resConnect *, struct hostent *);
extern bool_t isLSFAdmin(const char *);

extern bool_t xdr_resChildInfo(XDR  *, struct resChildInfo *,
			       struct LSFHeader *);

extern void rfServ_(int);

extern char *pty_translate(char *);
extern int check_valid_tty(char *);

extern void resAcctWrite(struct child *);
extern void initResLog(void);
extern char resAcctFN[MAXFILENAMELEN];
extern int resLogOn;
extern int resLogcpuTime;
extern void initRU(struct rusage *);
extern void resParentWriteAcct(struct LSFHeader *, XDR *, int);

extern int findRmiDest(int *, int *);

extern void delete_child(struct child *);
extern void destroy_child(struct child *);
extern int resSignal(struct child *chld, struct resSignal sig);

extern void dumpClient(struct client *, char * );
extern void dumpChild(struct child *, int, char *);

#define UTMP_CHECK_CODE "sbdRes"

#endif
