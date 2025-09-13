/* $Id: lib.rmi.h,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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

#ifndef LIB_RMI_H
#define LIB_RMI_H

typedef struct exceptHandleTblNode{
    char *key;
    exceptProto func;
    struct exceptHandleTblNode *next;
} exceptHandleTblNode_t;

typedef struct handleNode {
    int Errno;
    exceptHandleTblNode_t *exceptTblHead;
    char errmsg[256];
} handleNode_t;
 
#endif 


