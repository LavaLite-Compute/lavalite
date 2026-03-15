\

# NAME

**bsub** - submits a batch job to Lava

# SYNOPSIS

**bsub **\[*options*\] *command *\[*arguments*\]\
**bsub **\[**-h** \| **-V**\]

# OPTION LIST

\
**-B** .br **-H** .br \[**-I** \| **-Ip** \| **-Is** \| **-K**\]\
**-N** .br **-r** .br **-x** .br **-b
**\[\[*month***:**\]*day***:**\]*hour***:**minute\
**-C** *core_limit*\
**-c** \[*hours***:**\]*minutes*\[**/**host_name \| **/**host_model\]\
**-D** data_limit\
**-e **err_file\
.br **-E \"**pre_exec_command \[*argument *\...\]**\"** .br **-f **
**\"**local_file op \[*remote_file*\]**\" **\...\
**-F **file_limit\
\[**-i **i*nput_file* \| **-is** *input_file*\]\
**-J** *job_name \| ***-J**
**\"**job_name**\[**index_list**\]%**job_limit**\"** .br **-k
\"**checkpoint_dir \[*checkpoint_period*\]
\[**method=**method_name\]**\"** .br **-L** login_shell\
**-m** **\"**host_name\[**+**\[*pref_level*\]\] \|
*host_group*\[**+**\[*pref_level*\]\] \...**\"** .br **-M** *mem_limit*\
**-n **min_processors\[**,**max_processors\]\
**-o** out_file\
**-P** project_name\
**-q** **\"**queue_name \...**\"** .br **-R** **\"**res_req**\"**\
-**sp** *priority* .br **-S **stack_limit\
**-t **\[\[*month***:**\]*day***:**\]*hour***:**minute\
**-u** *mail_user* .br **-v** *swap_limit* .br **-w**
**\'**dependency_expression**\'** .br **-W
**\[*hours***:**\]*minutes*\[**/**host_name \| **/**host_model\]\
**-Zs** .SH DESCRIPTION

Submits a job for batch execution and assigns it a unique numerical job
ID.

Runs the job on a host that satisfies all requirements of the job, when
all conditions on the job, host, queue, and cluster are satisfied. If
Lava cannot run all jobs immediately, Lava scheduling policies determine
the order of dispatch. Jobs are started and suspended according to the
current system load.

Sets the user\'s execution environment for the job, including the
current working directory, file creation mask, and all environment
variables, and sets Lava environment variables before starting the job.

When a job is run, the command line and stdout/stderr buffers are stored
in the directory home_directory/.lsbatch on the execution host. If this
directory is not accessible, /tmp/.lsbtmpuser_ID is used as the job\'s
home directory. If the current working directory is under the home
directory on the submission host, then the current working directory is
also set to be the same relative directory under the home directory on
the execution host. The job is run in /tmp if the current working
directory is not accessible on the execution host.

If no command is supplied for **bsub**, **bsub** prompts for the command
from the standard input. On LINUX, the input is terminated by entering
CTRL-D on a new line.

By default, Lava assumes that uniform user names and user ID spaces
exist among all the hosts in the cluster. That is, a job submitted by a
given user will run under the same user\'s account on the execution
host. For situations where nonuniform user names and user ID spaces
exist, account mapping must be used to determine the account used to run
a job.

By default, uses the command name as the job name. Quotation marks are
significant.

By default, the job is not checkpointable.

By default, automatically selects an appropriate queue. If you defined a
default queue list by setting LSB_DEFAULTQUEUE, the queue is selected
from your list. If LSB_DEFAULTQUEUE is not defined, the queue is
selected from the system default queue list specified by the Lava
administrator (see the parameter DEFAULT_QUEUE in lsb.params(5)).

By default, Lava tries to obtain resource requirement information for
the job from the remote task list that is maintained by the load sharing
library (see **lsfintro**(1)). If the job is not listed in the remote
task list, the default resource requirement is to run the job on a host
or hosts that are of the same host type (see **lshosts**(1)) as the
submission host.

By default, assumes only one processor is requested.

By default, does not start a login shell but runs the job file under the
execution environment from which the job was submitted.

By default, the input file for the batch job is /dev/null (no input).

By default, sends mail to you when the job is done. The default
destination is defined by LSB_MAILTO in lsf.conf. The mail message
includes the job report, the job output (if any), and the error message
(if any).

By default, charges the job to the default project. The default project
is the project you define by setting the environment variable
LSB_DEFAULTPROJECT. If you do not set LSB_DEFAULTPROJECT, the default
project is the project specified by the Lava administrator in the
lsb.params configuration file (see the DEFAULT_PROJECT parameter in
lsb.params(5)). If DEFAULT_PROJECT is not defined, then Lava uses
default as the default project name.

Use **-n** to submit a parallel job.

Use **-I**, **-Is**, or **-Ip** to submit a batch interactive job.

Use **-J** to assign a name to your job.

Use **-k** to specify a checkpointable job.

To kill a batch job submitted with **bsub**, use **bkill**.

