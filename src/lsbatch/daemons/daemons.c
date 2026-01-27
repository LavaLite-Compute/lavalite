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

#include "lsbatch/daemons/daemons.h"

int init_ServSock(u_short port)
{
    int ch;

    ch = chan_listen_socket(SOCK_STREAM, port, SOMAXCONN, CHAN_OP_SOREUSE);
    if (ch < 0) {
        ls_syslog(LOG_ERR, "init_ServSock", "chan_listen_socket");
        return -1;
    }

    return ch;
}

int rcvJobFile(int chfd, struct lenData *jf)
{
    int cc;

    jf->data = NULL;
    jf->len = 0;

    if ((cc = chan_read(chfd, (void *) (&jf->len), NET_INTSIZE_)) !=
        NET_INTSIZE_) {
        ls_syslog(LOG_ERR, "%s: chan_read() failed %m", __func__);
        return -1;
    }

    jf->len = ntohl(jf->len);
    jf->data = calloc(jf->len, sizeof(char));

    if ((cc = chan_read(chfd, jf->data, jf->len)) != jf->len) {
        ls_syslog(LOG_ERR, "%s", __func__, "chan_read");
        free(jf->data);
        return -1;
    }

    return 0;
}

void childRemoveSpoolFile(const char *spoolFile, int options,
                          const struct passwd *pwUser)
{
    // Bug good bye spool
#if 0
    char fname[] = "childRemoveSpoolFile";
    char apiName[] = "ls_initrex";
    pid_t pid;
    char hostName[MAXHOSTNAMELEN];
    char errMsg[MAXLINELEN];
    int status;
    char * fromHost = NULL;
    const char *sp;
    char dirName[MAXFILENAMELEN];

    status = -1;

    if ( ( fromHost =  (char *) getSpoolHostBySpoolFile(spoolFile)) != NULL) {
        strcpy( hostName, fromHost );
    } else {
        ls_syslog(LOG_ERR, "Unable to get spool host from the string \'%s\'"
            , (spoolFile ? spoolFile : "NULL"));
        goto Done;
    }

    if ( pwUser == NULL ) {
        ls_syslog(LOG_ERR, "%s: Parameter const struct passwd * pwUser is NULL!"
            ,fname);
       goto Done;
    }

    sp = getLowestDir_(spoolFile);
    if (sp) {
        strcpy(dirName, sp);
    } else {
        strcpy(dirName, spoolFile);
    }

    sprintf( errMsg, "%s: Unable to remove spool file:\n,\'%s\'\n on host %s\n"
            ,fname, dirName ,fromHost );

    if ( ! (options &  FORK_REMOVE_SPOOL_FILE) ) {

        if ( (options &  CALL_RES_IF_NEEDED) ) {
            if (ls_initrex(1, 0) < 0) {
                status = -1;
                sprintf( errMsg, "%s: %s failed when trying to delete %s from %s\n"
                  ,fname, apiName, dirName, fromHost );
                goto Error;
            }
        }

        chuser( pwUser->pw_uid );

        status = removeSpoolFile( hostName, dirName );

        chuser(batchId);

        if ( status != 0 ) {
            goto Error;
        }
        goto Done;
    }

    switch(pid = fork()) {
        case 0:

            if (debug < 2) {
                 closeExceptFD(-1);
            }

            if ( (options &  CALL_RES_IF_NEEDED) ) {
                if (ls_initrex(1, 0) < 0) {
                    status = -1;
                    sprintf( errMsg, "%s: %s failed when trying to delete %s from %s\n"
                      ,fname, apiName, dirName, fromHost );
                    goto Error;
                }
            }

            chuser( pwUser->pw_uid );
            status = 0;
            if ( removeSpoolFile( hostName, dirName ) == 0 ) {
                exit( 0 );
            } else {
                exit( -1);
            }
            goto Done;
            break;

        case -1:

            if (logclass & (LC_FILE)) {
                ls_syslog(LOG_ERR, "%s", __func__,"fork" );
            }
            status = -1;
            sprintf( errMsg, "%s: Unable to fork to remove spool file:\n,\'%s\'\n on host %s\n"
              ,fname, dirName ,fromHost );

            goto Error;

        default:

            status = 0;
            goto Done;
    }

Error:

    if ( status == -1 )
    {

        lsb_merr(errMsg);
    }
Done:
#endif
}


