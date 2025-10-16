/* $Id: lsinfo.c,v 1.3 2007/08/15 22:18:55 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/lib.h"

static void usage(char *);
static void print_long(struct resItem *);
static char nameInList(char **, int, char *);
static char *flagToStr(int);
static char *orderTypeToStr(enum orderType);
static char *valueTypeToStr(enum valueType);

#define NL_SETN 27


int
main(int argc, char **argv)
{
    static char fname[] = "lsinfo/main";
    struct lsInfo *lsInfo;
    int i, cc, nnames;
    char *namebufs[256];
    char longFormat = false;
    char rFlag = false;
    char tFlag = false;
    char mFlag = false;
    char mmFlag = false;
    extern int optind;

    _i18n_init ( I18N_CAT_MIN );

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    if (logclass & (LC_TRACE))
        ls_syslog(LOG_DEBUG, "%s: Entering this routine...", fname);

    while ((cc = getopt(argc, argv, "VhlrmMt")) != EOF) {
        switch(cc) {
        case 'V':
            fputs(_LAVALITE_VERSION_, stderr);
            exit(0);
        case 'l':
            longFormat = true;
            break;
        case 'r':
            rFlag = true;
            break;
        case 't':
            tFlag = true;
            break;
        case 'm':
            mFlag = true;
            break;
        case 'M':
            mFlag  = true;
            mmFlag = true;
            break;
        case 'h':
        default:
            usage(argv[0]);
        }
    }

    for (nnames=0; optind < argc; optind++, nnames++)
        namebufs[nnames] = argv[optind];

    if ((lsInfo = ls_info()) == NULL) {
        ls_perror("lsinfo");
        exit(-10);
    }


    if (!nnames && !rFlag && !mFlag && !tFlag && !mmFlag)
        rFlag = mFlag = tFlag = true;
    else if (nnames)
        rFlag = true;

    if (rFlag) {
        if (!longFormat) {
            char *buf1, *buf2, *buf3, *buf4;

            buf1 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1804, "RESOURCE_NAME")), /* catgets  1804  */
                buf2 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1805, "  TYPE ")), /* catgets  1805  */
                buf3 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1806, "ORDER")), /* catgets  1806  */
                buf4 = putstr_(_i18n_msg_get(ls_catd,NL_SETN,1807, "DESCRIPTION")), /* catgets  1807  */

                printf("%-13.13s %7.7s  %5.5s  %s\n",
                       buf1, buf2, buf3, buf4);

            FREEUP(buf1);
            FREEUP(buf2);
            FREEUP(buf3);
            FREEUP(buf4);
        }

        for (i=0; i < lsInfo->nRes; i++) {
            if (!nameInList(namebufs, nnames, lsInfo->resTable[i].name))
                continue;
            if (!longFormat) {
                printf("%-13.13s %7.7s %5.5s   %s\n",
                       lsInfo->resTable[i].name,
                       valueTypeToStr(lsInfo->resTable[i].valueType),
                       orderTypeToStr(lsInfo->resTable[i].orderType),
                       lsInfo->resTable[i].des);
            } else
                print_long(&(lsInfo->resTable[i]));
        }

        for (i=0; i < nnames; i++)
            if (namebufs[i])
                printf(_i18n_msg_get(ls_catd,NL_SETN,1808, "%s: Resource name not found\n"),/* catgets  1808  */
                       namebufs[i]);

    }

    if (tFlag) {
        if (rFlag)
            putchar('\n');
        puts(_i18n_msg_get(ls_catd,NL_SETN,1809, "TYPE_NAME")); /* catgets  1809  */
        for (i=0;i<lsInfo->nTypes;i++)
            puts(lsInfo->hostTypes[i]);
    }

    if (mFlag) {
        if (rFlag || tFlag)
            putchar('\n');
        puts(_i18n_msg_get(ls_catd,NL_SETN,1810,
                           "MODEL_NAME      CPU_FACTOR      ARCHITECTURE")); /* catgets  1810  */
        for (i = 0; i < lsInfo->nModels; ++i)
            if (mmFlag || lsInfo->modelRefs[i])
                printf("%-16s    %6.2f      %s\n", lsInfo->hostModels[i],
                       lsInfo->cpuFactor[i], lsInfo->hostArchs[i]);
    }

    _i18n_end ( ls_catd );

    exit(0);
}

