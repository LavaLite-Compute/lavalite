/* $Id: lib.hosta.c,v 1.3 2007/08/15 22:18:50 tmizan Exp $
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

int
getAskedHosts_(char *optarg, char ***askedHosts, int *numAskedHosts,
               int *badIdx, int checkHost)
{
    int num = 64;
    int i;
    char *word;
    char **tmp;
    char *hname;
    const char *officialName;
    int foundBadHost = false;
    static char **hlist = NULL;
    static int nhlist = 0;
    char host[MAXHOSTNAMELEN];

    if (hlist) {
        for (i = 0; i < nhlist; i++)
            free(hlist[i]);
        free(hlist);
        hlist = NULL;
    }

    nhlist = 0;
    if ((hlist = calloc(num, sizeof (char *))) == NULL)  {
        lserrno = LSE_MALLOC;
        return -1;
    }

    *badIdx = 0;

    while((word = getNextWord_(&optarg)) != NULL) {

        strncpy(host, word, sizeof(host) - 1);
        if (ls_isclustername(host) <= 0) {
            if (checkHost == false) {
                hname = host;
            } else {
                if ((officialName = getHostOfficialByName_(host)) == NULL) {
                    if (!foundBadHost) {
                        foundBadHost = true;
                        *badIdx = nhlist;
                    }
                    hname = host;
                } else {
                    hname = (char*)officialName;
                }
            }
        } else
            hname = host;

        if ((hlist[nhlist] = putstr_(hname)) == NULL) {
            lserrno = LSE_MALLOC;
            goto Error;
        }

        nhlist++;
        if (nhlist == num) {
            if ((tmp = realloc(hlist, 2 * num * sizeof(char *)))
                == NULL) {
                lserrno = LSE_MALLOC;
                goto Error;
            }
            hlist = tmp;
            num = 2 * num;
        }
    }

    *numAskedHosts = nhlist;
    *askedHosts = hlist;

    if (foundBadHost) {
        lserrno = LSE_BAD_HOST;
        return -1;
    }

    return 0;

Error:
    for (i = 0; i < nhlist; i++)
        free(hlist[i]);
    free(hlist);
    hlist = NULL;
    nhlist = 0;
    return -1;
}