// enqueue header used by daemons
int
enqueue_header_reply(int ch_id, int rc)
{
    struct Buffer *reply_buf;
    struct packet_header hdr;
    XDR xdrs;

    if (chan_alloc_buf(&reply_buf, PACKET_HEADER_SIZE)) {
        LS_ERR("Failed to allocate buffer");
        return -1;
    }

    init_pack_hdr(&hdr);
    hdr.operation = rc;

    xdrmem_create(&xdrs, reply_buf->data, PACKET_HEADER_SIZE, XDR_ENCODE);
    if (!xdr_pack_hdr(&xdrs, &hdr)) {
        LS_ERR("xdr_pack_hdr failed");
        xdr_destroy(&xdrs);
        chan_free_buf(reply_buf);
        return -1;
    }

    reply_buf->len = xdr_getpos(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, reply_buf) < 0) {
        LS_ERR("chan_enqueue failed");
        chan_free_buf(reply_buf);
        return -1;
    }

    if (chan_set_write_interest(ch_id, true) < 0) {
        LS_ERR("chan_set_write_interest failed");
        return -1;
    }

    return 0;
}

// Used by both mbd and sbd
void freeJobSpecs(struct jobSpecs *spec)
{
    int i;

    if (spec->toHosts != NULL) {
        free(spec->toHosts);
        spec->toHosts = NULL;
        spec->numToHosts = 0;
    }

    if (spec->env != NULL && spec->numEnv > 0) {
        for (i = 0; i < spec->numEnv; i++) {
            FREEUP(spec->env[i]);
        }
        FREEUP(spec->env);
        spec->env = NULL;
        spec->numEnv = 0;
    }

    if (spec->eexec.len > 0 && spec->eexec.data != NULL) {
        FREEUP(spec->eexec.data);
        spec->eexec.data = NULL;
        spec->eexec.len = 0;
    }

    if (spec->job_file_data.len > 0 && spec->job_file_data.data != NULL) {
        FREEUP(spec->job_file_data.data);
        spec->job_file_data.data = NULL;
        spec->job_file_data.len = 0;
    }
}


int enqueue_payload(int ch_id, int op, void *payload, bool_t (*xdr_func)())
{
    return enqueue_payload_bufsiz(ch_id, op, payload, xdr_func, LL_BUFSIZ_4K);
}


// enqueue message this function is shared by daemons
int enqueue_payload_bufsiz(int ch_id, int op,
                           void *payload, bool_t (*xdr_func)(), size_t bufsiz)
{
    struct Buffer *buf;

    if (chan_alloc_buf(&buf, bufsiz) < 0) {
        LS_ERR("chan_alloc_buf failed op=%d bufsiz %d", op, bufsiz);
        return -1;
    }

    XDR xdrs;
    xdrmem_create(&xdrs, buf->data, bufsiz, XDR_ENCODE);

    struct packet_header hdr;
    init_pack_hdr(&hdr);
    hdr.operation = op;

    // xdr_encodeMsg() uses old-style
    // bool_t (*xdr_func)() so we keep the same type.
    if (!xdr_encodeMsg(&xdrs,
                       (char *)payload,
                       &hdr,
                       xdr_func,
                       0,
                       NULL)) {
        LS_ERR("xdr_encodeMsg failed op=%d", op);
        xdr_destroy(&xdrs);
        chan_free_buf(buf);
        return -1;
    }

    buf->len = (size_t)XDR_GETPOS(&xdrs);
    xdr_destroy(&xdrs);

    if (chan_enqueue(ch_id, buf) < 0) {
        LS_ERR("chan_enqueue failed op=%d len=%zu", op, buf->len);
        chan_free_buf(buf);
        return -1;
    }

    return 0;
}

