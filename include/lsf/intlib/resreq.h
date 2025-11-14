/* $Id: resreq.h,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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
#ifndef _RES_REQ_H_
#define _RES_REQ_H_

#ifndef MIN
#define MIN(x, y) (((x) <= (y)) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) (((x) >= (y)) ? (x) : (y))
#endif

#define IS_DIGIT(s) ((s) >= '0' && (s) <= '9')
#define IS_LETTER(s) (((s) >= 'a' && (s) <= 'z') || ((s) >= 'A' && (s) <= 'Z'))
#define IS_VALID_OTHER(s) ((s) == '_' || (s) == '~')

#define WILDCARD_STR "any"
#define LOCAL_STR "local"

#define PR_SELECT 0x01
#define PR_ORDER 0x02
#define PR_RUSAGE 0x04
#define PR_FILTER 0x08
#define PR_DEFFROMTYPE 0x10
#define PR_BATCH 0x20
#define PR_SPAN 0x40
#define PR_XOR 0x80

#define PR_ALL PR_SELECT | PR_ORDER | PR_RUSAGE | PR_SPAN

#define PARSE_OK 0
#define PARSE_BAD_EXP -1
#define PARSE_BAD_NAME -2
#define PARSE_BAD_VAL -3
#define PARSE_BAD_FILTER -4
#define PARSE_BAD_MEM -5

#define IDLETIME 5

struct resVal {
    char *selectStr;
    int nphase;
    int order[NBUILTINDEX];
    int genClass;
    float *val;
    int nindex;
    int *indicies;
    int *rusgBitMaps;
    int duration;
    float decay;
    int numHosts;
    int maxNumHosts;
    int pTile;
    int options;
    int selectStrSize;
    char **xorExprs;
};

extern int getValPair(char **resReq, int *val1, int *val2);

#endif
