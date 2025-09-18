/* $Id: lsb.h,v 1.6 2007/08/15 22:18:47 tmizan Exp $
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

/* Private module umbrella: fine to include config.h installed by
 * automake.
 */
#include "config.h"

/* System headers
*/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <limits.h>
#include <netdb.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <rpc/types.h>
#include <rpcsvc/ypclnt.h>
#include <sys/wait.h>
#include <arpa/inet.h>

/* Include all batch library header files
 */
#include "lsbatch.h"
/* Base core
 */
#include "lsf/intlib/libllcore.h"
/* Base libs
 */
#include "lsf/lib/liblavalite.h"
#include "lsbatch/lib/daemonout.h"
#include "lsbatch/lib/daemons.h"
//#include "lsbatch/lib/lsb.conf.h"
//#include "lsbatch/lib/lsb.log.h"
//#include "lsbatch/lib/lsb.spool.h"
#include "lsbatch/lib/lsb.xdr.h"

#define DEF_COMMITTED_RUN_TIME_FACTOR 0.0

extern struct config_param lsbParams[];
extern int initenv_(struct config_param *, char *);
extern int sig_encode(int);

#define DEFAULT_API_CONNTIMEOUT 10
#define DEFAULT_API_RECVTIMEOUT 0

/* Entries in the lsbParams[] array
 */
#define LSB_DEBUG         0
#define LSB_SHAREDIR      1
#define LSB_SBD_PORT      2
#define LSB_MBD_PORT      3
#define LSB_DEBUG_CMD     4
#define LSB_TIME_CMD      5
#define LSB_CMD_LOGDIR    6
#define LSB_CMD_LOG_MASK  7
#define LSB_API_CONNTIMEOUT 9
#define LSB_API_RECVTIMEOUT 10
#define LSB_SERVERDIR 11
#define LSB_MODE 12
#define LSB_SHORT_HOSTLIST 13
#define LSB_INTERACTIVE_STDERR 14
#define LSB_32_PAREN_ESC     15

#define LSB_API_QUOTE_CMD     14


typedef struct lsbSubSpoolFile {
    char inFileSpool[MAXFILENAMELEN];
    char commandSpool[MAXFILENAMELEN];
} LSB_SUB_SPOOL_FILE_T;

extern int creat_p_socket(void);
extern int serv_connect(char *, ushort, int);
extern int getServerMsg(int, struct LSFHeader *, char **rep_buf);
extern int callmbd(char *, char *, int, char **, struct LSFHeader *, int *,
		       int (*)(), int *);
extern int cmdCallSBD_(char *, char *, int, char **, struct LSFHeader *,
		       int *);


extern int PutQStr(FILE *, char *);
extern int Q2Str(char *, char *);
extern int authTicketTokens_(struct lsfAuth *, char *);

extern char *getNextValue0(char **line, char, char);
extern int readNextPacket(char **, int, struct LSFHeader *, int);
extern void closeSession(int);
extern void upperStr(char *, char *);
extern char* getUnixSpoolDir(char *);
extern char* getNTSpoolDir(char *);
extern char *getMasterName(void);
extern ushort get_mbd_port (void);
extern ushort get_sbd_port (void);
extern int getAuth(struct lsfAuth *);
extern int getCommonParams (struct submit  *, struct submitReq *,
                                                 struct submitReply *);
extern int getOtherParams (struct submit *, struct submitReq *,
                                   struct submitReply *, struct lsfAuth *,
                                   LSB_SUB_SPOOL_FILE_T*);
extern int chUserRemoveSpoolFile( const char * hostName,
				  const char * spoolFile);
extern void prtBETime_(struct submit *);
extern int runBatchEsub(struct lenData *, struct submit *);
