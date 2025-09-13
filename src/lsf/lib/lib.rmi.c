
/* $Id: lib.rmi.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/liblsf.h"



static exceptHandleTblNode_t *exceptTblHead; 

void *
ls_handle(void)
{
    handleNode_t *rc;

    rc = (handleNode_t *)malloc(sizeof(handleNode_t));
    rc->Errno = 0;
    rc->exceptTblHead = NULL;

    return rc;
}

int 
ls_catch( void *handle, char *key, exceptProto func)
{
    exceptHandleTblNode_t *tmpNode;

    tmpNode = (exceptHandleTblNode_t *)malloc(sizeof(exceptHandleTblNode_t));
    if (! tmpNode){
        return -1;
    }
    tmpNode->key = putstr_(key);
    tmpNode->func = func;
    
    tmpNode->next = exceptTblHead;
    exceptTblHead = tmpNode;
    
    return 0;
}

int 
ls_throw(void *handle, char *key)
{
    exceptHandleTblNode_t *tmpNode;

    for (tmpNode = exceptTblHead; 
        tmpNode; tmpNode = tmpNode->next){
        if (!strcmp(tmpNode->key, key)){
            break;
        }
    }
    if (tmpNode){
        return (*tmpNode->func)();
    }
    return -1;
}

char *
ls_sperror(char *usrMsg)
{
    char errmsg[256];
    char *rtstr;
    
    errmsg[0] = '\0';
 
    if (usrMsg)
        sprintf(errmsg, "%s: ", usrMsg);
 
    strcat(errmsg, ls_sysmsg());

    if ((rtstr=(char *)malloc(sizeof(char)*(strlen(errmsg)+1))) == NULL){
        lserrno = LSE_MALLOC;
        return NULL;
    }

    strcpy(rtstr, errmsg);
    return rtstr;
}
