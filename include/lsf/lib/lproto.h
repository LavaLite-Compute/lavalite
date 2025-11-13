#pragma once
/* $Id: lproto.h,v 1.9 2007/08/15 22:18:51 tmizan Exp $
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

#include "lsf/lib/lib.table.h"
#include "lsf/lib/lib.hdr.h"
// Bug hack res out here
#include "lsf/lib/lib.queue.h"

#define BIND_RETRY_TIMES 100

struct keymap {
    char *key;
    char *val;
    int   position;
};

struct admins {
    int nAdmins;
    int *adminIds;
    int *adminGIds;
    char **adminNames;
};

struct debugReq {
    int opCode;
    int level;
    int logClass;
    int options;
    char *hostName;
    char logFileName[MAXPATHLEN];
};

extern void putMaskLevel(int, char **);
extern bool_t xdr_debugReq (XDR *xdrs, struct debugReq  *debugReq,
                            struct packet_header *hdr);

#define    MBD_DEBUG         1
#define    MBD_TIMING        2
#define    SBD_DEBUG         3
#define    SBD_TIMING        4
#define    LIM_DEBUG         5
#define    LIM_TIMING        6

struct resPair {
    char *name;
    char *value;
};

struct sharedResource {
    char *resourceName;
    int  numInstances;
    struct resourceInstance **instances;
};

struct resourceInfoReq {
    int  numResourceNames;
    char **resourceNames;
    char *hostName;
    int  options;
};

struct resourceInfoReply {
    int    numResources;
    struct lsSharedResourceInfo *resources;
    int    badResource;
};

struct lsbShareResourceInfoReply {
    int    numResources;
    struct lsbSharedResourceInfo *resources;
    int    badResource;
};

#define HOST_ATTR_SERVER        (0x00000001)
#define HOST_ATTR_CLIENT        (0x00000002)
#define HOST_ATTR_NOT_LOCAL     (0x00000004)

extern int sharedResConfigured_;

#define INVALID_FD  (-1)
#define FD_IS_VALID(x)  ((x) >= 0 && (x) < sysconf(_SC_OPEN_MAX) )

#define AUTH_IDENT      "ident"
#define AUTH_PARAM_DCE  "dce"
#define AUTH_PARAM_EAUTH  "eauth"

// If p is NULL no operation is performed by free
// This is straight from the man page
#define FREEUP(p) do { free(p); (p) = NULL; } while (0)

#define STRNCPY(str1, str2, len)  { strncpy(str1, str2, len);   \
        str1[len -1] = '\0';                                    \
    }
/*
 * Bug ALIGNWORD_ legacy macro:
 * Keeps old buffer length arithmetic untouched.
 * XDR_STRLEN() is the correct replacement, but the legacy math has
 * been stable for years and is left intact until full message tests exist.
 */
#define ALIGNWORD_(s)    (((s)&0xfffffffc) + 4)

// Bug
#define NET_INTSIZE_ sizeof(uint32_t)

#define LS_EXEC_T "LS_EXEC_T"


#define GET_INTNUM(i) ((i)/INTEGER_BITS + 1)
#define SET_BIT(bitNo, integers)                                    \
    integers[(bitNo)/INTEGER_BITS] |= (1<< (bitNo)%INTEGER_BITS);
#define CLEAR_BIT(bitNo, integers)                                  \
    integers[(bitNo)/INTEGER_BITS] &= ~(1<< (bitNo)%INTEGER_BITS);
#define TEST_BIT(bitNo, integers, isSet)                                \
    {                                                                   \
        if (integers[(bitNo)/INTEGER_BITS] & (1<<(bitNo)%INTEGER_BITS)) \
            isSet = 1;                                                  \
        else                                                            \
            isSet = 0;                                                  \
    }

extern int expectReturnCode_(int, int, struct packet_header *);
extern int ackAsyncReturnCode_(int, struct packet_header *);
extern int resRC2LSErr_(int);
extern int ackReturnCode_(int);

extern int getConnectionNum_(char *hostName);
extern void inithostsock_(void);

extern int initenv_(struct config_param *, char *);
extern char *lsTmpDir_;
extern short getMasterCandidateNoByName_(char *);
extern char *getMasterCandidateNameByNo_(short);
extern int getNumMasterCandidates_();
extern int initMasterList_();
extern int getIsMasterCandidate_();
extern void freeupMasterCandidate_(int);
extern char *resetLSFUsreDomain(char *);


extern int runEsub_(struct lenData *, char *);
extern int runEexec_(char *, int, struct lenData *, char *);
extern int runEClient_(struct lenData *, char **);
extern char *runEGroup_(char *, char *);

extern int getAuth_(struct lsfAuth *, char *);
extern int verifyEAuth_(struct lsfAuth *, struct sockaddr_in *);
extern int putEauthClientEnvVar(char *);
extern int putEauthServerEnvVar(char *);
#ifdef INTER_DAEMON_AUTH
extern int putEauthAuxDataEnvVar(char *);
extern int putEauthAuxStatusEnvVar(char *);
#endif


extern int sig_encode(int);
extern int sig_decode(int);
extern int getSigVal(const char *);
extern char *getSigSymbolList (void);
extern char *getSigSymbol (int);
extern int blockALL_SIGS_(sigset_t *, sigset_t *);
extern int encodeTermios_(XDR *, struct termios *);
extern int decodeTermios_(XDR *, struct termios *);
extern int rstty_(char *host);
extern int rstty_async_(char *host);
extern int do_rstty_(int, int, int);

