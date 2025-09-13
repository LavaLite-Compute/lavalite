/* $Id: lib.words.h,v 1.2 2007/08/15 22:18:51 tmizan Exp $
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
#ifndef LIB_WORDS_H
#define LIB_WORDS_H

#define NODE_LEFT_DONE  1
#define NODE_ALL_DONE   2 
#define NODE_PASED      3 

#define PUSH_STACK(s, n) \
    {if (pushStack(s, n) < 0) {goto Error;}}

static struct pStack *blockStack;
static struct pStack *ptrStack;

static struct confNode *newNode(void);
static void freeNode(struct confNode *);
static char linkNode(struct confNode *, struct confNode *);
static char *readNextLine(struct lsConf *, int *);

static char addCond(struct lsConf *, char *);
static char checkCond(struct lsConf *, char *);

#endif
