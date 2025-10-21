/* $Id: lib.lsf.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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
#include "lsf/lib/lib.common.h"

#ifndef LSFRU_FIELD_ADD
#define LSFRU_FIELD_ADD(a,b) \
{ \
    if ((a) < 0 || (b) < 0) { \
        (a) = MAX((a), (b)); \
    } else { \
        (a) += (b); \
    } \
}
#endif

void
ls_ruunix2lsf(struct rusage *rusage, struct lsfRusage *lsfRusage)
{
    lsfRusage->ru_utime = rusage->ru_utime.tv_sec
        + rusage->ru_utime.tv_usec / 1000000.0;

    lsfRusage->ru_stime = rusage->ru_stime.tv_sec
        + rusage->ru_stime.tv_usec / 1000000.0;

    lsfRusage->ru_maxrss = rusage->ru_maxrss;
    lsfRusage->ru_ixrss = rusage->ru_ixrss;

    lsfRusage->ru_ismrss = -1.0;

    lsfRusage->ru_isrss = rusage->ru_isrss;
    lsfRusage->ru_minflt = rusage->ru_minflt;
    lsfRusage->ru_majflt = rusage->ru_majflt;
    lsfRusage->ru_nswap = rusage->ru_nswap;
    lsfRusage->ru_inblock = rusage->ru_inblock;
    lsfRusage->ru_oublock = rusage->ru_oublock;

    lsfRusage->ru_ioch = -1.0;

    lsfRusage->ru_idrss = rusage->ru_idrss;
    lsfRusage->ru_msgsnd = rusage->ru_msgsnd;
    lsfRusage->ru_msgrcv = rusage->ru_msgrcv;
    lsfRusage->ru_nsignals = rusage->ru_nsignals;
    lsfRusage->ru_nvcsw = rusage->ru_nvcsw;
    lsfRusage->ru_nivcsw = rusage->ru_nivcsw;
    lsfRusage->ru_exutime = -1.0;
}

void
ls_rulsf2unix(struct lsfRusage *lsfRusage, struct rusage *rusage)
{
    rusage->ru_utime.tv_sec = MIN( lsfRusage->ru_utime, LONG_MAX );
    rusage->ru_stime.tv_sec = MIN( lsfRusage->ru_stime, LONG_MAX );

    rusage->ru_utime.tv_usec = MIN(( lsfRusage->ru_utime
				- rusage->ru_utime.tv_sec) * 1000000, LONG_MAX);
    rusage->ru_stime.tv_usec = MIN(( lsfRusage->ru_stime
				- rusage->ru_stime.tv_sec) * 1000000, LONG_MAX);

    rusage->ru_maxrss = MIN( lsfRusage->ru_maxrss, LONG_MAX );
    rusage->ru_ixrss = MIN( lsfRusage->ru_ixrss, LONG_MAX );
    rusage->ru_isrss = MIN( lsfRusage->ru_isrss, LONG_MAX );

    rusage->ru_idrss = MIN( lsfRusage->ru_idrss, LONG_MAX );
    rusage->ru_isrss = MIN( lsfRusage->ru_isrss, LONG_MAX );
    rusage->ru_minflt = MIN( lsfRusage->ru_minflt, LONG_MAX );
    rusage->ru_majflt = MIN( lsfRusage->ru_majflt, LONG_MAX );
    rusage->ru_nswap = MIN( lsfRusage->ru_nswap, LONG_MAX );
    rusage->ru_inblock = MIN( lsfRusage->ru_inblock, LONG_MAX );
    rusage->ru_oublock = MIN( lsfRusage->ru_oublock, LONG_MAX );

    rusage->ru_msgsnd = MIN( lsfRusage->ru_msgsnd, LONG_MAX );
    rusage->ru_msgrcv = MIN( lsfRusage->ru_msgrcv, LONG_MAX );
    rusage->ru_nsignals = MIN( lsfRusage->ru_nsignals, LONG_MAX );
    rusage->ru_nvcsw = MIN( lsfRusage->ru_nvcsw, LONG_MAX );
    rusage->ru_nivcsw = MIN( lsfRusage->ru_nivcsw, LONG_MAX );

}

int
lsfRu2Str(FILE *log_fp, struct lsfRusage *lsfRu)
{
    return (fprintf(log_fp, " %1.6f %1.6f %1.0f %1.0f %1.0f %1.0f \
%1.0f %1.0f %1.0f %1.0f %1.0f %1.0f %1.0f %1.0f %1.0f %1.0f \
%1.0f %1.0f %1.0f", lsfRu->ru_utime, lsfRu->ru_stime, lsfRu->ru_maxrss,
        lsfRu->ru_ixrss, lsfRu->ru_ismrss, lsfRu->ru_idrss, lsfRu->ru_isrss,
        lsfRu->ru_minflt, lsfRu->ru_majflt, lsfRu->ru_nswap, lsfRu->ru_inblock,
        lsfRu->ru_oublock, lsfRu->ru_ioch, lsfRu->ru_msgsnd, lsfRu->ru_msgrcv,
        lsfRu->ru_nsignals, lsfRu->ru_nvcsw, lsfRu->ru_nivcsw,
        lsfRu->ru_exutime));

}

int
str2lsfRu(char *line, struct lsfRusage *lsfRu, int *ccount)
{
    int cc;

    cc = sscanf(line, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf \
%lf %lf %lf%n", &(lsfRu->ru_utime), &(lsfRu->ru_stime), &(lsfRu->ru_maxrss),
        &(lsfRu->ru_ixrss), &(lsfRu->ru_ismrss), &(lsfRu->ru_idrss),
        &(lsfRu->ru_isrss), &(lsfRu->ru_minflt), &(lsfRu->ru_majflt),
        &(lsfRu->ru_nswap), &(lsfRu->ru_inblock), &(lsfRu->ru_oublock),
        &(lsfRu->ru_ioch), &(lsfRu->ru_msgsnd), &(lsfRu->ru_msgrcv),
        &(lsfRu->ru_nsignals), &(lsfRu->ru_nvcsw), &(lsfRu->ru_nivcsw),
        &(lsfRu->ru_exutime), ccount);
    return cc;
}

void
lsfRusageAdd_ (struct lsfRusage *lsfRusage1, struct lsfRusage *lsfRusage2)
{
    LSFRU_FIELD_ADD(lsfRusage1->ru_utime, lsfRusage2->ru_utime);
    LSFRU_FIELD_ADD(lsfRusage1->ru_stime, lsfRusage2->ru_stime);
    LSFRU_FIELD_ADD(lsfRusage1->ru_maxrss,lsfRusage2->ru_maxrss);
    LSFRU_FIELD_ADD(lsfRusage1->ru_ixrss, lsfRusage2->ru_ixrss);
    LSFRU_FIELD_ADD(lsfRusage1->ru_ismrss, lsfRusage2->ru_ismrss);
    LSFRU_FIELD_ADD(lsfRusage1->ru_idrss, lsfRusage2->ru_idrss);
    LSFRU_FIELD_ADD(lsfRusage1->ru_isrss, lsfRusage2->ru_isrss);
    LSFRU_FIELD_ADD(lsfRusage1->ru_minflt, lsfRusage2->ru_minflt);
    LSFRU_FIELD_ADD(lsfRusage1->ru_majflt, lsfRusage2->ru_majflt);
    LSFRU_FIELD_ADD(lsfRusage1->ru_nswap, lsfRusage2->ru_nswap);
    LSFRU_FIELD_ADD(lsfRusage1->ru_inblock, lsfRusage2->ru_inblock);
    LSFRU_FIELD_ADD(lsfRusage1->ru_oublock, lsfRusage2->ru_oublock);
    LSFRU_FIELD_ADD(lsfRusage1->ru_ioch, lsfRusage2->ru_ioch);
    LSFRU_FIELD_ADD(lsfRusage1->ru_msgsnd, lsfRusage2->ru_msgsnd);
    LSFRU_FIELD_ADD(lsfRusage1->ru_msgrcv, lsfRusage2->ru_msgrcv);
    LSFRU_FIELD_ADD(lsfRusage1->ru_nsignals, lsfRusage2->ru_nsignals);
    LSFRU_FIELD_ADD(lsfRusage1->ru_nvcsw, lsfRusage2->ru_nvcsw);
    LSFRU_FIELD_ADD(lsfRusage1->ru_nivcsw, lsfRusage2->ru_nivcsw);
    LSFRU_FIELD_ADD(lsfRusage1->ru_exutime, lsfRusage2->ru_exutime);

}

void
cleanLsfRusage (struct lsfRusage *lsfRusage)
{
    lsfRusage->ru_utime = -1.0;
    lsfRusage->ru_stime = -1.0;
    lsfRusage->ru_maxrss = -1.0;
    lsfRusage->ru_ixrss = -1.0;
    lsfRusage->ru_ismrss = -1.0;
    lsfRusage->ru_idrss = -1.0;
    lsfRusage->ru_isrss = -1.0;
    lsfRusage->ru_minflt = -1.0;
    lsfRusage->ru_majflt = -1.0;
    lsfRusage->ru_nswap = -1.0;
    lsfRusage->ru_inblock = -1.0;
    lsfRusage->ru_oublock = -1.0;
    lsfRusage->ru_ioch = -1.0;
    lsfRusage->ru_msgsnd = -1.0;
    lsfRusage->ru_msgrcv = -1.0;
    lsfRusage->ru_nsignals = -1.0;
    lsfRusage->ru_nvcsw = -1.0;
    lsfRusage->ru_nivcsw = -1.0;
    lsfRusage->ru_exutime = -1.0;

}

void
cleanRusage (struct rusage *rusage)
{
    rusage->ru_utime.tv_sec = -1;
    rusage->ru_utime.tv_usec = -1;

    rusage->ru_stime.tv_sec = -1;
    rusage->ru_stime.tv_usec = -1;

    rusage->ru_maxrss = -1;
    rusage->ru_ixrss = -1;

    rusage->ru_isrss = -1;
    rusage->ru_minflt = -1;
    rusage->ru_majflt = -1;
    rusage->ru_nswap = -1;
    rusage->ru_inblock = -1;
    rusage->ru_oublock = -1;

    rusage->ru_idrss = -1;
    rusage->ru_msgsnd = -1;
    rusage->ru_msgrcv = -1;
    rusage->ru_nsignals = -1;
    rusage->ru_nvcsw = -1;
    rusage->ru_nivcsw = -1;
}
