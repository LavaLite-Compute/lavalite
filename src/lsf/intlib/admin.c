/* $Id: admin.c,v 1.6 2007/08/15 22:18:49 tmizan Exp $
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
#include "lsf/intlib/intlibout.h"

#define BADCH ":"
#define NL_SETN 22

extern int errLineNum_;
extern int optind;
extern char *optarg;

void
parseAndDo (char *cmdBuf, int (*func)() )
{
#define MAX_ARG 100

    extern int optind;
    int  argc, i ;
    char *argv[MAX_ARG];

    int see_string = 0;

    for (i = 0; i < MAX_ARG; i++) {

        while(isspace(*cmdBuf) && !see_string)
            cmdBuf++;

        if (*cmdBuf == '"')
        { cmdBuf++;
            see_string = 1;
        }

        if (*cmdBuf == '\0')
            break;

        argv[i] = cmdBuf;

        while(*cmdBuf !='\0' && !isspace(*cmdBuf) && *cmdBuf != '"')
        {
            cmdBuf++;
        }

        while ( see_string && *cmdBuf != '"')
        {
            cmdBuf++;
            if (*cmdBuf == '\0')
            {
                see_string = 0;
                ls_perror (_i18n_msg_get(ls_catd, NL_SETN, 100,
                            "Syntax Error of line parameter! \n"));  /* catgets 100 */
                exit(-1);
            }

        }
        if (see_string)
            see_string = 0;

        if (*cmdBuf != '\0') {
            *cmdBuf = '\0';
            cmdBuf++;
        }
    }
    if (i == 0)
        return;

    argv[i] = NULL;
    argc = i;
    optind = 1;

    (*func) (argc, argv);
}

int
adminCmdIndex(char *cmd, char *cmdList[])
{
    int i;
    static char quit[]="quit";

    if (strcmp("q", cmd)==0)
        cmd=quit;

    for (i=0; cmdList[i] != NULL; i++)
        if (strcmp(cmdList[i], cmd) == 0)
            return i;
    return -1;

}

void
cmdsUsage(char *cmd, char *cmdList[], char *cmdInfo[])
{

    static char intCmds[] = " ";
    int i;

    fprintf(stderr, "\n");
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ": %s [-h] [-V] [command] [command_options] [command_args]\n\n", cmd);
    fprintf(stderr, _i18n_msg_get(ls_catd,NL_SETN,102,
                "    where 'command' is:\n\n")); /* catgets 102 */

    for (i = 0; cmdList[i] != NULL; i++)
        if (strstr(intCmds,cmdList[i]) == NULL )

            fprintf(stderr, "    %-12.12s%s\n", cmdList[i], cmdInfo[i]);
    exit(-1);
}


void
oneCmdUsage(int i, char *cmdList[], char *cmdSyntax[])
{
    fprintf(stderr, I18N_Usage);
    fprintf(stderr, ":    %-12.12s%s\n", cmdList[i], cmdSyntax[i]);
    fflush(stderr);

}


void
cmdHelp (int argc, char **argv, char *cmdList[], char *cmdInfo[],
        char *cmdSyntax[])
{

    static char intCmds[] = " ";
    int i, j = 0;

    if (argc <= optind) {

        fprintf(stderr, "\n%s\n\n", _i18n_msg_get(ls_catd,NL_SETN, 104, "Commands are : "));  /* catgets 104  */

        for (i=0; cmdList[i] != NULL; i++) {

            if (strstr(intCmds,cmdList[i]) == NULL ) {
                j++;
                fprintf(stderr, "%-12.12s",  cmdList[i]);
                if (j % 6 == 0)
                    fprintf(stderr, "\n");
            }
        }
        fprintf(stderr, "\n\n%s\n\n", _i18n_msg_get(ls_catd,NL_SETN, 105, "Try help command... to get details. ")); /* catgets 105 */
        fflush(stderr);
        return;
    }

    for (; argc > optind; optind++)
        if ((i = adminCmdIndex(argv[optind], cmdList)) != -1) {
            oneCmdUsage(i, cmdList, cmdSyntax);
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 106,
                        "Function: %s\n\n"), /* catgets 106 */
                    cmdInfo[i]);
        } else
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 107,
                        "Invalid command <%s>\n\n"), /* catgets 107 */
                    argv[optind]);
    fflush(stderr);

}

char *
myGetOpt (int nargc, char **nargv, char *ostr)
{
    char svstr [256];
    char *cp1 = svstr;
    char *cp2 = svstr;
    char *optName;
    int i, num_arg;

    if ((optName = nargv[optind]) == NULL)
        return NULL;
    if (optind >= nargc || *optName != '-')
        return NULL;
    if (optName[1] && *++optName == '-') {
        ++optind;
        return NULL;
    }
    if (ostr == NULL)
        return NULL;
    strcpy (svstr, ostr);
    num_arg = 0;
    optarg = NULL;

    while (*cp2) {
        int cp2len = strlen(cp2);
        for (i=0; i<cp2len; i++) {
            if (cp2[i] == '|') {
                num_arg = 0;
                cp2[i] = '\0';
                break;
            }
            else if (cp2[i] == ':') {
                num_arg = 1;
                cp2[i] = '\0';
                break;
            }
        }
        if (i >= cp2len)
            return BADCH;

        if (!strcmp (optName, cp1)) {
            if (num_arg) {
                if (nargc <= optind + 1) {
                    fprintf (stderr,
                            _i18n_msg_get(ls_catd, NL_SETN, 108,
                                "%s: option requires an argument -- %s\n"),/* catgets 108 */
                            nargv[0], optName);
                    return BADCH;
                }
                optarg = nargv[++optind];
            }
            ++optind;
            return optName;
        }
        cp1 = &cp2[i];
        cp2 = ++cp1;
    }
    fprintf (stderr, _i18n_msg_get(ls_catd,NL_SETN,109,
                "%s: illegal option -- %s\n"), /* catgets 109 */
            nargv[0], optName);
    return BADCH;

}

