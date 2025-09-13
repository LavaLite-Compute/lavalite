/* $Id: cmd.bqc.c,v 1.2 2007/08/15 22:18:44 tmizan Exp $
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

#include "cmd.h"

#define NL_SETN 8 	

static int ctrlQueue (char *, int);
static int exitrc = 0;

int
bqc (int argc, char *argv[], int opCode)
{
    extern int optind;
    struct queueInfoEnt *queueInfo;
    char **queueList=NULL, **queues ;
    int numQueues, all = FALSE; 
    int i; 

    queues = NULL;
    if (argc == optind)              
        numQueues =1;
    else {
        numQueues = getNames (argc, argv, optind, &queueList, &all, "queueC");
        if (!all)
            queues = queueList;
    }

    if ((queueInfo = lsb_queueinfo(queues, &numQueues, NULL, NULL, 0))
        == NULL) {
        if (lsberrno == LSBE_BAD_QUEUE 
            && queues && queues[numQueues])
            lsb_perror(queues[numQueues]);
        else
            lsb_perror(NULL);
        return (-1);
    }

    for (i = 0; i < numQueues; i++) {
        ctrlQueue (queueInfo[i].queue, opCode);
    }
    return (exitrc);
} 
static int
ctrlQueue (char *queue, int opCode)
{

    if (lsb_queuecontrol(queue, opCode) < 0) {
        exitrc = -1;
        switch (lsberrno) {
        case LSBE_BAD_QUEUE:
        case LSBE_QUEUE_WINDOW:
        case LSBE_PERMISSION:
        case LSBE_BAD_USER:
        case LSBE_PROTOCOL:
        case LSBE_MBATCHD:
        default:
            lsb_perror (queue);
            return (-1);                   
        }
    }
    
    switch (opCode) {
    case QUEUE_OPEN:
        printf((_i18n_msg_get(ls_catd,NL_SETN,951, "Queue <%s> is opened\n")), queue); /* catgets  951  */
        break;
    case QUEUE_CLOSED:
        printf((_i18n_msg_get(ls_catd,NL_SETN,952, "Queue <%s> is closed\n")), queue); /* catgets  952  */
        break;
    case QUEUE_ACTIVATE:
	printf((_i18n_msg_get(ls_catd,NL_SETN,953, "Queue <%s> is activated\n")), queue); /* catgets  953  */
        break;
    case QUEUE_INACTIVATE:
        printf((_i18n_msg_get(ls_catd,NL_SETN,954, "Queue <%s> is inactivated\n")), queue); /* catgets  954  */
        break;
    default:
        fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,955, "Command internal error: corrupt opCode\n"))); /* catgets  955  */
        return (-1);
    }
    return (0);
} 

