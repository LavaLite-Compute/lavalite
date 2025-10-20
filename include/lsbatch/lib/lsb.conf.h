/* $Id: lsb.conf.h,v 1.3 2007/08/15 22:18:47 tmizan Exp $
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
#ifndef LSB_CONF_H
#define LSB_CONF_H

#define TYPE1  RESF_BUILTIN | RESF_DYNAMIC | RESF_GLOBAL
#define TYPE2  RESF_BUILTIN | RESF_GLOBAL

static char do_Param(struct Conf *, char *, int *);
static char do_Users(struct Conf *, char *, int *);
static char do_Hosts(struct Conf *, char *, int *, struct lsInfo *);
static char do_Queues(struct Conf *, char *, int *, struct lsInfo *);
static char do_hPartition(struct Conf *, char *, int *);
static char do_Groups(struct groupInfoEnt **, struct Conf *, char *,
					int *, int *);

static char addHost(struct hostInfoEnt *, char *, int);
static char addQueue(struct queueInfoEnt *, char *, int);
static char addUser (char *, int, float, char *, int, int); 
static char addMember(struct groupInfoEnt *, char *, int, char *,
					int, char *);

static char isInGrp (char *, struct groupInfoEnt *, int);
static char **expandGrp(char *, int *, int);
static struct groupInfoEnt *addGroup(struct groupInfoEnt **, char *, int *, int);


static char *parseGroups (char *, char *, int *, char *, int);

static struct groupInfoEnt *getUGrpData(char *);
static struct groupInfoEnt *getHGrpData(char *);
static struct groupInfoEnt *getGrpData(struct groupInfoEnt **, char *, int);
static struct userInfoEnt *getUserData(char *);
static struct hostInfoEnt *getHostData(char *);
static struct queueInfoEnt *getQueueData(char *);

static void initParameterInfo ( struct parameterInfo *);
static void freeParameterInfo ( struct parameterInfo *);
static void initUserInfo ( struct userInfoEnt *);
static void freeUserInfo ( struct userInfoEnt *);
static void initGroupInfo ( struct groupInfoEnt *);
static void freeGroupInfo ( struct groupInfoEnt *);
static void initHostInfo ( struct hostInfoEnt *);
static void freeHostInfo ( struct hostInfoEnt *);
static void initQueueInfo ( struct queueInfoEnt *);
static void freeQueueInfo ( struct queueInfoEnt *);

static void freeWorkUser ( int, int );
static void freeWorkHost ( int, int, int );
static void freeWorkQueue ( int );

static char threshValue(struct lsInfo *, float *, float *);
static void initThresholds(struct lsInfo *, float *, float *);
static void getThresh(struct lsInfo *, struct keymap *, float *, float *,
					char *, int *, char *);

static char searchAll(char *);
static char checkRequeEValues(struct queueInfoEnt *, char *, char *, int *);
static char parseCpuLimit(struct keymap, struct queueInfoEnt *, char *,
					int *, char *);
static char parseNqsQueues(struct queueInfoEnt *, char *, char *, int *);

static int my_atoi(char *, int, int);
static float my_atof (char *, float, float);

#endif  
