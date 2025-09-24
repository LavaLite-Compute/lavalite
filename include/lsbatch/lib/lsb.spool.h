/* $Id: lsb.spool.h,v 1.3 2007/08/15 22:18:48 tmizan Exp $
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
 
#ifndef LSB_SPOOL_H
#define LSB_SPOOL_H

typedef enum spoolOptions  {
    SPOOL_INPUT_FILE,
    SPOOL_COMMAND 
} spoolOptions_t;

typedef struct lsbSpoolInfo {
    char srcFile[MAXFILENAMELEN];    
    char spoolFile[MAXFILENAMELEN];  

} LSB_SPOOL_INFO_T;

typedef enum spoolCopyStatus {
    SPOOL_COPY_SUCCESS =  0,       
    SPOOL_COPY_EXISTS  =  1,       
    SPOOL_COPY_FAILURE = -1,       
    SPOOL_COPY_INITREX_FAILED = -2 
} spoolCopyStatus_t;

typedef struct listElement {
	char * elementName;
	struct listElement * nextElement;
} listElement_t;

typedef struct listElement * listElementPtr_t; 

typedef struct listHeader {
	time_t creationTime;
	listElementPtr_t firstElement;
	listElementPtr_t bestElement;
} listHeader_t;

typedef struct listHeader * listHeaderPtr_t;

#define JOB_SPOOLDIR_DELIMITER "|"

#define SPOOL_LSF_INDIR   "lsf_indir"
#define SPOOL_LSF_CMDDIR  "lsf_cmddir"
#define SPOOL_FAILED_HOSTS   20 

#define LSB_OK_HOST_LIST_UPDATE_PERIOD    300

extern listHeaderPtr_t okHostsListPtr_;

extern char *             getLocalHostOfficialName();
extern LSB_SPOOL_INFO_T * copySpoolFile( const char* srcFilePath
                                ,spoolOptions_t option );
extern char* findSpoolDir( const char* spoolHost );
extern spoolCopyStatus_t copyFileToHost( const char* localSrcFileFullPath
                                ,const char* hostName
                                ,const char* destinFileFullDir
                                ,const char* destinFileName
                                );
extern int removeSpoolFile( const char* hostName
                                ,const char* destinFileFullPath
                          );
extern char*                  getSpoolHostBySpoolFile(const char * spoolFile );                    

extern listHeaderPtr_t        createListHeader();
extern int		      deleteListHeader( 
                                          const listHeaderPtr_t pListHeader );
extern int		      deleteList( const listHeaderPtr_t pListHeader );

extern listElementPtr_t       createListElement( const char * elementName );
extern int		      deleteListElement( 
                                          const listElementPtr_t pListElement );

extern listElementPtr_t       addElementToList( const char * elementName 
			                 ,const listHeaderPtr_t pListHeader );
extern int		      removeElementFromList( 
                                          const listElementPtr_t pListElement
	                                 ,const listHeaderPtr_t pListHeader);
extern listElementPtr_t getBestListElement( 
                                          const listHeaderPtr_t pListHeader );

extern int                    setBestListElement( 
                                          const listElementPtr_t pBestElement
                                         ,const listHeaderPtr_t  pListHeader );

#endif 