Use **bmod** to modify jobs submitted with **bsub**. **bmod** takes
similar options to **bsub**.

# OPTIONS

**-B**

:   

    Sends mail to you when the job is dispatched and begins execution.

```{=html}
<!-- -->
```

**-H**

:   

    Holds the job in the PSUSP state when the job is submitted. The job
    will not be scheduled until you tell the system to resume the job
    (see** ** bresume(1)).

```{=html}
<!-- -->
```

**-I** \| **-Ip** \| **-Is**

:   

    Submits a batch interactive job. A new job cannot be submitted until
    the interactive job is completed or terminated.

> Sends the job\'s standard output (or standard error) to the terminal.
> Does not send mail to you when the job is done unless you specify the
> **-N** option.

> Terminal support is available for a batch interactive job.

> When you specify the **-Ip** option, submits a batch interactive job
> and creates a pseudo-terminal when the job starts. Some applications
> (for example, **vi**) require a pseudo-terminal in order to run
> correctly.

> When you specify the **-Is** option, submits a batch interactive job
> and creates a pseudo-terminal with shell mode support when the job
> starts. This option should be specified for submitting interactive
> shells, or applications which redefine the CTRL-C and CTRL-Z keys (for
> example, jove).

> If the **-i** *input_file *option is specified, you cannot interact
> with the job\'s standard input via the terminal.

> If the **-o** *out_file* option is specified, sends the job\'s
> standard output to the specified output file. If the **-e** *err_file*
> option is specified, sends the job\'s standard error to the specified
> error file*.*

> You cannot use **-I**, **-Ip**, or **-Is** with the **-K** option.

**-K**

:   

    Submits a batch job and waits for the job to complete. Sends the
    message \"Waiting for dispatch\" to the terminal when you submit the
    job. Sends the message \"Job is finished\" to the terminal when the
    job is done.

> You will not be able to submit another job until the job is completed.
> This is useful when completion of the job is required in order to
> proceed, such as a job script. If the job needs to be rerun due to
> transient failures, **bsub** returns after the job finishes
> successfully. **bsub** will exit with the same exit code as the job so
> that job scripts can take appropriate actions based on the exit codes.
> **bsub** exits with value 126 if the job was terminated while pending.

> You cannot use the **-K** option with the **-I**, **-Ip**, or **-Is**
> options.

**-N**

:   

    Sends the job report to you by mail when the job finishes. When used
    without any other options, behaves the same as the default.

> Use only with **-o**, **-I**, **-Ip**, and **-Is** options, which do
> not send mail, to force Lava to send you a mail message when the job
> is done.

**-r**

:   

    If the execution host becomes unavailable while a job is running,
    specifies that the job will rerun on another host. Lava requeues the
    job in the same job queue with the same job ID. When an available
    execution host is found, reruns the job as if it were submitted new.
    You receive a mail message informing you of the host failure and
    requeuing of the job.

> If the system goes down while a job is running, specifies that the job
> will be requeued when the system restarts.

> Reruns a job if the execution host or the system fails; it does not
> rerun a job if the job itself fails.

> If the execution host becomes unavailable after a job has been
> checkpointed (see **bsub -k** and **bchkpnt**(1)), the job is
> restarted from the last checkpoint. The restarted job is requeued for
> execution in the same way that you would restart a job using
> **brestart**(1). In order for the job to be successfully restarted,
> the job\'s checkpoint directory must reside in a shared file system
> accessible to the host receiving the restarted job.

**-x **

:   

    Puts the host running your job into exclusive execution mode.

> In exclusive execution mode, your job runs by itself on a host. It is
> dispatched only to a host with no other jobs running, and Lava does
> not send any other jobs to the host until the job completes.

> To submit a job in exclusive execution mode, the queue must be
> configured to allow exclusive jobs.

> When the job is dispatched, **bhosts**(1) reports the host status as
> closed_Excl, and **lsload**(1) reports the host status as lockU.

> Until your job is complete, the host is not selected by LIM in
> response to placement requests made by **lsplace**(1) or any other
> load sharing applications.

> You can force other batch jobs to run on the host by using the **-m**
> *host_name* option of **brun**(1) to explicitly specify the locked
> host.

**-b **\[\[*month***:**\]*day***:**\]*hour***:**minute 

:   

    Dispatches the job for execution on or after the specified date and
    time. The date and time are in the form of
    \[\[month:\]day:\]hour:minute where the number ranges are as
    follows: month 1-12, day 1-31, hour 0-23, minute 0-59.

> At least two fields must be specified. These fields are assumed to be
> hour:minute. If three fields are given, they are assumed to be
> day:hour:minute, and four fields are assumed to be
> month:day:hour:minute.

**-C** core_limit

:   

    Sets a per-process (soft) core file size limit for all the processes
    that belong to this batch job (see **getrlimit**(2)). The core limit
    is specified in kilobytes.

> The behavior of this option depends on platform-specific LINUX
> systems.

> In some cases, the process is sent a SIGXFSZ signal if the job
> attempts to create a core file larger than the specified limit. The
> SIGXFSZ signal normally terminates the process.

