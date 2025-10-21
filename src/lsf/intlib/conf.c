/* $Id: conf.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
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
#include "lsf/intlib/common.h"
#include "lsf/lib/lproto.h"

/* Bug. This is also garbage legacy. Probably originally the configuration was
 * read using this file and then garbage lsb.conf.c came along leaving out this
 * one. We should restore the original behavior and just use this file.
 * Does not make sense that intlib includes lib.
 */

char *
getNextValue(char **line)
{
    return (getNextValueQ_(line, '(', ')'));
}

int
keyMatch (struct keymap *keyList, char *line, int exact)
{
    int pos = 0;
    int i;
    char *sp = line;
    char *word;
    int found;

    i = 0;
    while (keyList[i].key != NULL) {
        keyList[i].position = -1;
        i++;
    }

    while ((word = getNextWord_(&sp)) != NULL) {
        i = 0;
        found = false;
        while (keyList[i].key != NULL) {
            if (strcasecmp(word, keyList[i].key) == 0) {
                if (keyList[i].position != -1)
                    return false;
                found = true;
                keyList[i].position = pos;
                break;
            }
            i++;
        }
        if (! found)
            return false;

        pos++;
    }

    if (! exact)
        return true;

    i = 0;
    while (keyList[i].key != NULL) {
        if (keyList[i].position == -1)
            return false;
        i++;
    }

    return true;
}

int
isSectionEnd(char *linep, char *lsfile, int *LineNum, char *sectionName)
{
    char *word;

    word = getNextWord_(&linep);
    if (strcasecmp(word, "end") != 0)
        return false;

    word = getNextWord_(&linep);
    if (! word ) {
        ls_syslog(LOG_ERR,
                  "%s %d: section %s ended without section name, ignored",
                  lsfile, *LineNum, sectionName);
        return true;
    }

    if (strcasecmp (word, sectionName) != 0)
        ls_syslog(LOG_ERR,
                  "%s%d: section %s ended with wrong section name %s, ignored",
                  lsfile, *LineNum, sectionName, word);

    return true;

}

char *
getBeginLine(FILE *fp, int *LineNum)
{
    char *sp;
    char *wp;

    for (;;) {
        sp = getNextLineC_(fp, LineNum, true);
        if (! sp)
            return NULL;

        wp = getNextWord_(&sp);
        if (wp && (strcasecmp(wp, "begin") == 0))
            return sp;
    }

}

int
readHvalues(struct keymap *keyList, char *linep, FILE *fp, char *lsfile,
            int *LineNum, int exact, char *section)
{
    char *key;
    char *value;
    char *sp, *sp1;
    char error = false;
    static char fname[] = "readHvalues";
    int i=0;

    sp = linep;
    key = getNextWord_(&linep);
    if ((sp1 = strchr(key, '=')) != NULL)
        *sp1 = '\0';

    value = strchr(sp, '=');
    if (!value) {
        ls_syslog(LOG_ERR,
                  "%s: %s %d: missing '=' after keyword %s, section %s ignored",
                  fname, lsfile, *LineNum, key, section);
        doSkipSection(fp, LineNum, lsfile, section);
        return -1;
    }
    value++;
    while (*value == ' ')
        value++;

    if (value[0] == '\0') {
        ls_syslog(LOG_ERR,
                  "%s: %s %d: nul value after keyword %s, section %s ignored",
                  fname, lsfile, *LineNum, key, section);
        return -1;
    }

    if (value[0] == '(') {
        value++;
        if ((sp1 = strrchr(value, ')')) != NULL)
            *sp1 = '\0';
    }
    if (putValue(keyList, key, value) < 0) {
        ls_syslog(LOG_ERR,
                  "%s: %s %d: bad keyword %s in section %s, ignoring the section",
                  fname,  lsfile, *LineNum, key, section);
        doSkipSection(fp, LineNum, lsfile, section);
        return -1;
    }

    if ((linep = getNextLineC_(fp, LineNum, true)) != NULL) {
        if (isSectionEnd(linep, lsfile, LineNum, section)) {
            if (! exact)
                return 0;

            i = 0;
            while (keyList[i].key != NULL) {
                if (keyList[i].val == NULL) {
                    ls_syslog(LOG_ERR,
                              "%s: %s %d: required keyword %s is missing in section %s, ignoring the section", fname, lsfile, *LineNum, keyList[i].key, section);
                    error = true;
                }
                i++;
            }
            if (error) {
                i = 0;
                while (keyList[i].key != NULL) {
                    if (keyList[i].val != NULL)
                        free(keyList[i].val);
                    i++;
                }
                return -1;
            }
            return 0;
        }

        return readHvalues(keyList, linep, fp, lsfile, LineNum, exact, section);
    }

    ls_syslog(LOG_ERR,
              "%s: %s %d: Premature EOF in section %s",
              fname, lsfile, *LineNum, section);
    return -1;
}

