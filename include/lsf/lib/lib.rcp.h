/* $Id: lib.rcp.h,v 1.3 2007/08/15 22:18:51 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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
 
#ifndef LIB_RCP_H
#define LSB_RCP_H

typedef struct rcpXfer {
    char *szSourceArg;
    char *szDestArg;
    char *szHostUser;
    char *szDestUser;
    char *szHost;
    char *szDest;
    int  iNumFiles;
    char *ppszHostFnames[1];
    char *ppszDestFnames[1];
    struct hostent *pheHost;
    struct hostent *pheDest;
    int  iOptions;
} lsRcpXfer;

#define RSHCMD "rsh"

#define SPOOL_DIR_SEPARATOR "/"
#define SPOOL_DIR_SEPARATOR_CHAR '/' 

#define SPOOL_BY_LSRCP      0x1



#define FILE_ERRNO(errno) \
    (errno == ENOENT || errno == EPERM || errno == EACCES || \
     errno == ELOOP || errno == ENAMETOOLONG || errno == ENOTDIR || \
         errno == EBADF || errno == EFAULT || \
         errno == EEXIST || errno == ENFILE || errno == EINVAL || \
         errno == EISDIR || errno == ENOSPC || errno == ENXIO || \
         errno == EROFS || errno == ETXTBSY)

#define LSRCP_MSGSIZE   1048576 

extern int mystat_(char *, struct stat *, struct hostent *);
extern int myopen_(char *, int, int, struct hostent *);
extern char * usePath(char *path);
extern int parseXferArg(char *arg, char **userName, char **hostName, char **fName);
extern int createXfer( lsRcpXfer *lsXfer );
extern int destroyXfer( lsRcpXfer *lsXfer );
extern int copyFile( lsRcpXfer *lsXfer, char* buf, int option );
extern int equivalentXferFile(lsRcpXfer *lsXfer, char *szLocalFile, char *szRemoteFile, 
                    struct stat *psLstat, struct stat *psRstat, char *szRhost);
extern int doXferRcp(lsRcpXfer *lsXfer, int option);
extern int rmDirAndFiles(char *dir);
extern int rmDirAndFilesEx(char *, int);
extern int createSpoolSubDir( const char * spoolFileFullPath );

#endif 