> In other cases, the writing of the core file terminates at the
> specified limit.

**-c** \[hours**:**\]minutes\[**/**host_name \| **/**host_model\] 

:   

    Limits the total CPU time the job can use. This option is useful for
    preventing runaway jobs or jobs that use up too many resources. When
    the total CPU time for the whole job has reached the limit, a
    SIGXCPU signal is first sent to the job, then SIGINT, SIGTERM, and
    SIGKILL.

> If LSB_JOB_CPULIMIT in lsf.conf is set to n, Lava-enforced CPU limit
> is disabled and Lava passes the limit to the operating system. When
> one process in the job exceeds the CPU limit, the limit is enforced by
> the operating system.

> The CPU limit is in the form of \[hours**:**\]minutes. The minutes can
> be specified as a number greater than 59. For example, three and a
> half hours can either be specified as 3:30, or 210.

> Optionally, you can supply a host name or a host model name defined in
> Lava. You must insert \`/\' between the CPU limit and the host name or
> model name. (See **lsinfo**(1) to get host model information.) If a
> host name or model name is not given, Lava uses the default CPU time
> normalization host defined at the queue level (DEFAULT_HOST_SPEC in
> lsb.queues) if it has been configured, otherwise uses the default CPU
> time normalization host defined at the cluster level
> (DEFAULT_HOST_SPEC in lsb.params) if it has been configured, otherwise
> uses the submission host.

> The CPU time you specify is the normalized CPU time. This is done so
> that the job does approximately the same amount of processing for a
> given CPU limit, even if it is sent to host with a faster or slower
> CPU. Whenever a normalized CPU time is given, the actual time on the
> execution host is the specified time multiplied by the CPU factor of
> the normalization host then divided by the CPU factor of the execution
> host.

**-D **data_limit 

:   

    Sets a per-process (soft) data segment size limit for each of the
    processes that belong to the batch job (see **getrlimit**(2)). The
    data limit is specified in kilobytes. A **sbrk** call to extend the
    data segment beyond the data limit will return an error.

```{=html}
<!-- -->
```

**-e** err_file 

:   

    Specify a file path. Appends the standard error output of the job to
    the specified file.

> If you use the special character %J in the name of the error file,
> then %J is replaced by the job ID of the job. If you use the special
> character %I in the name of the error file, then %I is replaced by the
> index of the job in the array if the job is a member of an array.
> Otherwise, %I is replaced by 0 (zero).

> If the current working directory is not accessible on the execution
> host after the job starts, Lava writes the standard error output file
> to /tmp/.

**-E** **\"**pre_exec_command \[arguments \...\]**\"** 

:   

    Runs the specified pre-exec command on the batch job\'s execution
    host before actually running the job. For a parallel job, the
    pre-exec command runs on the first host selected for the parallel
    job. If the pre- exec command exits with 0 (zero), then the real job
    is started on the selected host. Otherwise, the job (including the
    pre-exec command) goes back to PEND status and is rescheduled.

> If your job goes back into PEND status, Lava will keep on trying to
> run the pre-exec command and the real job when conditions permit. For
> this reason, be sure that your pre-exec command can be run many times
> without having side effects.

> The standard input and output for the pre-exec command are directed to
> the same files as for the real job. The pre-exec command runs under
> the same user ID, environment, home, and working directory as the real
> job. If the pre-exec command is not in the user\'s normal execution
> path (the \$PATH variable), the full path name of the command must be
> specified.

**-f** **\"**local_file op \[remote_file\]**\"** \...

:   

    Copies a file between the local (submission) host and the remote
    (execution) host. Specify absolute or relative paths, including the
    file names. You should specify the remote file as a file name with
    no path when running in non-shared systems.

> If the remote file is not specified, it defaults to the local file,
> which must be given. Use multiple **-f** options to specify multiple
> files.

> *op*

> > An operator that specifies whether the file is copied to the remote
> > host, or whether it is copied back from the remote host. The
> > operator must be surrounded by white space.
>
> > The following describes the operators:
>
> > \> Copies the local file to the remote file before the job starts.
> > Overwrites the remote file if it exists.
>
> > \< Copies the remote file to the local file after the job completes.
> > Overwrites the local file if it exists.
>
> > \<\< Appends the remote file to the local file after the job
> > completes. The local file must exist.
>
> > \>\< Copies the local file to the remote file before the job starts.
> > Overwrites the remote file if it exists. Then copies the remote file
> > to the local file after the job completes. Overwrites the local
> > file.
>
> > \<\> Copies the local file to the remote file before the job starts.
> > Overwrites the remote file if it exists. Then copies the remote file
> > to the local file after the job completes. Overwrites the local
> > file.

> If you use the **-i** *input_file *option, then you do not have to use
> the **-f** option to copy the specified input file to the execution
> host. Lava does this for you, and removes the input file from the
> execution host after the job completes.

> If you use the **-e** *err_file* or the **-o** *out_file* option, and
> you want the specified file to be copied back to the submission host
> when the job completes, then you must use the **-f** option.

> If the submission and execution hosts have different directory
> structures, you must ensure that the directory where the remote file
> and local file will be placed exists.

> If the local and remote hosts have different file name spaces, you
> must always specify relative path names. If the local and remote hosts
> do not share the same file system, you must ensure that the directory
> containing the remote file exists. It is recommended that only the
> file name be given for the remote file when running in heterogeneous
> file systems. This places the file in the job\'s current working
> directory. If the file is shared between the submission and execution
> hosts, then no file copy is performed.

> Lava uses **lsrcp** to transfer files (see **lsrcp**(1) command).
> **lsrcp** contacts RES on the remote host to perform the file
> transfer. If RES is not available, **rcp** is used (see **rcp**(1)).
> The user must ensure that the **rcp** binary is in the user\'s \$PATH
> on the execution host.

> Jobs that are submitted from Lava client hosts should specify the
> **-f** option only if **rcp** is allowed. Similarly, **rcp** must be
> allowed if account mapping is used.

**-F** file_limit 

:   

    Sets a per-process (soft) file size limit for each of the processes
    that belong to the batch job (see **getrlimit**(2)). The file size
    limit is specified in kilobytes. If a job process attempts to write
    to a file that exceeds the file size limit, then that process is
    sent a SIGXFSZ signal. The SIGXFSZ signal normally terminates the
    process.

```{=html}
<!-- -->
```

**-i **input_file \| **-is** input_file

:   

    Gets the standard input for the job from specified file. Specify an
    absolute or relative path. The input file can be any type of file,
    though it is typically a shell script text file.

> If the file exists on the execution host, Lava uses it. Otherwise,
> Lava attempts to copy the file from the submission host to the
> execution host. For the file copy to be successful, you must allow
> remote copy (**rcp**) access, or you must submit the job from a server
> host where RES is running. The file is copied from the submission host
> to a temporary file in the directory specified by the JOB_SPOOL_DIR
> parameter, or your \$HOME/.lsbatch directory on the execution host.
> Lava removes this file when the job completes.

> The **-is** option spools the input file to the directory specified by
> the JOB_SPOOL_DIR parameter in lsb.params, and uses the spooled file
> as the input file for the job. By default, if JOB_SPOOL_DIR is not
> specified, the input file is spooled to
> LSB_SHAREDIR/cluster_name/lsf_indir. If the lsf_indir directory does
> not exist, Lava creates it before spooling the file. Lava removes the
> spooled file when the job completes. Use the **-is** option if you
> need to modify or remove the input file before the job completes.
> Removing or modifying the original input file does not affect the
> submitted job.

> Unless you use **-is**, you can use the special characters %J and %I
> in the name of the input file. %J is replaced by the job ID. %I is
> replaced by the index of the job in the array, if the job is a member
> of an array, otherwise by 0 (zero). The special characters %J and %I
> are not valid with the **-is** option.

**-J** job_name

:   \
    **-J** **\"**job_name**\[**index*\_list***\]%**job_slot_limit**\"**

    Assigns the specified name to the job, and, for job arrays,
    specifies the indices of the job array and optionally the maximum
    number of jobs that can run at any given time.

> The job name need not be unique.

> To specify a job array, enclose the index list in square brackets, as
> shown, and enclose the entire job array specification in quotation
> marks, as shown. The index list is a comma-separated list whose
> elements have the syntax start\[-end\[**:**step\]\] where start, end
> and step are positive integers. If the step is omitted, a step of one
> is assumed. The job array index starts at one. By default, the maximum
> job array index is 1000.

> You may also use a positive integer to specify the system-wide job
> slot limit (the maximum number of jobs that can run at any given time)
> for this job array.

> All jobs in the array share the same job ID and parameters. Each
> element of the array is distinguished by its array index.

> After a job is submitted, you use the job name to identify the job.
> Specify **\"**job_ID**\[**index**\]\"** to** **work with elements of a
> particular array. Specify **\"**job_name**\[**index**\]\"** to work
> with elements of all arrays with the same name. Since job names are
> not unique, multiple job arrays may have the same name with a
> different or same set of indices.

**-k \"**checkpoint_dir \[checkpoint_period\]\[**method=**method_name\]**\"**

:   

    Makes a job checkpointable and specifies the checkpoint directory.
    If you omit the checkpoint period, the quotes are not required.
    Specify a relative or absolute path name.

> When a job is checkpointed, the checkpoint information is stored in
> *checkpoint_dir*/*job_ID*/*file_name*. Multiple jobs can checkpoint
> into the same directory. The system can create multiple files.

> The checkpoint directory is used for restarting the job (see
> **brestart**(1)).

> Optionally, specifies a checkpoint period in minutes. Specify a
> positive integer. The running job is checkpointed automatically every
> checkpoint period. The checkpoint period can be changed using
> **bchkpnt**(1). Because checkpointing is a heavyweight operation, you
> should choose a checkpoint period greater than half an hour.

> Optionally, specifies a custom checkpoint and restart method to use
> with the job. Use **method=default** to indicate to use Lava\'s
> default checkpoint and restart programs for the job, echkpnt.default
> and erestart.default.

> The echkpnt.method_name and erestart.method_name programs must be in
> LSF_SERVERDIR or in the directory specified by LSB_ECHKPNT_METHOD_DIR
> (environment variable or set in lsf.conf).

> If a custom checkpoint and restart method is already specified with
> LSB_ECHKPNT_METHOD (environment variable or in lsf.conf), the method
> you specify with bsub -k overrides this.

> Process checkpointing is not available on all host types, and may
> require linking programs with a special libraries (see
> **libckpt.a**(3)). Lava invokes **echkpnt** (see **echkpnt**(8)) found
> in LSF_SERVERDIR to checkpoint the job. You can override the default
> **echkpnt** for the job by defining as environment variables or in
> lsf.conf LSB_ECHKPNT_METHOD and LSB_ECHKPNT_METHOD_DIR to point to
> your own **echkpnt**. This allows you to use other checkpointing
> facilities, including application-level checkpointing.

\-**L** login_shell 

:   

    Initializes the execution environment using the specified login
    shell. The specified login shell must be an absolute path. This is
    not necessarily the shell under which the job will be executed.

```{=html}
<!-- -->
```

**-m** **\"**host_name\[**+**\[pref_level\]\] \| host_group\[**+**\[pref_level\]\] \...**\"**

:   

    Runs the job on one of the specified hosts.

> By default, if multiple hosts are candidates, runs the job on the
> least- loaded host. To change this, put a plus (+) after the names of
> hosts or host groups that you would prefer to use, optionally followed
> by a preference level. For preference level, specify a positive
> integer, with higher numbers indicating greater preferences for those
> hosts.

> For example, -m \"hostA groupB+2 hostC+1\" indicates that groupB is
> the most preferred and hostA is the least preferred.

> For information about host groups, use **bmgroup**.

> The keyword others can be specified with or without a preference level
> to refer to other hosts not otherwise listed. The keyword others must
> be specified with at least one host name or host group, it cannot be
> specified by itself. For example, -m \"hostA+ others\" means that
> hostA is preferred over all other hosts.

> If you use both the **-m \"***host_name*\[+\[*pref_level*\]\] \|
> *host_group*\[+\[*pref_level*\]\]\...\" option and the **-q**
> *queue_name* option, the specified queue must be configured to include
> all the hosts in the your host list. Otherwise, the job is not
> submitted. To find out what hosts are configured for the queue, use
> **bqueues -l**.

**-M** mem_limit 

:   

    Specify the memory limit, in kilobytes.

> If LSB_MEMLIMIT_ENFORCE or LSB_JOB_MEMLIMIT are set to y in lsf.conf,
> Lava kills the job when it exceeds the memory limit. Otherwise, Lava
> passes the memory limit to the operating system. UNIX operating
> systems that support RUSAGE_RSS for **setrlimit()** can apply the
> memory limit to each process.

**-n** min_proc\[**,**max_proc\] 

:   

    Submits a parallel job and specifies the minimum and maximum numbers
    of processors required to run the job (some of the processors may be
    on the same multiprocessor host). If you do not specify a maximum,
    the number you specify represents the exact number of processors to
    use.

> If the maximum number of processors is greater than the process limit
> of the queue to which the job is submitted, Lava will reject the job
> (see the PROCLIMIT parameter in lsb.queues(5)).

> Once at least the minimum number of processors is available, the job
> is dispatched to the first host selected. The list of selected host
> names for the job are specified in the environment variables LSB_HOSTS
> and LSB_MCPU_HOSTS. The job itself is expected to start parallel
> components on these hosts and establish communication among them,
> optionally using RES.

**-o** out_file 

:   

    Specify a file path. Appends the standard output of the job to the
    specified file. Sends the output by mail if the file does not exist,
    or the system has trouble writing to it.

> If only a file name is specified, Lava writes the output file to the
> current working directory. If the current working directory is not
> accessible on the execution host after the job starts, Lava writes the
> standard output file to /tmp/.

> If you use **-o** without **-e**, the standard error of the job is
> stored in the output file.

> If you use **-o** without **-N**, the job report is stored in the
> output file as the file header.

> If you use both **-o** and **-N**, the output is stored in the output
> file and the job report is sent by mail. The job report itself does
> not contain the output, but the report will advise you where to find
> your output.

> If you use the special character %J in the name of the output file,
> then %J is replaced by the job ID of the job. If you use the special
> character %I in the name of the output file, then %I is replaced by
> the index of the job in the array, if the job is a member of an array.
> Otherwise, %I is replaced by 0 (zero).

**-P** project_name 

:   

    Assigns the job to the specified project.

```{=html}
<!-- -->
```

**-p** process_limit

:   

    Sets the limit of the number of processes to *process_limit* for the
    whole job. The default is no limit. Exceeding the limit causes the
    job to terminate.

```{=html}
<!-- -->
```

**-q** **\"**queue_name \...**\"**

:   

    Submits the job to one of the specified queues. Quotes are optional
    for a single queue. For a list of available queues, use **bqueues**.

> When a list of queue names is specified, Lava selects the most
> appropriate queue in the list for your job based on the job\'s
> resource limits, and other restrictions, such as the requested hosts,
> your accessibility to a queue, queue status (closed or open), whether
> a queue can accept exclusive jobs, etc. The order in which the queues
> are considered is the same order in which these queues are listed. The
> queue listed first is considered first.

**-R \"**res_req**\"** 

:   

    Runs the job on a host that meets the specified resource
    requirements. Specify the resource requirement string as usual. The
    size of the resource requirement string is limited to 512 bytes.

> Any run-queue-length-specific resource, such as r15s, r1m or r15m,
> specified in the resource requirements refers to the normalized run
> queue length.

\-**sp** priority

:   

    Specifies user-assigned job priority which allow users to order
    their jobs in a queue. Valid values for priority are any integers
    between 1 and MAX_USER_PRIORITY (displayed by **bparams -l**).
    Incorrect job priorities are rejected. Lava and queue administrators
    can specify priorities beyond MAX_USER_PRIORITY.

> The job owner can change the priority of their own jobs. Lava and
> queue administrators can change the priority of all jobs in a queue.

> Job order is the first consideration to determine job eligibility for
> dispatch. Jobs are still subject to all scheduling policies regardless
> of job priority. Jobs with the same priority are ordered first come
> first served.

> User-assigned job priority can be configured with automatic job
> priority escalation to automatically increase the priority of jobs
> that have been pending for a specified period of time.

**-S** stack_limit 

:   

    Sets a per-process (soft) stack segment size limit (KB) for each of
    the processes that belong to the batch job (see **getrlimit**(2)).

```{=html}
<!-- -->
```

**-t **\[\[*month***:**\]*day***:**\]*hour***:**minute 

:   

    Specifies the job termination deadline. If a LINUX job is still
    running at the termination time, the job is sent a SIGUSR2 signal,
    and is killed if it does not terminate within ten minutes. (For a
    detailed description of how these jobs are killed, see **bkill**.)
    In the queue definition, a TERMINATE action can be configured to
    override the **bkill** default action (see the JOB_CONTROLS
    parameter in lsb.queues(5)).

> The format for the termination time is \[\[month:\]day:\]hour:minute
> where the number ranges are as follows: month 1-12, day 1-31, hour
> 0-23, minute 0-59.

> At least two fields must be specified. These fields are assumed to be
> hour:minute. If three fields are given, they are assumed to be
> day:hour:minute, and four fields are assumed to be
> month:day:hour:minute.

**-u** mail_user

:   

    Sends mail to the specified email destination.

```{=html}
<!-- -->
```

**-v** swap_limit

:   

    Set the total process virtual memory limit to *swap_limit* in KB for
    the whole job. The default is no limit. Exceeding the limit causes
    the job to terminate.

```{=html}
<!-- -->
```

**-w** **\'**dependency_expression**\'**

:   

    Lava will not place your job unless the dependency expression
    evaluates to TRUE. If you specify a dependency on a job that Lava
    cannot find (such as a job that has not yet been submitted), your
    job submission fails.

> The dependency expression is a logical expression composed of one or
> more dependency conditions. To make dependency expression of multiple
> conditions, use the following logical operators:

> && (AND)

> \|\| (OR)

> ! (NOT)

> Use parentheses to indicate the order of operations, if necessary.

> Enclose the dependency expression in single quotes (\') to prevent the
> shell from interpreting special characters (space, any logic operator,
> or parentheses). If you use single quotes for the dependency
> expression, use double quotes for quoted items within it, such as job
> names.

> In dependency conditions, job names specify only your own jobs, unless
> you are an Lava administrator. By default, if you use the job name to
> specify a dependency condition, and more than one of your jobs has the
> same name, all of your jobs that have that name must satisfy the test.
> If JOB_DEP_LAST_SUB in lsb.params is set to 1, the test is done on the
> job submitted most recently. Use double quotes (\") around job names
> that begin with a number. In the job name, specify the wildcard
> character asterisk (\*) at the end of a string, to indicate all jobs
> whose name begins with the string. For example, if you use jobA\* as
> the job name, it specifies jobs named jobA, jobA1, jobA_test,
> jobA.log, etc.

> Use the \* with dependency conditions to define one-to-one dependency
> among job array elements such that each element of one array depends
> on the corresponding element of another array. The job array size must
> be identical. For example, bsub **-w ** \"done(myarrayA\[\*\])\" -J
> \"myArrayB\[1-10\]\" myJob2 indicates that before element 1 of
> myArrayB can start, element 1 of myArrayA must be completed, and so
> on.

> You can also use the \* to establish one-to-one array element
> dependencies with bmod after an array has been submitted.

> If you want to specify array dependency by array name, set
> JOB_DEP_LAST_SUB in lsb.params. If you do not have this parameter set,
> the job will be rejected if one of your previous arrays has the same
> name but a different index.

> In dependency conditions, the variable *op* represents one of the
> following relational operators:

> \>

> \>=

> \<

> \<=

> ==

> !=

> Use the following conditions to form the dependency expression.

> **done(**job_ID \|**\"**job_name**\"** \...**)**

> > The job state is DONE.
>
> > Lava refers to the oldest job of *job_name* in memory.

> **ended(**job_ID \| **\"**job_name**\")**

> > The job state is EXIT or DONE.

> **exit(**job_ID \| **\"**job_name**\"** \[**,**\[op\] exit_code\]**)**

> > The job state is EXIT, and the job\'s exit code satisfies the
> > comparison test.
>
> > If you specify an exit code with no operator, the test is for
> > equality (== is assumed).
>
> > If you specify only the job, any exit code satisfies the test.

> **external(**job_ID \| **\"**job_name**\",** **\"**status_text**\")**

> > Specify the first word of the job status or message description (no
> > spaces). Only the first word is evaluated.
>
> > The job has the specified job status, or the text of the job\'s
> > status begins with the specified word.

> job_ID \| **\"**job_name**\"**

> > If you specify a job without a dependency condition, the test is for
> > the DONE state (Lava assumes the \"done\" dependency condition by
> > default).

> **numdone(**job_ID, op number \| **\*)**

> > For a job array, the number of jobs in the DONE state satisfies the
> > test. Use \* (with no operator) to specify all the jobs in the
> > array.

> **numended(**job_ID, op number \| **\*)**

> > For a job array, the number of jobs in the DONE or EXIT states
> > satisfies the test. Use \* (with no operator) to specify all the
> > jobs in the array.

> **numexit(**job_ID*,* op number \| **\*)**

> > For a job array, the number of jobs in the EXIT state satisfies the
> > test. Use \* (with no operator) to specify all the jobs in the
> > array.

> **numhold(**job_ID*,* op number \| **\*)**

> > For a job array, the number of jobs in the PSUSP state satisfies the
> > test. Use \* (with no operator) to specify all the jobs in the
> > array.

> **numpend(**job_ID*,* op number \| **\*)**

> > For a job array, the number of jobs in the PEND state satisfies the
> > test. Use \* (with no operator) to specify all the jobs in the
> > array.

> **numrun(**job_ID*,* op number \| **\*)**

> > For a job array, the number of jobs in the RUN state satisfies the
> > test. Use \* (with no operator) to specify all the jobs in the
> > array.

> **numstart(**job_ID*,* op number \| **\*)**

> > For a job array, the number of jobs in the RUN, USUSP, or SSUSP
> > states satisfies the test. Use \* (with no operator) to specify all
> > the jobs in the array.

> **post_done(**job_ID \| **\"**job_name**\")**

> > The job state is POST_DONE (the post-processing of specified job has
> > completed without errors).

> **post_err(**job_ID \| **\"**job_name**\")**

> > The job state is POST_ERR (the post-processing of the specified job
> > has completed with errors).

> **started(**job_ID \| **\"**job_name**\")**

> > The job state is:
>
> > \- RUN, DONE, or EXIT
>
> > \- PEND or PSUSP, and the job has a pre-execution command (bsub -E)
> > that is running.

**-W** \[hours**:**\]minutes\[**/**host_name \| **/**host_model\]

:   

    Sets the run time limit of the batch job. If a LINUX job runs longer
    than the specified run limit, the job is sent a SIGUSR2 signal, and
    is killed if it does not terminate within ten minutes. (For a
    detailed description of how these jobs are killed, see **bkill**.)
    In the queue definition, a TERMINATE action can be configured to
    override the **bkill** default action (see the JOB_CONTROLS
    parameter in lsb.queues(5)).

> The run limit is in the form of \[hours**:**\]minutes. The minutes can
> be specified as a number greater than 59. For example, three and a
> half hours can either be specified as 3:30, or 210.

> Optionally, you can supply a host name or a host model name defined in
> Lava. You must insert \"/\" between the run limit and the host name or
> model name. (See **lsinfo**(1) to get host model information.) If a
> host name or model name is not given, Lava uses the default CPU time
> normalization host defined at the queue level (DEFAULT_HOST_SPEC in
> lsb.queues) if it has been configured, otherwise uses the default CPU
> time normalization host defined at the cluster level
> (DEFAULT_HOST_SPEC in lsb.params) if it has been configured, otherwise
> uses the submission host.

> The CPU time you specify is the normalized CPU time. This is done so
> that the job does approximately the same amount of processing, even if
> it is sent to host with a faster or slower CPU. Whenever a normalized
> CPU time is given, the actual time on the execution host is the
> specified time multiplied by the CPU factor of the normalization host
> then divided by the CPU factor of the execution host.

> If the job also has termination time specified through the **bsub -t**
> option, Lava determines whether the job can actually run for the
> specified length of time allowed by the run limit before the
> termination time. If not, then the job will be aborted. If the
> IGNORE_DEADLINE parameter is set in lsb.queues(5), this behavior is
> overridden and the run limit is ignored.

**-Zs**

:   

    Spools a job command file to the directory specified by the
    JOB_SPOOL_DIR parameter in lsb.params, and uses the spooled file as
    the command file for the job.

> By default, if JOB_SPOOL_DIR is not specified, the input file is
> spooled to LSB_SHAREDIR/*cluster_name*/lsf_cmddir. If the lsf_cmddir
> directory does not exist, Lava creates it before spooling the file.
> Lava removes the spooled file when the job completes.

> The **-Zs** option is not supported for embedded job commands because
> Lava is unable to determine the first command to be spooled in an
> embedded job command.

**-h**

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V**

:   

    Prints Lava release version to stderr and exits.

```{=html}
<!-- -->
```

command \[argument\]

:   

    The job can be specified by a command line argument command, or
    through the standard input if the command is not present on the
    command line. The* command* can be anything that is provided to a
    UNIX Bourne shell (see **sh**(1)). command is assumed to begin with
    the first word that is not part of a **bsub** option. All arguments
    that follow *command* are provided as the arguments to the
    *command*.

> If the batch job is not given on the command line, **bsub** reads the
> job commands from standard input. If the standard input is a
> controlling terminal, the user is prompted with \"bsub\>\" for the
> commands of the job. The input is terminated by entering CTRL-D on a
> new line. You can submit multiple commands through standard input. The
> commands are executed in the order in which they are given. **bsub**
> options can also be specified in the standard input if the line begins
> with #BSUB; e.g., \"#BSUB -x\". If an option is given on both the
> **bsub** command line, and in the standard input, the command line
> option overrides the option in the standard input. The user can
> specify the shell to run the commands by specifying the shell path
> name in the first line of the standard input, such as \"#!/bin/csh\".
> If the shell is not given in the first line, the Bourne shell is used.
> The standard input facility can be used to spool a user\'s job script;
> such as \"bsub \< script\". See EXAMPLES below for examples of
> specifying commands through standard input.

# OUTPUT

If the job is successfully submitted, displays the job ID and the queue
to which the job has been submitted.

# EXAMPLES

\% **bsub sleep 100 ** .IP Submit the UNIX command sleep together with
its argument 100 as a batch job.

\% **bsub -q short -o my_output_file \"pwd; ls\" ** .IP Submit the LINUX
command pwd and ls as a batch job to the queue named short and store the
job output in my_output file.

\% **bsub -m \"host1 host3 host8 host9\" my_program ** .IP Submit
my_program to run on one of the candidate hosts: host1, host3, host8 and
host9.

\% **bsub -q \"queue1 queue2 queue3\" -c 5 my_program ** .IP Submit
my_program to one of the candidate queues: queue1, queue2, and queue3
which are selected according to the CPU time limit specified by -c 5.

\% **bsub -I ls ** .IP Submit a batch interactive job which displays the
output of ls at the user\'s terminal.

\% **bsub -Ip vi myfile ** .IP Submit a batch interactive job to edit
myfile.

\% **bsub -Is csh ** .IP Submit a batch interactive job that starts up
csh as an interactive shell.

\% **bsub -b 20:00 -J my_job_name my_program ** .IP Submit my_program to
run after 8 p.m. and assign it the job name my_job_name.

\% **bsub my_script ** .IP Submit my_script as a batch job. Since
my_script is specified as a command line argument, the my_script file is
not spooled. Later changes to the my_script file before the job
completes may affect this job.

\% **bsub \< default_shell_script ** .IP where default_shell_script
contains:

> sim1.exe\
> sim2.exe

> The file default_shell_script is spooled, and the commands will be run
> under the Bourne shell since a shell specification is not given in the
> first line of the script.

\% **bsub \< csh_script ** .IP where csh_script contains:

> #!/bin/csh\
> sim1.exe\
> sim2.exe

> csh_script is spooled and the commands will be run under /bin/csh.

\% **bsub -q night \< my_script ** .IP where my_script contains:

> #!/bin/sh\
> #BSUB -q test\
> #BSUB -o outfile -e errfile \# my default stdout, stderr files\
> #BSUB -m \"host1 host2\" \# my default candidate hosts\
> #BSUB -f \"input \> tmp\" -f \"output \<\< tmp\"\
> #BSUB -D 200 -c 10/host1\
> #BSUB -t 13:00\
> #BSUB -k \"dir 5\"\
> sim1.exe\
> sim2.exe

> The job is submitted to the night queue instead of test, because the
> command line overrides the script.

\% **bsub -b 20:00 -J my_job_name ** .IP bsub\> sleep 1800\
bsub\> my_program\
bsub\> CTRL-D

> The job commands are entered interactively.

# LIMITATIONS

When using account mapping the command bpeek(1) will not work. File
transfer via the -f option to bsub(1) requires rcp(1) to be working
between the submission and execution hosts. Use the -N option to request
mail, and/or the -o and -e options to specify an output file and error
file, respectively.

# SEE ALSO

bjobs(1), bkill(1),bqueues(1), bhosts(1), bmgroup(1), bmod(1),
bchkpnt(1), brestart(1), sh(1), getrlimit(2), sbrk(2), libckpt.a(3),
lsb.users(5), lsb.queues(5), lsb.params(5), lsb.hosts(5), mbatchd(8)
