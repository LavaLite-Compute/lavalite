\

# NAME

**brestart** - restarts checkpointed jobs

# SYNOPSIS

**brestart **\[*bsub options*\] \[**-f**\] *checkpoint_dir *\[*job_ID*
\| **\"***job_ID***\[***index***\]\"**\]

**brestart** \[**-h** \| **-V**\]

# OPTION LIST

\
**-B** .br **-f** .br **-N** .br **-x** .br **-b ***begin_time* .br **-C
***core_limit* .br **-c** \[*hour***:**\]*minute*\[**/***host_name* \|
**/***host_model*\]\
**-D ***data_limit* .br **-E \"***pre_exec_command *\[*argument
*\...\]**\"** .br **-F ***file_limit* .br **-m**
**\"***host_name*\[**+**\[*pref_level*\]\] \|
*host_group*\[**+**\[*pref_level*\]\] \...**\"** .br **-G ***user_group*
.br **-M ***mem_limit* .br **-q*** ***\"***queue_name *\...**\"** .br
**-S ***stack_limit* .br **-t ***term_time* .br **-w**
**\`***dependency_expression***\'** .br **-W
***run_limit*\[**/***host_name\| ***/***host_model*\]\
*checkpoint_dir *\[*job_ID* \| **\"***job_ID***\[***index***\]\"**\]\
\[**-h** \| **-V**\]

# DESCRIPTION

Restarts a checkpointed job using the checkpoint files saved in
*checkpoint_dir/last_job_ID/*. Only jobs that have been successfully
checkpointed can be restarted.

Jobs are re-submitted and assigned a new job ID. The checkpoint
directory is renamed using the new job ID,
*checkpoint_dir/new_job_ID/. * .PP By default, jobs are restarted with
the same output file and file transfer specifications, job name, window
signal value, checkpoint directory and period, and rerun options as the
original job.

To restart a job on another host, both hosts must be binary compatible,
run the same OS version, have access to the executable, have access to
all open files (Lava must locate them with an absolute path name), and
have access to the checkpoint directory.

The environment variable LSB_RESTART is set to Y when a job is
restarted.

Lava invokes the erestart(8) executable found in LSF_SERVERDIR to
perform the restart.

Only the bsub options listed here can be used with brestart.

# OPTIONS

Only the bsub options listed in the option list above can be used for
brestart. Except for the following option, see bsub(1) for a description
of brestart options.

**-f**

:   

    Forces the job to be restarted even if non-restartable conditions
    exist (these conditions are operating system specific).

# SEE ALSO

bsub(1), bjobs(1), bmod(1), bqueues(1), bhosts(1), bchkpnt(1),
lsb.queues(5), echkpnt(8), erestart(8), mbatchd(8)

# LIMITATIONS

In kernel-level checkpointing, you cannot change the value of core
limit, CPU limit, stack limit or memory limit with brestart.
