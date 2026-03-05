/* $Id: misc.c,v 1.8 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/lib/ll.sys.h"
#include "lsf/lib/lproto.h"
#include "lsf/lib/lib.h"

void putMaskLevel(int level, char **para)
{
    level = level + LOG_DEBUG;

    if ((level >= LOG_DEBUG) && (level <= LOG_DEBUG3)) {
        FREEUP(*para);

        switch (level) {
        case LOG_DEBUG:
            *para = putstr_("LOG_DEBUG");
            break;
        case LOG_DEBUG1:
            *para = putstr_("LOG_DEBUG1");
            break;
        case LOG_DEBUG2:
            *para = putstr_("LOG_DEBUG2");
            break;
        case LOG_DEBUG3:
            *para = putstr_("LOG_DEBUG3");
            break;
        }
    }
}

/* Bug. use strstr()
 */
int matchName(char *pattern, char *name)
{
    int i, ip;

    if (!pattern || !name)
        return false;

    ip = (int) strlen(pattern);
    for (i = 0; i < ip && pattern[i] != '['; i++) {
        if (pattern[i] == '*')
            return true;

        if (name[i] == '\0' || name[i] == '[' || pattern[i] != name[i])
            return false;
    };

    if (name[i] == '\0' || name[i] == '[')
        return true;

    return false;
}

char **parseCommandArgs(char *comm, char *args)
{
    int argmax = 10;
    int argc = 0;
    int quote = 0;
    char *i = args;
    char *j = args;
    char **argv = NULL;

    if ((argv = calloc(argmax, sizeof(char *))) == NULL)
        goto END;

    if (comm)
        argv[argc++] = comm;

    if (!args)
        goto END;

    while (*j && (*j == ' ' || *j == '\t'))
        ++j;
    if (!*j)
        goto END;
    *i = *j;

    quote = 0;
    argv[argc++] = i;
    do {
        switch (*j) {
        case ' ':
        case '\t':
            if (quote) {
                *i++ = *j++;
            } else {
                *i++ = *j++ = '\0';
                while (*j && (*j == ' ' || *j == '\t'))
                    ++j;

                if (argc == argmax - 1) {
                    argmax *= 2;
                    argv = (char **) realloc(argv, argmax * sizeof(char *));
                    if (!argv)
                        goto END;
                }

                *i = *j;
                argv[argc++] = i;
            }
            break;

        case '\'':
        case '"':
            if (quote) {
                if (quote == *j) {
                    quote = 0;
                    ++j;
                } else {
                    *i++ = *j++;
                }
            } else {
                quote = *j++;
            }
            break;

        case '\\':
            if (quote != '\'')
                ++j;
            *i++ = *j++;
            break;

        default:
            *i++ = *j++;
            break;
        }
    } while (*j);

END:
    if (argv)
        argv[argc] = NULL;
    return argv;
}

/* Bug. The problem is not NULL is that fp can point to
 * invalid memory if you are sloppy.
 */
int FCLOSEUP(FILE **fp)
{
    if (!fp) {
        lserrno = LSE_FILE_CLOSE;
        return -1;
    }

    fclose(*fp);
    *fp = NULL;

    return 0;
}

// Old stuff
void openChildLog(const char *defLogFileName, const char *confLogDir,
                  int use_stderr, char **confLogMaskPtr)
{
}

void displayEnhancementNames(void)
{
}
