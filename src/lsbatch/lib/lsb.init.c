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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA
 */

#include "lsbatch/lib/lsb.h"

struct config_param lsbParams[LSB_PARAM_COUNT] =
{
    [LSB_DEBUG]                = { "LSB_DEBUG", NULL },
    [LSB_CONFDIR]              = { "LSB_CONFDIR", NULL },
    [LSB_SHAREDIR]             = { "LSB_SHAREDIR", NULL },
    [LSB_MAILTO]               = { "LSB_MAILTO", NULL },
    [LSB_MAILPROG]             = { "LSB_MAILPROG", NULL },
    [LSB_DEBUG_MBD]            = { "LSB_DEBUG_MBD", NULL },
    [LSB_TIME_MBD]             = { "LSB_TIME_MBD", NULL },
    [LSB_SIGSTOP]              = { "LSB_SIGSTOP", NULL },
    [LSB_MBD_CONNTIMEOUT]      = { "LSB_MBD_CONNTIMEOUT", NULL },
    [LSB_MBD_MAILREPLAY]       = { "LSB_MBD_MAILREPLAY", NULL },
    [LSB_MBD_MIGTOPEND]        = { "LSB_MBD_MIGTOPEND", NULL },
    [LSB_MBD_BLOCK_SEND]       = { "LSB_MBD_BLOCK_SEND", NULL },
    [LSB_MEMLIMIT_ENFORCE]     = { "LSB_MEMLIMIT_ENFORCE", NULL },
    [LSB_BSUBI_OLD]            = { "LSB_BSUBI_OLD", NULL },
    [LSB_STOP_IGNORE_IT]       = { "LSB_STOP_IGNORE_IT", NULL },
    [LSB_HJOB_PER_SESSION]     = { "LSB_HJOB_PER_SESSION", NULL },
    [LSB_REQUEUE_HOLD]         = { "LSB_REQUEUE_HOLD", NULL },
    [LSB_SMTP_SERVER]          = { "LSB_SMTP_SERVER", NULL },
    [LSB_MAILSERVER]           = { "LSB_MAILSERVER", NULL },
    [LSB_MAILSIZE_LIMIT]       = { "LSB_MAILSIZE_LIMIT", NULL },
    [LSB_REQUEUE_TO_BOTTOM]    = { "LSB_REQUEUE_TO_BOTTOM", NULL },
    [LSB_ARRAY_SCHED_ORDER]    = { "LSB_ARRAY_SCHED_ORDER", NULL },
    [LSB_QPOST_EXEC_ENFORCE]   = { "LSB_QPOST_EXEC_ENFORCE", NULL },
    [LSB_MIG2PEND]             = { "LSB_MIG2PEND", NULL },
    [LSB_UTMP]                 = { "LSB_UTMP", NULL },
    [LSB_JOB_CPULIMIT]         = { "LSB_JOB_CPULIMIT", NULL },
    [LSB_JOB_MEMLIMIT]         = { "LSB_JOB_MEMLIMIT", NULL },
    [LSB_MOD_ALL_JOBS]         = { "LSB_MOD_ALL_JOBS", NULL },
    [LSB_SET_TMPDIR]           = { "LSB_SET_TMPDIR", NULL },
    [LSB_PTILE_PACK]           = { "LSB_PTILE_PACK", NULL },
    [LSB_STDOUT_DIRECT]        = { "LSB_STDOUT_DIRECT", NULL },
    [LSB_NO_FORK]              = { "LSB_NO_FORK", NULL },
    [LSB_DEBUG_CMD]            = { "LSB_DEBUG_CMD", NULL },
    [LSB_TIME_CMD]             = { "LSB_TIME_CMD", NULL },
    [LSB_CMD_LOGDIR]           = { "LSB_CMD_LOGDIR", NULL },
    [LSB_CMD_LOG_MASK]         = { "LSB_CMD_LOG_MASK", NULL },
    [LSB_API_CONNTIMEOUT]      = { "LSB_API_CONNTIMEOUT", NULL },
    [LSB_API_RECVTIMEOUT]      = { "LSB_API_RECVTIMEOUT", NULL },
    [LSB_SERVERDIR]            = { "LSB_SERVERDIR", NULL },
    [LSB_SHORT_HOSTLIST]       = { "LSB_SHORT_HOSTLIST", NULL },
    [LSB_INTERACTIVE_STDERR]   = { "LSB_INTERACTIVE_STDERR", NULL },
     // Legacy placeholder several code depend on this...
    [LSB_NULL_PARAM] = {NULL, NULL},
};

_Static_assert(
    (sizeof(lsbParams) / sizeof(lsbParams[0])) == LSB_PARAM_COUNT,
    "lsbParams[] size mismatch vs enum daemon_param_id"
);

int _lsb_conntimeout = DEFAULT_API_CONNTIMEOUT;
int _lsb_recvtimeout = DEFAULT_API_RECVTIMEOUT;
int _lsb_fakesetuid = 0;

int lsbMode_ = LSB_MODE_BATCH;

extern int bExceptionTabInit(void);
extern int mySubUsage_(void *);

// LavaLite we dont want the library to log. The library has to follow
// a contract based on it interface that's all
int lsb_init(char *appName)
{
    static int lsbenvset = false;

    // Unused
    (void)appName;

    if (lsbenvset)
        return 0;

    // LavaLite this call will fill genParams as well
    if (initenv_(lsbParams, NULL) < 0) {
        lsberrno = LSBE_LSLIB;
        return -1;
    }

    if (lsbParams[LSB_API_CONNTIMEOUT].paramValue) {
        _lsb_conntimeout = atoi(lsbParams[LSB_API_CONNTIMEOUT].paramValue);
        if (_lsb_conntimeout < 0)
            _lsb_conntimeout = DEFAULT_API_CONNTIMEOUT;
    }

    if (lsbParams[LSB_API_RECVTIMEOUT].paramValue) {
        _lsb_recvtimeout = atoi(lsbParams[LSB_API_RECVTIMEOUT].paramValue);
        if (_lsb_recvtimeout < 0)
            _lsb_recvtimeout = DEFAULT_API_RECVTIMEOUT;
    }

    if (!lsbParams[LSB_SHAREDIR].paramValue) {
        lsberrno = LSBE_NO_ENV;
        return -1;
    }

    lsbenvset = true;

    return 0;
}

uint16_t get_mbd_port(void)
{
    uint16_t mbd_port = 0;

    if (!genParams[LSB_MBD_PORT].paramValue) {
        mbd_port = 0;
        lsberrno = LSBE_SERVICE;
        return 0;
    }
    mbd_port = atoi(genParams[LSB_MBD_PORT].paramValue);

    return mbd_port;
}

uint16_t get_sbd_port(void)
{
    uint16_t sbd_port;

    if (!genParams[LSB_SBD_PORT].paramValue) {
        sbd_port = 0;
        lsberrno = LSBE_SERVICE;
        return 0;
    }

    sbd_port = atoi(genParams[LSB_SBD_PORT].paramValue);

    return sbd_port;
}
