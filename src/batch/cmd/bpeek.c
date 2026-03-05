/* $Id: bpeek.c,v 1.5 2007/08/15 22:18:43 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */

// NOTE since we disable remote execution beep does not work as it used it
// we need to rewrite bpeek using a ssh command, a python script would
// do most likely

#include "lsbatch/cmd/cmd.h"

static void peek_file(char *, struct jobInfoEnt *, char);

static void usage(void)
{
    fprintf(stderr,
            "%s: usage: [-h] [-V] [-f] [-m host_name | -q queue_name | "
            "-J job_name | jobId\n",
            __func__);
}

int main(int argc, char **argv)
{
    char *queue = NULL;
    char *host = NULL;
    char *jobName = NULL;
    int64_t jobId;
    int options;
    struct jobInfoEnt *jInfo;
    char *outFile;
    char fflag = FALSE;
    int cc;

    if (lsb_init(argv[0]) < 0) {
        lsb_perror("lsb_init");
        return -1;
    }

    while ((cc = getopt(argc, argv, "Vhfq:m:J:")) != EOF) {
        switch (cc) {
        case 'q':
            queue = optarg;
            break;
        case 'm':
            host = optarg;
            break;
        case 'J':
            jobName = optarg;
            break;
        case 'V':
            fprintf(stderr, "%s\n", LAVALITE_VERSION_STR);
            return -1;
        case 'f': // tail -f
            fflag = TRUE;
            break;
        case 'h':
        default:
            usage();
            return -1;
        }
    }

    cc = lsb_openjobinfo(jobId, jobName, NULL, queue, host, options);
    if (cc < 0) {
        return -1;
    }
    while ((jInfo = lsb_readjobinfo())) {
        ;
    }
    lsb_closejobinfo();

    if (jobId && jInfo->jobId != jobId) {
        lsberrno = LSBE_JOB_ARRAY;
        lsb_perror("bpeek");
        return -1;
    }

    if ((jInfo->submit.options & SUB_INTERACTIVE)) {
        fprintf(stderr, "Job <%s> : Cannot bpeek an interactive job.\n",
                lsb_jobid2str(jInfo->jobId));
        return -1;
    }

    if (IS_PEND(jInfo->status) || jInfo->execUsername[0] == '\0') {
        fprintf(stderr, "Job <%s> : Not yet started.\n",
                lsb_jobid2str(jInfo->jobId));
        return -1;
    }
    if (IS_FINISH(jInfo->status)) {
        fprintf(stderr, "Job <%s> : Already finished.\n",
                lsb_jobid2str(jInfo->jobId));
        exit(-1);
    }

    char *output = lsb_peekjob(jInfo->jobId);
    if (output == NULL) {
        char msg[LL_BUFSIZ_64];
        sprintf(msg, "%s <%s>", "Job", lsb_jobid2str(jInfo->jobId));
        lsb_perror(msg);
        return -1;
    }

    peek_file(output, jInfo, fflag);

    return 0;
}

static void peek_file(char *jobFile, struct jobInfoEnt *jInfo, char fflag)
{
    return;
}