static void
usage(char *cmd)
{
    fprintf (stderr, "%s: %s [-h] [-V] [-l] [-r] [-m] [-M] [-t] [resource_name ...]\n",I18N_Usage, cmd);
    exit(-1);
}

static void
print_long(struct resItem *res)
{

    char tempStr[15];
    static int first = true;

    if (first) {
        printf("%s:  %s\n",
               _i18n_msg_get(ls_catd,NL_SETN,1812, "RESOURCE_NAME"), /* catgets  1812  */
               res->name);
        first = false;
    } else
        printf("\n%s:  %s\n",
               _i18n_msg_get(ls_catd,NL_SETN,1812, "RESOURCE_NAME"),
               res->name);
    printf(_i18n_msg_get(ls_catd,NL_SETN,1814, "DESCRIPTION: %s\n"),res->des); /* catgets  1814  */

    printf("%-7.7s ", I18N(15, "TYPE"));    /* catgets  1815  */
    printf("%5s  ",   I18N(1816, "ORDER"));     /* catgets  1816  */
    printf("%9s ",    I18N(1817, "INTERVAL"));  /* catgets  1817  */
    printf("%8s ",    I18N(1818, "BUILTIN"));   /* catgets  1818  */
    printf("%8s ",    I18N(1819, "DYNAMIC"));   /* catgets  1819  */
    printf("%8s\n",   I18N(1820, "RELEASE"));   /* catgets  1820  */

    sprintf(tempStr,"%d",res->interval);
    printf("%-7.7s %5s  %9s %8s %8s %8s\n",
           valueTypeToStr(res->valueType),
           orderTypeToStr(res->orderType),
           tempStr,
           flagToStr(res->flags & RESF_BUILTIN),
           flagToStr(res->flags & RESF_DYNAMIC),
           flagToStr(res->flags & RESF_RELEASE));
}

static char
*flagToStr(int flag)
{
    static char *sp = NULL;
    if (flag)
        sp = I18N_Yes;
    else
        sp = I18N_No;
    return sp;
}

static char
*valueTypeToStr(enum valueType valtype)
{
    static char *type = NULL;

    switch(valtype) {
    case LS_NUMERIC:
        type = _i18n_msg_get(ls_catd,NL_SETN,1822, "Numeric"); /* catgets  1822  */
        break;
    case LS_BOOLEAN:
        type = _i18n_msg_get(ls_catd,NL_SETN,1823, "Boolean"); /* catgets  1823  */
        break;
    default:
        type = _i18n_msg_get(ls_catd,NL_SETN,1824, "String"); /* catgets  1824  */
        break;
    }
    return type;
}

static char
*orderTypeToStr(enum orderType ordertype)
{
    char *order;
    switch(ordertype) {
    case INCR:
        order = _i18n_msg_get(ls_catd,NL_SETN,1825, "Inc"); /* catgets  1825  */
        break;
    case DECR:
        order = _i18n_msg_get(ls_catd,NL_SETN,1826, "Dec"); /* catgets  1826  */
        break;
    default:
        order = _i18n_msg_get(ls_catd,NL_SETN,1827, "N/A"); /* catgets  1827  */
        break;
    }
    return order;
}

static char
nameInList(char **namelist, int listsize, char *name)
{
    int i, j;

    if (listsize == 0)
        return true;

    for (i=0; i < listsize; i++) {
        if (!namelist[i])
            continue;
        if (strcmp(name, namelist[i]) == 0) {
            namelist[i] = NULL;

            for (j=i+1; j < listsize; j++) {
                if(!namelist[j])
                    continue;
                if (strcmp(name, namelist[j]) == 0)
                    namelist[j] = NULL;
            }
            return true;
        }
    }
    return false;
}
