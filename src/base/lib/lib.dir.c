/* $Id: lib.dir.c,v 1.5 2007/08/15 22:18:50 tmizan Exp $
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

/* Bug. This code is all mesozoic legacy. Use system calls like chdir()
 * stat(), fopen() etc.
 */
#include "lsf/lib/lib.h"

extern struct config_param genParams[];
static struct hTab hashTab;

static int putin_(int, char *, int, char *, int, char *);
static int getMap_(void);
static int tryPwd(char *path, char *pwdpath);
static int netHostChdir(char *, struct hostent *);
static char *mountNet_(struct hostent *);
static char *usePath(char *);

char chosenPath[MAXPATHLEN];

static char *usePath(char *path)
{
    strcpy(chosenPath, path);
    return chosenPath;
}

int mychdir_(char *path, struct hostent *hp)
{
    char *goodpath = path;
    char *sp = NULL;
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;
    static char first = true;
    char filename[MAXPATHLEN];

    if (path == NULL || strlen(path) == 0 || path[0] != '/' || AM_NEVER)
        return (chdir(usePath(path)));

    if (hp != NULL)
        if (netHostChdir(path, hp) == 0)
            return 0;

    if (strstr(path, "/tmp_mnt") == path) {
        sp = path + strlen("/tmp_mnt");
        if (chdir(usePath(sp)) == 0)
            return 0;
    } else {
        if (chdir(usePath(path)) == 0)
            return 0;
    }

    if (errno != ENOENT && errno != ENOTDIR)
        return -1;

    if (getcwd(filename, sizeof(filename)) == NULL)
        goto try;

    sp = getenv("HOME");
    if (sp != NULL)
        chdir(sp);

    chdir(filename);

    try:
        if (path[0] != '/')
            return -1;

    if ((goodpath = strstr(path, "/exp/lsf")) != NULL) {
        if (chdir(usePath(goodpath)) == 0)
            return 0;
    }

    if (strstr(path, "/tmp_mnt") == path) {
        goodpath = path + strlen("/tmp_mnt");
    } else {
        if (chdir(usePath(path)) == 0)
            return 0;
        sp = getenv("PWD");
        if (tryPwd(path, sp) == 0)
            return 0;
    }

    if (goodpath == NULL)
        goodpath = strchr(path + 1, '/');
    else
        goodpath = strchr(goodpath + 1, '/');
    if (goodpath != NULL) {
        if (chdir(usePath(goodpath)) == 0)
            return 0;
    } else {
        return -1;
    }

    if (first) {
        first = false;
        if (getMap_() != 0)
            return -1;
    }

    hashEntryPtr = h_firstEnt_(&hashTab, &hashSearchPtr);
    if (hashEntryPtr == NULL) {
        errno = ENOENT;
        return -1;
    }

    while (hashEntryPtr != NULL) {
        sprintf(filename, "%s%s", hashEntryPtr->keyname, goodpath);
        if (chdir(usePath(filename)) == 0)
            return 0;
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
    }

    goodpath = strchr(goodpath + 1, '/');
    if (goodpath == NULL) {
        return -1;
    }

    hashEntryPtr = h_firstEnt_(&hashTab, &hashSearchPtr);
    while (hashEntryPtr != NULL) {
        sprintf(filename, "%s%s", hashEntryPtr->keyname, goodpath);
        if (chdir(usePath(filename)) == 0)
            return 0;
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
    }

    if (chdir(usePath(goodpath)) == 0)
        return 0;

    if (strstr(path, "/tmp_mnt") != path)
        return -1;

    goodpath = path + strlen("/tmp_mnt");
    if (*goodpath == '\0')
        return -1;

    strcpy(filename, goodpath);

    sp = strchr(filename + 1, '/');
    if (sp == NULL)
        return -1;

    goodpath = strchr(sp + 1, '/');
    if (goodpath == NULL)
        return -1;

    if ((sp = strchr(goodpath + 1, '/')) == NULL)
        return -1;

    *goodpath = '\0';
    strcat(filename, sp);

    if (chdir(usePath(filename)) == 0)
        return 0;

    if ((sp = strchr(goodpath + 1, '/')) == NULL)
        return -1;

    *goodpath = '\0';
    strcat(filename, sp);

    if (chdir(usePath(filename)) == 0)
        return 0;

    if (chdir(usePath(path)) == 0)
        return 0;

    return -1;
}

