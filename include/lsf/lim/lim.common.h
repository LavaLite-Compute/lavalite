/* $Id: lim.common.h,v 1.7 2007/08/15 22:18:53 tmizan Exp $
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

#include "lim.h"
#include <math.h>

extern int pipefd[2];
extern struct limLock limLock;

void sendLoad(void);

int  maxnLbHost;
int  ncpus=1;

float cpu_usage = 0.0;
#ifdef MEAS
float realcla = 0.0;
int sd_cnt = 0;
int rcv_cnt = -1;
#endif
