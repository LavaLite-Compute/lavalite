/*
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#pragma once

// Pad the n onject possible strlen(str)
#define XDR_PADLEN(n) (((4 - ((n) % 4)) % 4))
#define XDR_STRLEN(n) (4 + (n) + XDR_PADLEN(n))

// The XDR stream, the data structure, the context and the
// encode/decode function
bool_t xdr_array_element(XDR *, void *, void *, bool_t (*)());

// This is the type of the encode/decode function
typedef bool_t (*xdr_func_t)(XDR *, void *, void *);

// Generic XDR functions used throught the system libraries
void xdr_lsffree(bool_t (*)(), void *, struct packet_header *);
bool_t xdr_time_t(XDR *, time_t *);
bool_t xdr_lsfRusage(XDR *, struct lsfRusage *, void *);
bool_t xdr_lvector(XDR *, float *, uint32_t);
bool_t xdr_array_string(XDR *, char **, int, int);
bool_t xdr_var_string(XDR *, char **);
bool_t xdr_stringLen(XDR *, struct stringLen *, struct packet_header *);
bool_t xdr_lenData(XDR *, struct lenData *);
bool_t xdr_lsfLimit(XDR *, struct lsfLimit *, struct packet_header *);
bool_t xdr_portno(XDR *, uint16_t *);
bool_t xdr_sockaddr_in(XDR *, struct sockaddr_in *);

// Protocol related xdr
extern bool_t xdr_pack_hdr(XDR *, struct packet_header *);
extern bool_t xdr_encodeMsg(XDR *, char *, struct packet_header *, bool_t (*)(),
                            int, struct lsfAuth *);
extern bool_t xdr_arrayElement(XDR *, char *, struct packet_header *,
                               bool_t (*)(), ...);
extern bool_t xdr_stringLen(XDR *, struct stringLen *, struct packet_header *);
extern bool_t xdr_lsfAuth(XDR *, struct lsfAuth *, struct packet_header *);
extern int xdr_lsfAuthSize(struct lsfAuth *);

// lim XDR functions
bool_t xdr_decisionReq(XDR *, struct decisionReq *, struct packet_header *);
bool_t xdr_placeReply(XDR *, struct placeReply *, struct packet_header *);
bool_t xdr_loadReply(XDR *, struct loadReply *, struct packet_header *);
bool_t xdr_jobXfer(XDR *, struct jobXfer *, struct packet_header *);
bool_t xdr_hostInfo(XDR *, struct shortHInfo *, struct packet_header *);
bool_t xdr_limLock(XDR *, struct limLock *, struct packet_header *);
bool_t xdr_lsInfo(XDR *, struct lsInfo *, struct packet_header *);
bool_t xdr_hostInfoReply(XDR *, struct hostInfoReply *, struct packet_header *);
bool_t xdr_masterInfo(XDR *, struct masterInfo *, struct packet_header *);
bool_t xdr_clusterInfoReq(XDR *, struct clusterInfoReq *,
                          struct packet_header *);
bool_t xdr_clusterInfoReply(XDR *, struct clusterInfoReply *,
                            struct packet_header *);
bool_t xdr_shortHInfo(XDR *, struct shortHInfo *, void *);
bool_t xdr_shortCInfo(XDR *, struct shortCInfo *, void *);
bool_t xdr_cInfo(XDR *, struct cInfo *, struct packet_header *);
bool_t xdr_resourceInfoReq(XDR *, struct resourceInfoReq *,
                           struct packet_header *);
bool_t xdr_resourceInfoReply(XDR *, struct resourceInfoReply *, void *);
bool_t xdr_jRusage(XDR *, struct jRusage *, void *);
// wire it the ll naming for bytes on the wire in network format
// host info
bool_t xdr_wire_host_info(XDR *, struct wire_host_info *,
                          struct packet_header *);
bool_t xdr_wire_host_info_reply(XDR *, struct wire_host_info_reply *,
                                struct packet_header *);
// host load info
bool_t xdr_wire_load_info(XDR *, struct wire_load_info *);
bool_t xdr_wire_load_info_reply(XDR *, struct wire_load_info_reply *);

// ls_info()
bool_t xdr_wire_res_item(XDR *, struct wire_res_item *);
bool_t xdr_wire_host_type(XDR *, struct wire_host_type *);
bool_t xdr_wire_host_model(XDR *, struct wire_host_model *);
bool_t xdr_wire_lsinfo_reply(XDR *, struct wire_lsinfo_reply *);

// ls_clusterinfo()
bool_t xdr_wire_cluster_info(XDR *, struct wire_cluster_info *);
bool_t xdr_wire_cluster_info_reply(XDR *, struct wire_cluster_info_reply *);