int
getConfirm (char *msg)
{
    char answer[MAXLINELEN];
    int i = 0;

    while (1) {
        fputs(msg, stdout);
        fflush(stdout);
        if (fgets(answer, MAXLINELEN, stdin) == NULL) {
            return false;
        }
        i = 0;
        while (answer[i] == ' ')
            i++;
        if ((answer[i] == 'y' || answer[i] == 'n'
                    || answer[i] == 'Y' || answer[i] == 'N')
                && answer[i+1] == '\n')
            break;
    }
    return ((answer[i] == 'Y' || answer[i] == 'y'));

}

int
checkConf(int verbose, int who)
{
    char confCheckBuf[] = "RECONFIG_CHECK=true";
    pid_t pid;
    char *daemon, *lsfEnvDir;
    static struct config_param lsfParams[] =
    {
        {"LSF_SERVERDIR", NULL},
        {"LSF_CONFDIR", NULL},
        {"LSB_CONFDIR", NULL},
        {"LSB_SHAREDIR", NULL},
        {NULL, NULL},
    };
    struct config_param *plp;
    int status;
    int fatalErr = false, cc = 0;
    int fd;

    if (lsfParams[0].paramValue == NULL) {
        lsfEnvDir = getenv("LSF_ENVDIR");
        cc = initenv_(lsfParams, lsfEnvDir);
    }
    if (cc < 0) {
        if (lserrno == LSE_CONF_SYNTAX) {
            char lno[20];
            sprintf (lno, _i18n_msg_get(ls_catd, NL_SETN, 110,
                        "Line %d"), errLineNum_); /* catgets 110 */
            ls_perror(lno);
        } else
            ls_perror("initenv_");
    }
    plp = lsfParams;
    for (; plp->paramName != NULL; plp++)
        if (plp->paramValue == NULL) {
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 111,
                        "%s is missing or has a syntax error in lsf.conf file\n"),/* catgets 111 */
                    plp->paramName);
            fatalErr = true;
        }
    if (fatalErr)
        return EXIT_FATAL_ERROR;
    if (cc < 0)
        return EXIT_WARNING_ERROR;

    if ((daemon = calloc(strlen(lsfParams[0].paramValue)+15,
                         sizeof(char))) == NULL) {
        perror("calloc");
        return EXIT_FATAL_ERROR;
    }

    strcpy(daemon, lsfParams[0].paramValue);

    strcat(daemon, ((who == 1) ? "/lim" : "/mbatchd"));

    if (access(daemon, X_OK) < 0) {
        perror(daemon);
        free(daemon);
        return EXIT_FATAL_ERROR;
    }

    if (putenv(confCheckBuf)) {
        fprintf(stderr,
                _i18n_msg_get(ls_catd, NL_SETN, 112,
                    "Failed to set environment variable RECONFIG_CHECK\n"));    /* catgets 112 */
        free(daemon);
        return EXIT_FATAL_ERROR;
    }


    if ((pid = fork()) < 0) {
        perror("fork");
        free(daemon);
        return EXIT_FATAL_ERROR;
    }

    if (pid == 0) {
        if (!verbose) {
            fd = open(LSDEVNULL, O_RDWR);
            dup2(fd, 1);
            dup2(fd, 2);
        }

        uid_t uid = getuid();
        if (uid != 0) {
            if (seteuid(uid) < 0) {
                char buf[MICROBUF_SIZ];
                sprintf(buf, "%s: setuid() failed %m", __func__);
                perror(buf);
                exit(-1);
            }
        }
        execlp(daemon, daemon, "-C", (char *)0);
        perror("execlp");

        exit(EXIT_RUN_ERROR);
    }


    free(daemon);
    fprintf(stderr, (_i18n_msg_get(ls_catd,NL_SETN,115, "\nChecking configuration files ...\n"))); /* catgets 115 */

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return EXIT_FATAL_ERROR;
    }

    if (WIFEXITED(status) != 0 && WEXITSTATUS(status) != 0xf8)
        if (verbose)
            fprintf(stderr, "---------------------------------------------------------\n");


    if (WIFEXITED(status) == 0) {
        fprintf(stderr,
                _i18n_msg_get(ls_catd, NL_SETN, 116,
                    "Child process killed by signal.\n\n")); /* catgets 116 */
        return EXIT_FATAL_ERROR;
    }

    switch (WEXITSTATUS(status)) {
        case  0 :
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 117,
                        "No errors found.\n\n"));  /* catgets 117 */
            return EXIT_NO_ERROR;

        case  0xff :
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 118,
                        "There are fatal errors.\n\n")); /* catgets 118 */
            return EXIT_FATAL_ERROR;

        case  0xf8 :
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 119,
                        "Fail to run checking program \n\n")); /* catgets 119 */
            return EXIT_FATAL_ERROR;

        case  0xfe :
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 120,
                        "No fatal errors found.\n\n"));/* catgets 120 */
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 121, "Warning: Some configuration parameters may be incorrect.\n"));  /* catgets 121 */
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 122,
                        "         They are either ignored or replaced by default values.\n\n"));  /* catgets 122  */
            return EXIT_WARNING_ERROR;

        default :
            fprintf(stderr,
                    _i18n_msg_get(ls_catd, NL_SETN, 123,
                        "Errors found.\n\n"));  /* catgets 123 */
            return EXIT_FATAL_ERROR;
    }

}