const char * mbd_op_str(mbdReqType op)
{
    switch (op) {

    case BATCH_STATUS_ACK:
        return "BATCH_STATUS_ACK";

    case BATCH_JOB_SUB:
        return "BATCH_JOB_SUB";

    case BATCH_JOB_INFO:
        return "BATCH_JOB_INFO";

    case BATCH_JOB_PEEK:
        return "BATCH_JOB_PEEK";

    case BATCH_JOB_SIG:
        return "BATCH_JOB_SIG";

    case BATCH_HOST_INFO:
        return "BATCH_HOST_INFO";

    case BATCH_QUE_INFO:
        return "BATCH_QUE_INFO";

    case BATCH_GRP_INFO:
        return "BATCH_GRP_INFO";

    case BATCH_QUE_CTRL:
        return "BATCH_QUE_CTRL";

    case BATCH_RECONFIG:
        return "BATCH_RECONFIG";

    case BATCH_HOST_CTRL:
        return "BATCH_HOST_CTRL";

    case BATCH_JOB_SWITCH:
        return "BATCH_JOB_SWITCH";

    case BATCH_JOB_MOVE:
        return "BATCH_JOB_MOVE";

    case BATCH_JOB_MIG:
        return "BATCH_JOB_MIG";

    case BATCH_STATUS_JOB:
        return "BATCH_STATUS_JOB";

    case BATCH_SLAVE_RESTART:
        return "BATCH_SLAVE_RESTART";

    case BATCH_USER_INFO:
        return "BATCH_USER_INFO";

    case BATCH_PARAM_INFO:
        return "BATCH_PARAM_INFO";

    case BATCH_JOB_MODIFY:
        return "BATCH_JOB_MODIFY";

    case BATCH_JOB_EXECED:
        return "BATCH_JOB_EXECED";

    case BATCH_JOB_MSG:
        return "BATCH_JOB_MSG";

    case BATCH_STATUS_MSG_ACK:
        return "BATCH_STATUS_MSG_ACK";

    case BATCH_RESOURCE_INFO:
        return "BATCH_RESOURCE_INFO";

    case BATCH_RUSAGE_JOB:
        return "BATCH_RUSAGE_JOB";

    case BATCH_JOB_FORCE:
        return "BATCH_JOB_FORCE";

    case BATCH_STATUS_CHUNK:
        return "BATCH_STATUS_CHUNK";

    case BATCH_SET_JOB_ATTR:
        return "BATCH_SET_JOB_ATTR";

    case BATCH_SBD_REGISTER:
        return "BATCH_SBD_REGISTER";

    case BATCH_SBD_REGISTER_ACK:
        return "BATCH_SBD_REGISTER_ACK";

    case BATCH_NEW_JOB:
        return "BATCH_NEW_JOB";
    // mbd sbd new protocol
    case BATCH_NEW_JOB_REPLY:
        return "BATCH_NEW_JOB_REPLY";

    case BATCH_NEW_JOB_REPLY_ACK:
        return "BATCH_NEW_JOB_REPLY_ACK";

    case BATCH_JOB_EXECUTE:
        return "BATCH_JOB_EXECUTE";

    case BATCH_JOB_EXECUTE_ACK:
        return "BATCH_JOB_EXECUTE_ACK";

    case BATCH_JOB_FINISH:
        return "BATCH_JOB_FINISH";

    case BATCH_JOB_FINISH_ACK:
        return "BATCH_JOB_FINISH_ACK";

    case BATCH_JOB_SIGNAL:
        return "BATCH_JOB_SIGNAL";

    case BATCH_JOB_SIGNAL_REPLY:
        return "BATCH_JOB_SIGNAL_REPLY";

    case BATCH_JOB_SIGNAL_MANY:
        return "BATCH_JOB_SIGNAL_MANY";

    case BATCH_JOB_SIGNAL_MANY_REPLY:
        return "BATCH_JOB_SIGNAL_MANY_REPLY";

    case BATCH_LAST_OP:
        return "BATCH_LAST_OP";

    default:
        return "UNKNOWN_MBD_OP";
    }
}

const char * job_state_str(int status)
{
    if (status & JOB_STAT_DONE)
        return "JOB_STAT_DONE";

    if (status & JOB_STAT_EXIT)
        return "JOB_STAT_EXIT";

    if (status & JOB_STAT_RUN)
        return "JOB_STAT_RUN";

    if (status & JOB_STAT_USUSP)
        return "JOB_STAT_USUSP";

    if (status & JOB_STAT_SSUSP)
        return "JOB_STAT_SSUSP";

    if (status & JOB_STAT_PSUSP)
        return "JOB_STAT_PSUSP";

    if (status & JOB_STAT_PEND)
        return "JOB_STAT_PEND";

    if (status == JOB_STAT_NULL)
        return "JOB_STAT_NULL";

    return "UNKNOWN_STATE";
}
