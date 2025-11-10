#pragma once
/*
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

/* Library parameters defining entry in genParams_[] array
 * every system components has its own array of these variables
 * but in different order as some variables are specific to a
 * different component
 */

typedef enum {
    LSF_CONFDIR,
    LSF_SERVERDIR,
    LSF_LIM_DEBUG,
    LSF_RES_DEBUG,
    LSF_STRIP_DOMAIN,
    LSF_LIM_PORT,
    LSF_LOG_MASK,
    LSF_SERVER_HOSTS,
    LSF_AUTH,
    LSF_ID_PORT,
    LSF_RES_TIMEOUT,
    LSF_API_CONNTIMEOUT,
    LSF_API_RECVTIMEOUT,
    LSF_TMPDIR,
    LSF_LOGDIR,
    LSF_MASTER_LIST,
    LSF_INTERACTIVE_STDERR,
} lib_params_t;