static int tryPwd(char *path, char *pwdpath)
{
    char *PA, *PAPB, *pa, *pb, *pc, *sp1;
    char filename[MAXFILENAMELEN];

    if (pwdpath == NULL)
        return -1;

    if (strcmp(pwdpath, "/") == 0)
        return -1;

    strcpy(filename, pwdpath);
    sp1 = strchr(filename + 1, '/');
    if (sp1 != NULL)
        *sp1 = '\0';
    PA = putstr_(filename);
    strcpy(filename, pwdpath);
    if (sp1 != NULL) {
        sp1 = strchr(sp1 + 1, '/');
        if (sp1 != NULL)
            *sp1 = '\0';
    }
    PAPB = putstr_(filename);

    pa = path;
    pb = strchr(path + 1, '/');
    if (pb == NULL)
        pb = pa;
    pc = strchr(pb + 1, '/');
    if (pc == NULL)
        pc = pb;

    strcpy(filename, PA);
    strcat(filename, pa);
    if (chdir(usePath(filename)) == 0) {
        free(PA);
        free(PAPB);
        return 0;
    }

    strcpy(filename, PA);
    strcat(filename, pb);
    if (chdir(usePath(filename)) == 0) {
        free(PA);
        free(PAPB);
        return 0;
    }

    strcpy(filename, PAPB);
    strcat(filename, pc);
    if (chdir(usePath(filename)) == 0) {
        free(PA);
        free(PAPB);
        return 0;
    }

    strcpy(filename, PAPB);
    strcat(filename, pb);
    if (chdir(usePath(filename)) == 0) {
        free(PA);
        free(PAPB);
        return 0;
    }

    free(PA);
    free(PAPB);
    return -1;
}

static int getMap_(void)
{
    char *domain;
    struct ypall_callback incallback;
    int i;

    h_initTab_(&hashTab, 64);
    incallback.foreach = putin_;
    if ((i = yp_get_default_domain(&domain)) != 0)
        return i;
    return (yp_all(domain, "auto.master", &incallback));
}

static int putin_(int status, char *inkey, int inkeylen, char *inval,
                  int invallen, char *indata)
{
    if (ypprot_err(status) != 0)
        return true;

    inkey[inkeylen] = '\0';
    if (strcmp(inkey, "/-") == 0)
        return false;

    (void) h_addEnt_(&hashTab, inkey, 0);

    return false;
}

static int netHostChdir(char *path, struct hostent *hp)
{
    char filename[MAXFILENAMELEN];
    char *mp;

    if (AM_LAST || AM_NEVER) {
        if (chdir(usePath(path)) == 0)
            return 0;
    }

    if (strstr(path, "/net/") == path)
        return -1;

    if (strstr(path, "/tmp_mnt/") == path)
        return -1;

    if (hp == NULL)
        return -1;

    if ((mp = mountNet_(hp)) == NULL)
        return -1;
    sprintf(filename, "%s%s", mp, path);
    return (chdir(usePath(filename)));
}

static char *mountNet_(struct hostent *hp)
{
    char hostnamebuf[MAXHOSTNAMELEN];
    static char filename[MAXFILENAMELEN];
    char *sp;
    char cwd[MAXPATHLEN];

    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return NULL;

    strcpy(hostnamebuf, hp->h_name);
    sp = hostnamebuf;
    if ((sp = strchr(sp, '.')) != NULL) {
        *sp = '\0';
        sprintf(filename, "/net/%s", hostnamebuf);
        if (chdir(filename) == 0) {
            chdir(cwd);
            return filename;
        }
        *sp = '.';
    }

    sprintf(filename, "/net/%s", hostnamebuf);
    if (chdir(filename) == 0) {
        chdir(cwd);
        return filename;
    }

    return NULL;
}

int myopen_(char *filename, int flags, int mode, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (open(usePath(filename), flags, mode));

    if (AM_LAST)
        if ((i = open(usePath(filename), flags, mode)) != -1)
            return i;

    if (strstr(filename, "/net/") == filename)
        return (open(usePath(filename), flags, mode));

    if (strstr(filename, "/tmp_mnt/") == filename)
        return (open(usePath(filename), flags, mode));

    if ((mp = mountNet_(hp)) == NULL)
        return (open(usePath(filename), flags, mode));

    sprintf(fnamebuf, "%s%s", mp, filename);
    i = open(usePath(fnamebuf), flags, mode);
    if (i >= 0)
        return i;

    return (open(usePath(filename), flags, mode));
}