extern char isanumber_(char *);
extern char islongint_(const char *);
extern char isint_(char *);
extern int isdigitstr_(const char *);
extern char *putstr_ (const char *);
extern int ls_strcat(char *,int,char *);
extern char *mygetwd_(const char *);
extern char *chDisplay_(char *);
extern void init_pack_hdr(struct packet_header *);
extern struct group *mygetgrnam( const char *name);
extern void *myrealloc(void *ptr, size_t size);
extern char *getNextToken(char **sp);
extern int getValPair(char **resReq, int *val1, int *val2);
extern char *my_getopt (int nargc, char **nargv, char *ostr, char **errMsg);
extern int putEnv(char *env, char *val);
extern const char* getCmdPathName_(const char *cmdStr, int* cmdLen);
extern int replace1stCmd_(const char* oldCmdArgs, const char* newCmdArgs,
                          char* outCmdArgs, int outLen);
extern const char* getLowestDir_(const char* filePath);
extern void getLSFAdmins_(void);
extern bool_t isLSFAdmin_(const char *name);
extern int64_t atoi64_(const char *);

extern void stripDomain_(char *);

extern const struct hostent* setHostEntry_(const struct hostent* hp);

#define LOCAL_HATTRIB_UPDATE_INTERVAL        (30*60)
#define HOSTNS_HATTRIB_UPDATE_INTERVAL       (24*60*60)
#define INFINITE_HATTRIB_UPDATE_INTERVAL     (-1)

extern int (*getHostAttribFuncP)(char *hname, int updateIntvl);
extern int daemonId;

#define DISABLE_HNAME_SERVER    (0x0001)
#define HOSTNS_NEED_MAPPING       (0x0002)
#define HOSTNS_REFRESH_CACHE      (0x0004)
#define HOSTNS_ENTRY_ONLY         (0x0008)

extern struct hostent *Gethostbyname_ex_ (char *, int options);

extern int lockHost_(time_t duration, char *hname);
extern int unlockHost_(char *hname);

extern int lsfRu2Str(FILE *, struct lsfRusage *);
extern int  str2lsfRu(char *, struct lsfRusage *, int *);

extern char *getNextLineC_(FILE *, int *, int);
extern char *getNextLine_(FILE *, int);
extern char *getNextWord_(char **);
extern char * getNextWord1_(char **line);
extern char *getNextWordSet(char **, const char *);
extern char * getline_(FILE *fp, int *);
extern char * getThisLine_(FILE *fp, int *LineCount);
extern char * getNextValueQ_(char **, char, char);
extern int  stripQStr (char *q, char *str);
extern int addQStr (FILE *, char *str);
extern struct pStack *initStack(void);
extern int pushStack(struct pStack *, struct confNode *);
extern struct confNode * popStack(struct pStack *);
extern void freeStack(struct pStack *);

extern char *getNextLineD_(FILE *, int *, int);
extern char *getNextLineC_conf(struct lsConf *, int *, int);
extern char *getNextLine_conf(struct lsConf *, int);
extern void subNewLine_(char*);

extern void doSkipSection(FILE *, int *, char *, char *);
extern int isSectionEnd (char *, char *, int *, char *);
extern int keyMatch (struct keymap *keyList, char *line, int exact);
extern int mapValues (struct keymap *keyList, char *line);
extern int readHvalues(struct keymap *, char *, FILE *, char *, int *, int, char *);
extern char *getNextValue(char **line);
extern int putValue(struct keymap *keyList, char *key, char *value);
extern char *getBeginLine(FILE *, int *);
extern int putInLists (char *, struct admins *, int *, char *);
extern int isInlist (char **, char *, int);

extern void doSkipSection_conf(struct lsConf *, int *, char *, char *);
extern char *getBeginLine_conf(struct lsConf *, int *);

extern ssize_t nb_read_fix(int, void *, size_t);
extern ssize_t nb_write_fix(int, const void *, size_t);
extern ssize_t nb_read_timeout(int, void *, size_t, int);
extern ssize_t b_read_fix(int, void *, size_t);
extern ssize_t b_write_fix(int, const void *, size_t);
extern int b_connect_(int, const struct sockaddr *, socklen_t , int);
extern int rd_select_(int, struct timeval *);
extern int rd_poll_(int, struct timeval *);
extern int b_accept_(int, struct sockaddr *, socklen_t *);
extern int blockSigs_(int, sigset_t *, sigset_t *);

extern int readDecodeHdr_ (int, char *,  ssize_t (*readFunc)(),
                           XDR *xdrs,
                           struct packet_header *hdr);
extern int writeEncodeHdr_(int, struct packet_header *, ssize_t (*)());
extern int io_nonblock_(int);
extern int io_block_(int);
extern void rlimitEncode_(struct lsfLimit *, struct rlimit *, int);
extern void rlimitDecode_(struct lsfLimit *, struct rlimit *, int);

extern void verrlog_(int level, FILE *fp, const char *fmt, va_list ap);
extern int getLogClass_ (char *, char *);
extern int getLogMask(char **, char *);
extern void ls_openlog(const char *, const char *, int, char *);
extern void ls_closelog(void);
extern int  ls_setlogmask(int maskpri);

extern void initkeylist(struct keymap *, int, int, struct lsInfo *);
extern void freekeyval(struct keymap *);
extern char *parsewindow(char *, char *, int *, char *);

extern int expandList_(char ***, int, char **);
extern int expandList1_(char ***, int, int *, char **);

extern void xdr_lsffree(bool_t (*)(), void *, struct packet_header *);

extern int createUtmpEntry(char *, pid_t, char *);
extern int removeUtmpEntry(pid_t);

extern int createSpoolSubDir(const char *);

/* Original wrapper had wrappers around POSIX calls to get user information.
 * LavaLite uses the POSIX calls directly whenever possible, but some wrappers
 * are maintained for convenience.
 */
int get_uid(const char *, uid_t *);
int millisleep_(uint32_t);
int is_valid_host(const char *hname);
