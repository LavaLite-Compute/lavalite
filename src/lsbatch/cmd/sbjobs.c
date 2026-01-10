/*
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
 */

#include "lsf/lsbatch/lib/lsb.h"
#include "lsf/lsbatch/lib/lsb.sbdlib.h"

static const char *
yn(int v)
{
    return v ? "y" : "n";
}

static void
print_jobs(const char *host, struct sbdJobInfo *jobs, int n)
{
    printf("host=%s jobs=%d\n", host, n);
    printf("%-8s %-6s %-6s %-5s %-5s %-3s %-3s %-3s %-3s %-3s %-3s %-4s %-8s %s\n",
           "JOBID", "PID", "PGID", "ST", "STEP",
           "PA", "EA", "FA",
           "RS", "ES", "FS",
           "EV", "EXIT", "JOBFILE");

    for (int i = 0; i < n; i++) {
        struct sbdJobInfo *j = &jobs[i];

        printf("%-8ld %-6d %-6d %-5d %-5d %-3s %-3s %-3s %-3s %-3s %-3s %-4s %-8d %s\n",
               (long)j->job_id,
               j->pid,
               j->pgid,
               j->state,
               j->step,
               yn(j->pid_acked),
               yn(j->execute_acked),
               yn(j->finish_acked),
               yn(j->reply_sent),
               yn(j->execute_sent),
               yn(j->finish_sent),
               yn(j->exit_status_valid),
               j->exit_status,
               j->job_file ? j->job_file : "");
    }
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <host> [host...]\n", argv[0]);
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        const char *host = argv[i];
        struct sbdJobInfo *jobs = NULL;
        int n = 0;

        if (sbd_job_info(host, &jobs, &n) < 0) {
            fprintf(stderr, "sbjobs: %s: failed\n", host);
            continue;
        }

        print_jobs(host, jobs, n);
        sbd_job_info_free(jobs, n);
    }

    return 0;
}
