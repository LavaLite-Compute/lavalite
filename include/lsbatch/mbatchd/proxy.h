/* $Id: proxy.h,v 1.3 2007/08/15 22:18:46 tmizan Exp $
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

#ifndef _MBD_INCLUDE_PROXY_H_
#define _MBD_INCLUDE_PROXY_H_


typedef struct proxyListEntry      PROXY_LIST_ENTRY_T;

 
struct proxyListEntry {
    LIST_ENTRY_T *      forw;
    LIST_ENTRY_T *      back;
    void *              subject;
};

extern PROXY_LIST_ENTRY_T *     proxyListEntryCreate(void *subject);
extern void                     proxyListEntryDestroy(PROXY_LIST_ENTRY_T *pxy);


extern LIST_T *          pxyRsvJL;



extern void                     proxyUSJLAttachObsvr(void);
extern void                     proxyUSJLAddEntry(struct jData *job);

extern void                     proxyHSJLAttachObsvr(void);
extern void                     proxyHSJLAddEntry(struct jData *job);

extern void                     proxyHRsvJLAddEntry(struct jData *job);
extern void                     proxyHRsvJLRemoveEntry(struct jData *job);

extern void                     proxyRsvJLAddEntry(struct jData *job);
extern void                     proxyRsvJLRemoveEntry(struct jData *job);


extern bool_t                   proxyListEntryEqual(PROXY_LIST_ENTRY_T *pxy,
						    void *subject,
						    int hint);

extern bool_t                   pendJobPrioEqual(PROXY_LIST_ENTRY_T *pxy,
						 struct jData *subjectJob,
						 int hint);

extern bool_t                   startJobPrioEqual(PROXY_LIST_ENTRY_T *pxy,
						  struct jData *subjectJob,
						  int hint);


#define JOB_PROXY_GET_JOB(pxy) ((struct jData *)(pxy)->subject)

extern struct jData *           jobProxyGetPendJob(PROXY_LIST_ENTRY_T *pxy);
extern void                     jobProxySyslog(PROXY_LIST_ENTRY_T *pxy,
					       void * hint);
extern char *                   jobProxySprintf(PROXY_LIST_ENTRY_T *pxy,
						void *hint);

#endif 



