/* $Id: resourcecmd.c,v 1.4 2007/08/15 22:18:49 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
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

#include "lsf/intlib/libllcore.h"
#include "lsf/lib/lproto.h"

static void prtOneInstance (char *, struct lsSharedResourceInstance *);
static int makeShare(char *, char ***, char ***, char ***,
                     int (*)(struct resItem *));
static int isStaticSharedResource(struct resItem *);
static int isDynamicSharedResource(struct resItem *);
static void prtTableHeader();

int
getResourceNames (int argc, char **argv, int optind, char **resourceNames)
{

    int numNames = 0, i;

    if (argc >= optind+1) {
        for (numNames = 0; argc > optind; optind++) {
            for (i = 0; i < numNames; i++) {
                if (strcmp (resourceNames[i], argv[optind]) == 0)
                    break;
            }

            if (numNames == 0) {
                resourceNames[numNames++] =  argv[optind];
            } else if (i == numNames) {
                resourceNames[numNames] = argv[optind];
                numNames ++;
            }
        }
    }
    return numNames;
}

void
displayShareResource(int argc, char **argv, int index, int flag, int extflag)
{
    int i, j, k, numRes = 0;
    struct lsSharedResourceInfo  *lsResourceInfo;
    struct lsInfo *lsInfo;
    char **resources = NULL, **resourceNames = NULL;
    int firstFlag = 1;

    if ((lsInfo = ls_info()) == NULL) {
        ls_perror("lsinfo");
        exit(-10);
    }

    if (argc > index) {
        if ((resourceNames =
             (char **) malloc ((argc - index) * sizeof (char *))) == NULL) {
            lserrno = LSE_MALLOC;
            ls_perror("lshosts");
            exit (-1);
        }
        numRes = getResourceNames (argc, argv, index, resourceNames);
    }

    if (numRes > 0)
        resources = resourceNames;

    TIMEIT(0, (lsResourceInfo = ls_sharedresourceinfo (resources, &numRes, NULL, 0)), "ls_sharedresourceinfo");

    if (lsResourceInfo == NULL) {
        ls_perror("ls_sharedresourceinfo");
        exit(-1);
    }

    for (k = 0; k < numRes; k++) {

        for (i = 0; i < lsResourceInfo[k].nInstances; i++)  {

            for (j = 0; j < lsInfo->nRes; j++) {

                if (!extflag)
                    if (lsInfo->resTable[j].flags & RESF_EXTERNAL)
                        continue;

                if (strcmp(lsInfo->resTable[j].name,
                           lsResourceInfo[k].resourceName) == 0) {
                    if (flag == true) {
                        if (!(lsInfo->resTable[j].flags & RESF_DYNAMIC)) {

                            if (firstFlag){
                                firstFlag = 0;
                                prtTableHeader();
                            }
                            prtOneInstance(lsResourceInfo[k].resourceName,
                                           &(lsResourceInfo[k].instances[i]));
                        }
                    } else  {
                        if (lsInfo->resTable[j].flags & RESF_DYNAMIC) {

                            if (firstFlag) {
                                firstFlag = 0;
                                prtTableHeader();
                            }
                            prtOneInstance(lsResourceInfo[k].resourceName,
                                           &(lsResourceInfo[k].instances[i]));
                        }
                    }
                }
            }
        }
    }
    if (firstFlag){
        if (flag)
            printf("No static shared resources defined \n");
        else
            printf("No dynamic shared resources defined \n");
    }
    FREEUP(resourceNames);
}

static void
prtTableHeader()
{
    char *res, *val, *loc;

    res = putstr_("RESOURCE");
    val = putstr_("VALUE");

    loc = putstr_("LOCATION");

    if (res == NULL || val == NULL || loc == NULL) {
        printf(I18N_FUNC_FAIL_M, "prtTableHeader", "putstr_");
        return;
    } else
        printf("%-25s%20s%15s\n", res, val, loc);
    return;
}

static void
prtOneInstance (char *name, struct lsSharedResourceInstance *instance)
{
    int i, currentPos, len;
    char space52[] = "                                                    ";
    char fmt[10];

    if ((len = strlen(instance->value)) <= 20)  {
        currentPos = 45 + 7;
        printf ("%-25s%20s       ", name, instance->value);
    } else {
        currentPos = 25 + len + 2;
        printf ("%-25s%20s  ", name, instance->value);
    }

    for (i = 0; i < instance->nHosts; i++) {

        len = strlen(instance->hostList[i]);
        currentPos = currentPos + len + 1;
        sprintf (fmt, "%s%d%s ", "%", len, "s");
        if (currentPos > 80) {
            printf ("\n%s", space52);
            currentPos = 52 + len + 1;
        }
        printf (fmt, instance->hostList[i]);
    }
    printf("\n");
}

int
makeShareField(char *hostname, int flag,
               char ***nameTable, char ***valueTable, char ***formatTable)
{
    if (flag == true)
        return makeShare(hostname, nameTable, valueTable, formatTable,
                         &isStaticSharedResource);

    return makeShare(hostname, nameTable, valueTable, formatTable,
                     &isDynamicSharedResource);
}

static int
makeShare(char *hostname, char ***nameTable, char ***valueTable,
          char ***formatTable, int (*resourceSelect)(struct resItem *))
{
    static int first = true;
    static struct lsSharedResourceInfo *resourceInfo;
    static struct lsInfo *lsInfo;
    static char **namTable;
    static char **valTable;
    static char **fmtTable;
    static int numRes, nRes;
    int k, i, j;
    char *hPtr;
    int ii, numHosts, found;

    if (first == true) {
        if ((lsInfo = ls_info()) == NULL) {
            return -1;
        }

        resourceInfo = ls_sharedresourceinfo(NULL,
                                             &numRes,
                                             NULL,
                                             0);

        if (resourceInfo == NULL) {
            return -1;
        }
        if ((namTable = calloc(numRes, sizeof(char *))) == NULL){
            lserrno = LSE_MALLOC;
            return -1;
        }
        if ((valTable = calloc(numRes, sizeof(char *))) == NULL){
            lserrno = LSE_MALLOC;
            return -1;
        }
        if ((fmtTable = calloc(numRes, sizeof(char *))) == NULL){
            lserrno = LSE_MALLOC;
            return -1;
        }
        first = false;
    } else {

        for (i = 0; i < nRes; i++) {
            FREEUP(fmtTable[i]);
        }
    }

    nRes = 0;
    for (k = 0; k < numRes; k++) {
        found = false;
        for (j = 0; j < lsInfo->nRes; j++) {
            if (strcmp(lsInfo->resTable[j].name,
                       resourceInfo[k].resourceName) == 0) {
                if (resourceSelect(&lsInfo->resTable[j])) {
                    found = true;
                    break;
                }
                break;
            }
        }
        if (!found) {
            continue;
        }
        namTable[nRes] = resourceInfo[k].resourceName;
        found = false;
        for (i = 0; i < resourceInfo[k].nInstances; i++) {
            numHosts =  resourceInfo[k].instances[i].nHosts;
            for (ii = 0; ii < numHosts; ii++) {
                hPtr  = resourceInfo[k].instances[i].hostList[ii];
                if (strcmp(hPtr, hostname) == 0) {
                    valTable[nRes] = resourceInfo[k].instances[i].value;
                    found = true;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
        if (!found) {
            valTable[nRes] = "-";
        }
        nRes++;
    }
    if (nRes) {
        j = 0;
        for (i = 0; i < nRes; i++) {
            char fmt[LL_BUFSIZ_32];
            int nameLen;

            nameLen = strlen(namTable[i]);
            sprintf(fmt, "%s%d.%d%s", "%", nameLen+2, nameLen+1, "s");
            fmtTable[j++] = putstr_(fmt);
        }
    }
    *nameTable = namTable;
    *valueTable = valTable;
    *formatTable = fmtTable;
    return nRes;
}

static int
isStaticSharedResource(struct resItem *resEnt)
{
    return ((!(resEnt->flags & RESF_DYNAMIC)) &&
            ((resEnt->valueType & LS_STRING) ||
             (resEnt->valueType & LS_NUMERIC)));
}

static int
isDynamicSharedResource(struct resItem *resEnt)
{
    return (resEnt->flags & RESF_DYNAMIC);
}
