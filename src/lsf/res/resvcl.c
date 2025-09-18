/* $Id: resvcl.c,v 1.2 2007/08/15 22:18:56 tmizan Exp $
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "../lsf.h"

#include "../../pass/lib/pam.h"

#define RES_VCL_VERSION_NAME        "res_vcl_version"
#define RES_VCL_CLUSTER_INIT_NAME   "res_vcl_cluster_init"
#define RES_VCL_NODE_INIT_NAME      "res_vcl_node_init"
#define RES_VCL_PRE_SPAWN_NAME      "res_vcl_pre_spawn"
#define RES_VCL_PRE_EXEC_NAME       "res_vcl_pre_exec"
#define RES_VCL_ATEXIT_NAME         "res_vcl_atexit"
#define RES_VCL_NODE_FINALIZE_NAME  "res_vcl_node_finalize"

char *fname = "res_vcl_test";

int
get_token(rmi_buffer_t *buffer)
{
  srand((unsigned int) buffer->size);
  if ( logclass & LC_EXEC) { 
      ls_syslog(LOG_DEBUG, "get_token: token=%d",rand());
  } 
  return 0;
}

int
get_rand(rmi_buffer_t *buffer)
{
  srand((unsigned int) buffer->size + 31417);
  if ( logclass & LC_EXEC) { 
      ls_syslog(LOG_DEBUG, "get_rand: randn=%d",rand());
  }
  return 0;
}

rmi_handler_t my_handlers[] = {
    {"get_token", get_token, "get a random number"},
    {"get_rand", get_rand, "get a random number"},
    {(char *) NULL, (rmi_handler_func_t) NULL, (char *) NULL}
};

int
res_vcl_version(void)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_VERSION_NAME);
    }
    return PAM_VERSION;
}

int
res_vcl_cluster_init(int master)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s at %d",fname,
    	    RES_VCL_CLUSTER_INIT_NAME, master);
    }
    return 0;
}

int
res_vcl_node_init(int *argc, char ***argv, char ***env,
    rmi_handler_t **handler_table, size_t *handler_table_size)
{
    *handler_table = my_handlers;
    *handler_table_size = 2;
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_NODE_INIT_NAME);
    }
    return 0;
}

int
res_vcl_pre_spawn(ls_task_params_t *procinfo)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_PRE_SPAWN_NAME);
    }
    return 0;
}

int
res_vcl_pre_exec(ls_task_t *taskinfo)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_PRE_EXEC_NAME);
    } 
    return 0;
}

int
res_vcl_atexit(ls_task_t *taskinfo)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_ATEXIT_NAME);
    }
    return 0;
}

int
res_vcl_node_finalize(void)
{
    if ( logclass & LC_EXEC) { 
        ls_syslog(LOG_DEBUG,"%s: This is %s",fname,RES_VCL_NODE_FINALIZE_NAME);
    } 
    return 0;
}