int
putValue(struct keymap *keyList, char *key, char *value)
{
    int i;

    i=0;
    while (keyList[i].key != NULL) {
        if (strcasecmp(keyList[i].key, key) == 0) {
            if (keyList[i].val != NULL)
                free (keyList[i].val);
            if (strcmp (value, "-") == 0)
                keyList[i].val = putstr_("");
            else
                keyList[i].val = putstr_(value);
            return 0;
        }
        i++;
    }

    return -1;
}

void
doSkipSection(FILE *fp, int *LineNum, char *lsfile, char *sectionName)
{
    char *word;
    char *cp;

    while ((cp = getNextLineC_(fp, LineNum, true)) != NULL) {
        word = getNextWord_(&cp);
        if (strcasecmp(word, "end") == 0) {
            word = getNextWord_(&cp);
            if (! word) {
                ls_syslog(LOG_ERR,
                          "%s%d: Section ended without section name, ignored",
                          lsfile, *LineNum);
            } else {
                if (strcasecmp(word, sectionName) != 0)
                    ls_syslog(LOG_ERR,
                              "%s(%d: Section %s ended with wrong section name %s, ignored", lsfile, *LineNum, sectionName, word);
            }
            return;
        }
    }

    ls_syslog(LOG_ERR, "%s: %s%d: premature EOF in section",  __func__,
              lsfile, *LineNum);

}

int
mapValues(struct keymap *keyList, char *line)
{
    int pos = 0;
    char *value;
    int i = 0;
    int found;
    int numv = 0;

    while (keyList[i].key != NULL) {
        keyList[i].val = NULL;
        if (keyList[i].position != -1)
            numv++;
        i++;
    }

    while ((value = getNextValue(&line)) != NULL) {
        i=0;
        found = false;
        while (keyList[i].key != NULL) {
            if (keyList[i].position != pos) {
                i++;
                continue;
            }
            if (strcmp (value, "-") == 0)
                keyList[i].val = putstr_("");
            else
                keyList[i].val = putstr_(value);
            found = true;
            break;
        }
        if (! found)
            goto fail;
        pos++;
    }

    if (pos != numv)
        goto fail;

    return 0;

fail:
    i=0;
    while (keyList[i].key != NULL)  {
        if (keyList[i].val != NULL) {
            free(keyList[i].val);
            keyList[i].val = NULL;
        }

        i++;
    }
    return -1;

}

int
putInLists (char *word, struct admins *admins, int *numAds, char *forWhat)
{
    struct passwd *pw;
    static char fname[] = "putInLists";
    char **tempNames;
    int i, *tempIds, *tempGids;

    if (!(pw = getpwnam(word))) {
        ls_syslog(LOG_ERR, "%s: <%s> is not a valid user name; ignored",
                  __func__, word);
        return 0;
    }
    if (isInlist (admins->adminNames, pw->pw_name, admins->nAdmins)) {
        ls_syslog(LOG_WARNING,
                  "%s: Duplicate user name <%s> %s; ignored",
                  fname, word, forWhat);
        return 0;
    }
    admins->adminIds[admins->nAdmins] = pw->pw_uid;
    admins->adminGIds[admins->nAdmins] = pw->pw_gid;
    admins->adminNames[admins->nAdmins] = putstr_(pw->pw_name);
    admins->nAdmins += 1;

    if (logclass & LC_TRACE)
        ls_syslog(LOG_DEBUG, "putInLists: uid %d gid %d name <%s>",
                  pw->pw_uid, pw->pw_gid, pw->pw_name);

    if (admins->nAdmins >= *numAds) {
        *numAds = *numAds * 2;
        tempIds = (int *) realloc(admins->adminIds, *numAds * sizeof (int));
        tempGids = (int *) realloc(admins->adminGIds, *numAds * sizeof (int));
        tempNames = (char **) realloc(admins->adminNames, *numAds * sizeof (char *));
        if (tempIds == NULL || tempGids == NULL || tempNames == NULL) {

            ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M,  fname, "realloc");
            FREEUP (tempIds);
            FREEUP (tempGids);
            FREEUP (tempNames);

            FREEUP (admins->adminIds);
            FREEUP (admins->adminGIds);
            for (i = 0; i < admins->nAdmins; i++)
                FREEUP (admins->adminNames[i]);
            FREEUP (admins->adminNames);
            admins->nAdmins = 0;
            lserrno = LSE_MALLOC;
            return -1;
        } else {
            admins->adminIds = tempIds;
            admins->adminGIds = tempGids;
            admins->adminNames = tempNames;
        }
    }
    return 0;
}

int
isInlist (char **adminNames, char *userName, int actAds)
{
    int i;

    if (actAds == 0)
        return false;
    for (i = 0; i < actAds; i++) {
        if (strcmp (adminNames[i], userName) == 0)
            return true;
    }
    return false;

}
