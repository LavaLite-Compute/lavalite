/* $Id: lsb.xdr.h,v 1.11 2007/08/15 22:18:48 tmizan Exp $
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

#ifndef _LSB_XDR_
#define _LSB_XDR_

#include "lsbatch/lib/lsb.h"

extern bool_t xdr_submitReq(XDR *,
			    struct submitReq *,
			    struct packet_header *);

extern bool_t xdr_submitMbdReply(XDR *,
				 struct submitMbdReply *,
				 struct packet_header *);

extern bool_t xdr_signalReq(XDR *,
			    struct signalReq *,
			    struct packet_header *);

extern bool_t xdr_lsbMsg(XDR *,
			 struct lsbMsg *,
			 struct packet_header *);

extern bool_t xdr_controlReq(XDR *,
			     struct controlReq *,
			     struct packet_header *);

extern bool_t xdr_debugReq (XDR *,
			    struct debugReq *,
			    struct packet_header *);

extern bool_t xdr_infoReq(XDR *,
			  struct infoReq *,
			  struct packet_header *);

extern bool_t xdr_parameterInfo(XDR *,
				struct parameterInfo *,
				struct packet_header *);

extern bool_t xdr_userInfoEnt(XDR *,
			       struct userInfoEnt *,
			       struct packet_header *);

extern bool_t xdr_userInfoReply(XDR *,
				struct userInfoReply *,
				struct packet_header *);

extern bool_t xdr_hostInfoEnt(XDR *,
			      struct hostInfoEnt *,
			      struct packet_header *,
			      int *);

extern bool_t xdr_hostDataReply(XDR *,
				struct hostDataReply *,
				struct packet_header *);

extern bool_t xdr_queueInfoEnt(XDR *,
			       struct queueInfoEnt *,
			       struct packet_header *,
			       int *);

extern bool_t xdr_queueInfoReply(XDR *,
				 struct queueInfoReply *,
				 struct packet_header *);

extern bool_t xdr_jobInfoHead(XDR *,
			      struct jobInfoHead *,
			      struct packet_header *);

extern bool_t xdr_jobInfoReply(XDR *,
			       struct jobInfoReply *,
			       struct packet_header *);

extern bool_t xdr_jobInfoEnt(XDR *,
			     struct jobInfoEnt *,
			     struct packet_header *);

extern bool_t xdr_jobInfoReq(XDR *,
			     struct jobInfoReq *,
			     struct packet_header *);

extern bool_t xdr_jobPeekReq(XDR *,
			     struct jobPeekReq *,
			     struct packet_header *);

extern bool_t xdr_jobPeekReply(XDR *,
			       struct jobPeekReply *,
			       struct packet_header *);

extern bool_t xdr_jobMoveReq(XDR *,
			     struct jobMoveReq *,
			     struct packet_header *);

extern bool_t xdr_jobSwitchReq(XDR *,
			       struct jobSwitchReq *,
			       struct packet_header *);

extern bool_t xdr_groupInfoReply(XDR *,
				 struct groupInfoReply *,
				 struct packet_header *);

extern bool_t xdr_groupInfoEnt(XDR *,
			       struct groupInfoEnt *,
			       struct packet_header *);

extern bool_t xdr_migReq(XDR *,
			 struct migReq *,
			 struct packet_header *);

extern bool_t xdr_time_t(XDR *,
			 time_t *);

extern bool_t xdr_xFile(XDR *,
			struct xFile *,
			struct packet_header *);

bool_t xdr_modifyReq(XDR *,
		     struct  modifyReq *,
		     struct packet_header *);

extern bool_t xdr_var_string(XDR *,
			     char **);

extern bool_t xdr_lsbShareResourceInfoReply(XDR *,
					    struct  lsbShareResourceInfoReply *,
					    struct packet_header *hdr);

extern bool_t xdr_runJobReq(XDR *,
			    struct runJobRequest *,
			    struct packet_header *);

extern bool_t xdr_jobAttrReq(XDR *,
			    struct jobAttrInfoEnt *,
			    struct packet_header *);

#endif