FILE *myfopen_(char *filename, char *type, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    FILE *fp;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (fopen(usePath(filename), type));

    if (AM_LAST)
        if ((fp = fopen(usePath(filename), type)) != NULL)
            return fp;

    if (strstr(filename, "/net/") == filename)
        return (fopen(usePath(filename), type));

    if (strstr(filename, "/tmp_mnt/") == filename)
        return (fopen(usePath(filename), type));

    if ((mp = mountNet_(hp)) == NULL)
        return (fopen(usePath(filename), type));

    sprintf(fnamebuf, "%s%s", mp, filename);
    fp = fopen(usePath(fnamebuf), type);
    if (fp != NULL)
        return fp;

    return (fopen(usePath(filename), type));
}

int mystat_(char *filename, struct stat *sbuf, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (stat(usePath(filename), sbuf));

    if (AM_LAST)
        if ((i = stat(usePath(filename), sbuf)) != -1)
            return i;

    if (strstr(filename, "/net/") == filename)
        return (stat(usePath(filename), sbuf));

    if (strstr(filename, "/tmp_mnt/") == filename)
        return (stat(usePath(filename), sbuf));

    if ((mp = mountNet_(hp)) == NULL)
        return (stat(usePath(filename), sbuf));

    sprintf(fnamebuf, "%s%s", mp, filename);
    i = stat(usePath(fnamebuf), sbuf);
    if (i >= 0)
        return i;

    return (stat(usePath(filename), sbuf));
}

int mychmod_(char *filename, mode_t mode, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (chmod(usePath(filename), mode));

    if (AM_LAST)
        if ((i = chmod(usePath(filename), mode)) != -1)
            return i;

    if (strstr(filename, "/net/") == filename)
        return (chmod(usePath(filename), mode));

    if (strstr(filename, "/tmp_mnt/") == filename)
        return (chmod(usePath(filename), mode));

    if ((mp = mountNet_(hp)) == NULL)
        return (chmod(usePath(filename), mode));

    sprintf(fnamebuf, "%s%s", mp, filename);
    i = chmod(usePath(fnamebuf), mode);
    if (i >= 0)
        return i;

    return (chmod(usePath(filename), mode));
}

int myunlink_(char *filename, struct hostent *hp, int doMount)
{
    char fnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (unlink(usePath(filename)));

    if (AM_LAST)
        if ((i = unlink(usePath(filename))) != -1)
            return 1;

    if (doMount) {
        if (strstr(filename, "/net/") == filename)
            return (unlink(usePath(filename)));

        if (strstr(filename, "/tmp_mnt/") == filename)
            return (unlink(usePath(filename)));

        if ((mp = mountNet_(hp)) == NULL) {
            return 1;
        }

        sprintf(fnamebuf, "%s%s", mp, filename);
        i = unlink(usePath(fnamebuf));
        if (i >= 0)
            return i;
    }
    return (unlink(usePath(filename)));
}

int mymkdir_(char *filename, mode_t mode, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || filename[0] != '/' || AM_NEVER)
        return (mkdir(usePath(filename), mode));

    if (AM_LAST)
        if ((i = mkdir(usePath(filename), mode)) != -1)
            return i;

    if (strstr(filename, "/net/") == filename)
        return (mkdir(usePath(filename), mode));

    if (strstr(filename, "/tmp_mnt/") == filename)
        return (mkdir(usePath(filename), mode));

    if ((mp = mountNet_(hp)) == NULL)
        return (mkdir(usePath(filename), mode));

    sprintf(fnamebuf, "%s%s", mp, filename);
    i = mkdir(usePath(fnamebuf), mode);
    if (i >= 0)
        return i;

    return (mkdir(usePath(filename), mode));
}

int myrename_(char *from, char *to, struct hostent *hp)
{
    char fnamebuf[MAXFILENAMELEN];
    char tnamebuf[MAXFILENAMELEN];
    int i;
    char *mp;

    if (!hp || AM_NEVER)
        return (rename(from, to));

    if (AM_LAST)
        if ((i = rename(from, to)) != -1)
            return i;

    if ((strstr(from, "/net/") == from) && (strstr(to, "/net") == to))
        return (rename(from, to));

    if ((strstr(from, "/tmp_mnt/") == from) && (strstr(to, "/tmp_mnt/") == to))
        return (rename(from, to));

    if ((mp = mountNet_(hp)) == NULL)
        return (rename(from, to));

    if (from[0] == '/')
        sprintf(fnamebuf, "%s%s", mp, from);
    else
        strcpy(fnamebuf, from);

    if (to[0] == '/')
        sprintf(tnamebuf, "%s%s", mp, to);
    else
        strcpy(tnamebuf, to);

    i = rename(fnamebuf, tnamebuf);
    if (i >= 0)
        return i;

    return (rename(from, to));
}
