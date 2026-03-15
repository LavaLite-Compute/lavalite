\

# NAME

**lsf.conf**

## Overview

Installation of and operation of Lava is controlled by the lsf.conf
file. This chapter explains the contents of the lsf.conf file.

## Contents

> · About lsf.conf
>
> · Parameters

# About lsf.conf

The lsf.conf file is created during installation by the Lava setup
program, and records all the settings chosen when Lava was installed.
The lsf.conf file dictates the location of the specific configuration
files and operation of individual servers and applications.

The lsf.conf file is used by Lava and applications built on top of it.
For example, information in lsf.conf is used by Lava daemons and
commands to locate other configuration files, executables, and network
services. lsf.conf is updated, if necessary, when you upgrade to a new
version.

This file can also be expanded to include Lava application-specific
parameters.

# Location

The default location of lsf.conf is in /etc. This default location can
be overridden when necessary by either the environment variable
LSF_ENVDIR or the command line option **-d** available to some of the
applications.

# Format

Each entry in lsf.conf has one of the following forms:

NAME=VALUE

NAME=

NAME=\"STRING1 STRING2 \...\"

The equal sign = must follow each NAME even if no value follows and
there should be no space beside the equal sign.

A value that contains multiple strings separated by spaces must be
enclosed in quotation marks.

Lines starting with a pound sign (#) are comments and are ignored. Do
not use #if as this is reserved syntax for time-based configuration.

# Parameters

# LSB_API_CONNTIMEOUT

## Syntax

**LSB_API_CONNTIMEOUT=***time_seconds* .SS Description

The timeout in seconds when connecting to the Batch system.

## Valid Values

Any positive integer or zero

## Default

10

## See Also

LSB_API_RECVTIMEOUT

# LSB_API_RECVTIMEOUT

## Syntax

**LSB_API_RECVTIMEOUT=***time_seconds* .SS Description

Timeout in seconds when waiting for a reply from the Batch system.

## Valid Values

Any positive integer or zero

## Default

0

## See Also

LSB_API_CONNTIMEOUT

# LSB_CMD_LOG_MASK

## Syntax

**LSB_CMD_LOG_MASK=***log_level* .SS Description

Specifies the logging level of error messages from Lava Batch commands.

For example:

LSB_CMD_LOG_MASK=LOG_DEBUG

Batch commands log error messages in different levels so that you can
choose to log all messages, or only log messages that are deemed
critical. The level specified by LSB_CMD_LOG_MASK determines which
messages are recorded and which are discarded. All messages logged at
the specified level or higher are recorded, while lower level messages
are discarded.

For debugging purposes, the level LOG_DEBUG contains the fewest number
of debugging messages and is used only for very basic debugging. The
level LOG_DEBUG3 records all debugging messages, and is not often used.
Most debugging is done at the level LOG_DEBUG2.

The commands log to the syslog facility unless LSB_CMD_LOGDIR is set.

## Valid Values

The log levels from highest to lowest are:

> · LOG_EMERG
>
> · LOG_ALERT
>
> · LOG_CRIT
>
> · LOG_ERR
>
> · LOG_WARNING
>
> · LOG_NOTICE
>
> · LOG_INFO
>
> · LOG_DEBUG
>
> · LOG_DEBUG1
>
> · LOG_DEBUG2
>
> · LOG_DEBUG3

## Default

LOG_WARNING

## See Also

LSB_CMD_LOGDIR, LSB_DEBUG, LSB_TIME_CMD, LSF_LOG_MASK, LSB_DEBUG_CMD,
LSF_TIME_CMD

# LSB_CMD_LOGDIR

## Syntax

**LSB_CMD_LOGDIR=***path* .SS Description

Specifies the path to the Batch command log files.

## Default

/tmp

## See Also

LSB_CMD_LOG_MASK, LSF_LOGDIR

# LSB_CONFDIR

## Syntax

**LSB_CONFDIR=***dir* .SS Description

Specifies the path to the directory containing the Lava configuration
files.

Lava Batch configuration directories are installed under LSB_CONFDIR.

Configuration files for each Lava cluster are stored in a subdirectory
of LSB_CONFDIR. This subdirectory contains several files that define
Lava Batch user and host lists, operation parameters, and queues.

All files and directories under LSB_CONFDIR must be readable from all
hosts in the cluster. LSB_CONFDIR/*cluster_name*/configdir must be owned
by the Lava administrator.

Do not redefine this parameter after Lava has been installed. To move
these directories to another location, use lsfsetup and choose the
Product Install option to install configuration files in another
location.

## Default

LSF_CONFDIR/lsbatch

## See Also

LSF_CONFDIR

# LSB_DEBUG

## Syntax

**LSB_DEBUG=1** \| **2** .SS Description

Sets the Batch system to debug.

If defined, Lava Batch will run in single user mode. In this mode, no
security checking is performed; do not run the Lava Batch daemons as
root.

When LSB_DEBUG is defined, Lava will not look in the system services
database for port numbers. Instead, it uses the port numbers defined by
the parameters LSB_MBD_PORT/LSB_SBD_PORT in lsf.conf. If these
parameters are not defined, it uses port number 40000 for MBD and port
number 40001 for SBD.

You should always specify 1 for this parameter unless you are testing
Lava Batch.

Can also be defined from the command line.

## Valid Values

> · LSB_DEBUG=1

> Lava Batch runs in the background with no associated control terminal.

> · LSB_DEBUG=2

> Lava Batch runs in the foreground and prints error messages to tty.

## Default

Undefined

## See Also

LSF_LIM_DEBUG, LSF_RES_DEBUG

# LSB_DEBUG_CMD

## Syntax

**LSB_DEBUG_CMD=***log_class* .SS Description

Sets the debugging log class for commands and APIs.

Specifies the log class filtering that will be applied to Batch commands
or the API. Only messages belonging to the specified log class are
recorded.

LSB_DEBUG_CMD (which sets the log class) is used in combination with
LSB_CMD_LOG_MASK (which sets the log level). For example:

LSB_CMD_LOG_MASK=LOG_DEBUG\
LSB_DEBUG_CMD=\"LC_TRACE LC_EXEC\"

Debugging is turned on when you define both parameters.

The daemons log to the syslog facility unless LSB_CMD_LOGDIR is defined.

To specify multiple log classes, use a space-separated list enclosed by
quotation marks. For example:

LSB_DEBUG_CMD=\"LC_TRACE LC_EXEC\"

Can also be defined from the command line.

## Valid Values

Valid log classes are:

> · LC_AUTH - Log authentication messages
>
> · LC_CHKPNT - Log checkpointing messages
>
> · LC_COMM - Log communication messages
>
> · LC_ELIM - Log ELIM messages
>
> · LC_EXEC - Log significant steps for job execution
>
> · LC_FILE - Log file transfer messages
>
> · LC_HANG - Mark where a program might hang
>
> · LC_JARRAY - Log job array messages
>
> · LC_JLIMIT - Log job slot limit messages
>
> · LC_LOADINDX - Log load index messages
>
> · LC_PEND - Log messages related to job pending reasons
>
> · LC_PERFM - Log performance messages
>
> · LC_PIM - Log PIM messages
>
> · LC_SIGNAL - Log messages pertaining to signals
>
> · LC_SYS - Log system call messages
>
> · LC_TRACE - Log significant program walk steps
>
> · LC_XDR - Log everything transferred by XDR

## Default

Undefined

## See Also

LSB_CMD_LOG_MASK, LSB_CMD_LOGDIR

# LSB_DEBUG_MBD

## Syntax

**LSB_DEBUG_MBD=***log_class* .SS Description

Sets the debugging log class for MBD.

Specifies the log class filtering that will be applied to MBD. Only
messages belonging to the specified log class are recorded.

LSB_DEBUG_MBD (which sets the log class) is used in combination with
LSF_LOG_MASK (which sets the log level). For example:

LSF_LOG_MASK=LOG_DEBUG\
LSB_DEBUG_MBD=\"LC_TRACE LC_EXEC\"

To specify multiple log classes, use a space-separated list enclosed in
quotation marks. For example:

LSB_DEBUG_MBD=\"LC_TRACE LC_EXEC\"

You need to restart the daemons after setting LSB_DEBUG_MBD for your
changes to take effect.

If you use the command badmin mbddebug to temporarily change this
parameter without changing lsf.conf, you will not need to restart the
daemons.

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

## Valid Values

Valid log classes are the same as for LSB_DEBUG_CMD except for the log
classes LC_ELIM and LC_JARRAY which cannot be used with LSB_DEBUG_MBD.
See LSB_DEBUG_CMD.

## Default

Undefined

## See Also

LSF_LOG_MASK, LSF_LOGDIR, **badmin mbddebug**

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

This parameter can also be defined from the command line.

## Valid Values

For a list of valid log classes, see LSB_DEBUG_CMD.

## Default

Undefined

## See Also

LSB_DEBUG_CMD, LSF_LOG_MASK, LSF_LOGDIR

# LSB_DEBUG_SBD

## Syntax

**LSB_DEBUG_SBD=***log_class* .SS Description

Sets the debugging log class for SBD.

Specifies the log class filtering that will be applied to SBD. Only
messages belonging to the specified log class are recorded.

LSB_DEBUG_SBD (which sets the log class) is used in combination with
LSF_LOG_MASK (which sets the log level). For example:

LSF_LOG_MASK=LOG_DEBUG\
LSB_DEBUG_SBD=\"LC_TRACE LC_EXEC\"

To specify multiple log classes, use a space-separated list enclosed in
quotation marks. For example:

LSB_DEBUG_SBD=\"LC_TRACE LC_EXEC\"

You need to restart the daemons after setting LSB_DEBUG_SBD for your
changes to take effect.

If you use the command badmin sbddebug to temporarily change this
parameter without changing lsf.conf, you will not need to restart the
daemons.

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

## Valid Values

Valid log classes are the same as for LSB_DEBUG_CMD except for the log
classes LC_ELIM and LC_JARRAY which cannot be used with LSB_DEBUG_SBD.
See LSB_DEBUG_CMD.

## Default

Undefined

## See Also

LSB_DEBUG_MBD, LSF_LOG_MASK, LSF_LOGDIR, **badmin** .SH
LSB_ECHKPNT_METHOD

## Syntax

**LSB_ECHKPNT_METHOD=***method_name* .SS Description

Name of custom echkpnt and erestart methods.

Can also be defined as an environment variable, or specified through the
bsub -k option.

The name you specify here will be used for both your custom echkpnt and
erestart programs. You must assign your custom *echkpnt* and *erestart*
programs the name echkpnt.*method_name* and erestart.*method_name*. The
programs echkpnt.*method_name* and erestart.*method_name*. must be in
LSF_SERVERDIR or in the directory specified by LSB_ECHKPNT_METHOD_DIR.

Do not define LSB_ECHKPNT_METHOD=default as default is a reserved
keyword to indicate to use Lava\'s default echkpnt and erestart methods.
You can however, specify bsub -k \"my_dir method=default\" my_job to
indicate that you want to use Lava\'s default checkpoint and restart
methods.

When this parameter is undefined in lsf.conf or as an environment
variable and no custom method is specified at job submission through
bsub -k, Lava uses echkpnt.default and erestart.default to checkpoint
and restart jobs.

When this parameter is defined, Lava uses the custom checkpoint and
restart methods specified.

## Limitations

The method name and directory(LSB_ECHKPNT_METHOD_DIR) combination must
be unique in the cluster.

For example, you may have two echkpnt applications with the same name
such as echkpnt.mymethod but what differentiates them is the different
directories defined with LSB_ECHKPNT_METHOD_DIR. It is the cluster
administrator\'s responsibility to ensure that method name and method
directory combinations are unique in the cluster.

## Default

Undefined; Lava uses echkpnt.default and erestart.default to checkpoint
and restart jobs

## Product

Lava Base, Lava Batch

## See Also

LSB_ECHKPNT_METHOD_DIR, LSB_ECHKPNT_KEEP_OUTPUT

# LSB_ECHKPNT_METHOD_DIR

## Syntax

**LSB_ECHKPNT_METHOD_DIR=***path* .SS Description

Absolute path name of the directory in which custom echkpnt and erestart
programs are located.

Can also be defined as an environment variable.

## Default

Undefined; Lava searches in LSF_SERVERDIR for custom echkpnt and
erestart programs

## Product

Lava Base, Lava Batch

## See Also

LSB_ECHKPNT_METHOD, LSB_ECHKPNT_KEEP_OUTPUT

# LSB_ECHKPNT_KEEP_OUTPUT

## Syntax

**LSB_ECHKPNT_KEEP_OUTPUT=y**\|**Y** .SS Description

Saves the standard output and standard error of custom echkpnt and
erestart methods to:

> · *checkpoint_dir*/\$LSB_JOBID/echkpnt.out
>
> · *checkpoint_dir*/\$LSB_JOBID/echkpnt.err
>
> · *checkpoint_dir*/\$LSB_JOBID/erestart.out
>
> · *checkpoint_dir*/\$LSB_JOBID/erestart.err

Can also be defined as an environment variable.

## Default

Undefined; standard error and standard output messages from custom
echkpnt and erestart programs is directed to /dev/null and discarded by
Lava.

## Product

Lava Base, Lava Batch

## See Also

LSB_ECHKPNT_METHOD, LSB_ECHKPNT_METHOD_DIR

# LSB_INTERACT_MSG_ENH

## Syntax

**LSB_INTERACT_MSG_ENH = y** \| **Y** .SS Description

If set, enables enhanced messaging for interactive batch jobs. To
disable interactive batch job messages, set LSB_INTERACT_MSG_ENH to any
value other than y or Y; for example, LSB_INTERACT_MSG_ENH=N.

## Default

Undefined

## See Also

LSB_INTERACT_MSG_INTVAL

# LSB_INTERACT_MSG_INTVAL

## Syntax

**LSB_INTERACT_MSG_INTVAL =** *seconds* .SS Description

Specifies the update interval in seconds for interactive batch job
messages. LSB_INTERACT_MSG_INTVAL is ignored if LSB_INTERACT_MSG_ENH is
not set.

Because the job information that Lava uses to get the pending or
suspension reason is updated according to the value of MBD_SLEEP_TIME,
there is no advantage to setting LSB_INTERACT_MSG_INTVAL less than
MBD_SLEEP_TIME

## Default

Undefined. If LSB_INTERACT_MSG_INTVAL is set to an incorrect value, the
default update interval is 60 seconds.

## See Also

LSB_INTERACT_MSG_ENH in lsf.conf, MBD_SLEEP_TIME in lsb.params

# LSB_JOB_CPULIMIT

## Syntax

**LSB_JOB_CPULIMIT = y** \| **n** .SS Description

Determines whether the CPU limit is a per-process limit enforced by the
OS or whether it is a per-job limit enforced by Lava:

> · The per-process limit is enforced by the OS when the CPU time of one
> process of the job exceeds the CPU limit.
>
> · The per-job limit is enforced by Lava when the total CPU time of all
> processes of the job exceed the CPU limit.

This parameter applies to CPU limits set when a job is submitted with
**bsub -c** , and to CPU limits set for queues by CPULIMIT in
lsb.queues.

The setting of LSB_JOB_CPULIMIT has the following effect on how the
limit is enforced:

When LSB_JOB_CPULIMIT is Y, the Lava-enforced per-job limit is enabled,
and the OS-enforced per-process limit is disabled.

When LSB_JOB_CPULIMIT is N, the Lava-enforced per-job limit is disabled,
and the OS-enforced per-process limit is enabled.

When LSB_JOB_CPULIMIT is undefined, the Lava-enforced per-job limit is
enabled, and the OS-enforced per-process limit is enabled.

> · Lava-enforced per-job limit\--When the sum of the CPU time of all
> processes of a job exceed the CPU limit, Lava sends a SIGXCPU signal
> (where supported by the operating system) from the operating system to
> all processes belonging to the job, then SIGINT, SIGTERM and SIGKILL.
> The interval between signals is 10 seconds by default.

> On UNIX, the time interval between SIGXCPU, SIGINT, SIGKILL, SIGTERM
> can be configured with the parameter JOB_TERMINATE_INTERVAL in
> lsb.params.

> · OS-enforced per process limit\--When one process in the job exceeds
> the CPU limit, the limit is enforced by the operating system. For more
> details, refer to your operating system documentation for setrlimit().

## Default

Undefined

## Notes

To make LSB_JOB_CPULIMIT take effect, use the command badmin hrestart
all to restart all SBDs in the cluster.

Changing the default Terminate job control action\--You can define a
different terminate action in lsb.queues with the parameter JOB_CONTROLS
if you do not want the job to be killed. For more details on job
controls, see the *Lava Administrator\'s Guide*.

## Limitations

If a job is running and the parameter is changed, Lava is not able to
reset the type of limit enforcement for running jobs.

> · If the parameter is changed from per-process limit enforced by the
> OS to per-job limit enforced by Lava (LSB_JOB_CPULIMIT=n changed to
> LSB_JOB_CPULIMIT=y), both per-process limit and per-job limit will
> affect the running job. This means that signals may be sent to the job
> either when an individual process exceeds the CPU limit or the sum of
> the CPU time of all processes of the job exceed the limit. A job that
> is running may be killed by the OS or by Lava.
>
> · If the parameter is changed from per-job limit enforced by Lava to
> per-process limit enforced by the OS (LSB_JOB_CPULIMIT=y changed to
> LSB_JOB_CPULIMIT=n), the job will be allowed to run without limits
> because the per-process limit was previously disabled.

## See Also

lsb.queues, bsub, JOB_TERMINATE_INTERVAL, LSB_MOD_ALL_JOBS

# LSB_JOB_MEMLIMIT

## Syntax

**LSB_JOB_MEMLIMIT=y** \| **n** .SS Description

Determines whether the memory limit is a per-process limit enforced by
the OS or whether it is a per-job limit enforced by Lava.

> · The per-process limit is enforced by the OS when the memory
> allocated to one process of the job exceeds the memory limit.
>
> · The per-job limit is enforced by Lava when the sum of the memory
> allocated to all processes of the job exceeds the memory limit.

This parameter applies to memory limits set when a job is submitted with
**bsub -M ***mem_limit*, and to memory limits set for queues with
MEMLIMIT in lsb.queues.

The setting of LSB_JOB_MEMLIMIT has the following effect on how the
limit is enforced:

When LSB_JOB_MEMLIMIT is Y, the Lava-enforced per-job limit is enabled,
and the OS-enforced per-process limit is disabled.

When LSB_JOB_MEMLIMIT is N or undefined, the Lava-enforced per- job
limit is disabled, and the OS-enforced per-process limit is enabled.

> · Lava-enforced per-job limit\--When the total memory allocated to all
> processes in the job exceeds the memory limit, Lava sends the
> following signals to kill the job: SIGINT, SIGTERM, then SIGKILL. The
> interval between signals is 10 seconds by default.

> On UNIX, the time interval between SIGINT, SIGKILL, SIGTERM can be
> configured with the parameter JOB_TERMINATE_INTERVAL in lsb.params.

> · OS-enforced per process limit\--When the memory allocated to one
> process of the job exceeds the memory limit, the operating system
> enforces the limit. Lava passes the memory limit to the operating
> system. Some operating systems apply the memory limit to each process,
> and some do not enforce the memory limit at all.

> OS memory limit enforcement is only available on systems that support
> RUSAGE_RSS for setrlimit().

## Default

Undefined; per-process memory limit enforced by the OS; per-job memory
limit enforced by Lava disabled

## Notes

To make LSB_JOB_MEMLIMIT take effect, use the command badmin hrestart
all to restart all SBDs in the cluster.

If LSB_JOB_MEMLIMIT is set, it overrides the setting of the parameter
LSB_MEMLIMIT_ENFORCE. The parameter LSB_MEMLIMIT_ENFORCE is ignored.

The difference between LSB_JOB_MEMLIMIT set to y and
LSB_MEMLIMIT_ENFORCE set to y is that with LSB_JOB_MEMLIMIT, only the
per-job memory limit enforced by Lava is enabled. The per- process
memory limit enforced by the OS is disabled. With LSB_MEMLIMIT_ENFORCE
set to y, both the per-job memory limit enforced by Lava and the
per-process memory limit enforced by the OS are enabled.

Changing the default Terminate job control action\--You can define a
different Terminate action in lsb.queues with the parameter JOB_CONTROLS
if you do not want the job to be killed. For more details on job
controls, see the *Lava Administrator\'s Guide*.

## Limitations

If a job is running and the parameter is changed, Lava is not able to
reset the type of limit enforcement for running jobs.

> · If the parameter is changed from per-process limit enforced by the
> OS to per-job limit enforced by Lava (LSB_JOB_MEMLIMIT=n or undefined
> changed to LSB_JOB_MEMLIMIT=y), both per-process limit and per-job
> limit will affect the running job. This means that signals may be sent
> to the job either when the memory allocated to an individual process
> exceeds the memory limit or the sum of memory allocated to all
> processes of the job exceed the limit. A job that is running may be
> killed by Lava.
>
> · If the parameter is changed from per-job limit enforced by Lava to
> per-process limit enforced by the OS (LSB_JOB_MEMLIMIT=y changed to
> LSB_JOB_MEMLIMIT=n or undefined), the job will be allowed to run
> without limits because the per-process limit was previously disabled.

## See Also

LSB_MEMLIMIT_ENFORCE, lsb.queues, bsub, JOB_TERMINATE_INTERVAL,
LSB_MOD_ALL_JOBS

# LSB_MAILPROG

## Syntax

**LSB_MAILPROG=***file_name* .SS Description

Path and file name of the mail program used by the Batch system to send
email.

This is the electronic mail program that Lava will use to send system
messages to the user.

When Lava needs to send email to users it invokes the program defined by
LSB_MAILPROG in lsf.conf. You can write your own custom mail program and
set LSB_MAILPROG to the path where this program is stored.

The Lava administrator can set the parameter as part of cluster
reconfiguration.

Provide the name of any mail program. For your convenience, Lava
provides the following mail programs:

> · sendmail: Supports the sendmail protocol on UNIX
>
> > If lsmail is specified, the parameter LSB_MAILSERVER must also be
> > specified.
>
> If this parameter is modified, the Lava administrator must restart SBD
> on all hosts to retrieve the new value.
>
> > · On UNIX:
>
> > Lava Batch normally uses /usr/lib/sendmail as the mail transport
> > agent to send mail to users. Lava Batch calls LSB_MAILPROG with two
> > arguments; one argument gives the full name of the sender, and the
> > other argument gives the return address for Batch mail.
>
> > LSB_MAILPROG must read the body of the mail message from the
> > standard input. The end of the message is marked by end-of-file. Any
> > program or shell script that accepts the arguments and input, and
> > delivers the mail correctly, can be used.
>
> > LSB_MAILPROG must be executable by any user.

## Examples

LSB_MAILPROG=/serverA/tools/lsf/bin/unixhost.exe

## Default

/usr/lib/sendmail (UNIX)

## See Also

LSB_MAILSERVER, LSB_MAILTO

# LSB_MAILSERVER

## Syntax

**LSB_MAILSERVER=***mail_protocol:mail_server* .SS Description

If this parameter is modified, the Lava administrator must restart SBD
on all hosts to retrieve the new value.

## Examples

LSB_MAILSERVER = EXCHANGE:Host2@company.com

LSB_MAILSERVER = SMTP:MailHost

## Default

Undefined

## See Also

LSB_MAILPROG

# LSB_MAILSIZE_LIMIT

## Syntax

**LSB_MAILSIZE_LIMIT=***email_size_in_KB* .SS Description

Limits the size of the email containing Lava batch job output
information.

The Lava Batch system sends job information such as CPU, process and
memory usage, job output, and errors in email to the submitting user
account. Some batch jobs can create large amounts of output. To prevent
large job output files from interfering with your mail system, use
LSB_MAILSIZE_LIMIT to set the maximum size in KB of the email containing
the job information. Specify a positive integer.

If the size of the job output email exceeds LSB_MAILSIZE_LIMIT, the
output is saved to a file under JOB_SPOOL_DIR or to the default job
output directory if JOB_SPOOL_DIR is undefined. The email informs users
of where the job output is located.

If the **-o** option of **bsub** is used, the size of the job output is
not checked against LSB_MAILSIZE_LIMIT.

If you use a custom mail program specified by the LSB_MAILPROG parameter
that can use the LSB_MAILSIZE environment variable, it is not necessary
to configure LSB_MAILSIZE_LIMIT.

## Default

By default, LSB_MAILSIZE_LIMIT is not enabled. No limit is set on size
of batch job output email.

## See Also

LSB_MAILPROG, LSB_MAILTO

# LSB_MAILTO

## Syntax

**LSB_MAILTO=***mail_account* .SS Description

Lava Batch sends electronic mail to users when their jobs complete or
have errors, and to the Lava administrator in the case of critical
errors in the Lava Batch system. The default is to send mail to the user
who submitted the job, on the host on which the daemon is running; this
assumes that your electronic mail system forwards messages to a central
mailbox.

The LSB_MAILTO parameter changes the mailing address used by Lava Batch.
LSB_MAILTO is a format string that is used to build the mailing address.

Common formats are:

> · !U\--Mail is sent to the submitting user\'s account name on the
> local host. The substring !U, if found, is replaced with the user\'s
> account name.
>
> · !U@company_name.com\--Mail is sent to *user*@*company_name*.com on
> the mail server specified by LSB_MAILSERVER.
>
> · !U@!H\--Mail is sent to *user*@*submission_hostname*. The substring
> !H is replaced with the name of the submission host.

All other characters (including any other \`!\') are copied exactly.

If this parameter is modified, the Lava administrator must restart the
SBD daemons on all hosts to retrieve the new value.

## Default

!U

## See Also

LSB_MAILPROG, LSB_MAILSIZE_LIMIT

# LSB_MIG2PEND

## Syntax

**LSB_MIG2PEND=0 **\| **1** .SS Description

Applies only to migrating jobs.

If 1, requeues migrating jobs intead of restarting or rerunning them on
the next available host. Requeues the jobs in the PEND state, in order
of the original submission time and job priority, unless
LSB_REQUEUE_TO_BOTTOM is also defined.

Undefined

## See Also

LSB_REQUEUE_TO_BOTTOM

# LSB_MBD_PORT

See LSF_LIM_PORT, LSF_RES_PORT, LSB_MBD_PORT, LSB_SBD_PORT.

# LSB_MEMLIMIT_ENFORCE

## Syntax

**LSB_MEMLIMIT_ENFORCE=y** \| **n** .SS Description

Specify y to enable Lava memory limit enforcement.

If enabled, Lava sends a signal to kill all processes that exceed queue-
level memory limits set by MEMLIMIT in lsb.queues or job-level memory
limits specified by **bsub -M ***mem_limit*.

Otherwise, Lava passes memory limit enforcement to the OS. UNIX
operating systems that support RUSAGE_RSS for setrlimit() can apply the
memory limit to each process.

## Default

Not defined. Lava passes memory limit enforcement to the OS.

## See Also

lsb.queues

# LSB_MOD_ALL_JOBS

## Syntax

**LSB_MOD_ALL_JOBS=y** \| **Y** .SS Description

If set, enables **bmod** to modify resource limits and location of job
output files for running jobs.

After a job has been dispatched, the following modifications can be
made:

> · CPU limit (**-c **\[*hour***:**\]*minute*\[**/***host_name* \|
> **/***host_model*\] \| **-cn**)
>
> · Memory limit (**-M** *mem_limit* \| **-Mn**)
>
> · Run limit (**-W** *run_limit*\[**/***host_name* \|
> **/***host_model*\] \| **-Wn**)
>
> · Standard output file name (**-o** *output_file* \| **-on**)
>
> · Standard error file name (**-e** *error_file* \| **-en**)
>
> · Rerunnable jobs (**-r** \| **-rn**)

To modify the CPU limit or the memory limit of running jobs, the
parameters LSB_JOB_CPULIMIT=Y and LSB_JOB_MEMLIMIT=Y must be defined in
lsf.conf.

## Default

Undefined

## See Also

LSB_JOB_CPULIMIT, LSB_JOB_MEMLIMIT

# LSB_REQUEUE_TO_BOTTOM

## Syntax

**LSB_REQUEUE_TO_BOTTOM=0** \| **1** .SS Description

Optional. If 1, requeues automatically requeued jobs to the bottom of
the queue instead of to the top. Also requeues migrating jobs to the
bottom of the queue if LSB_MIG2PEND is defined.

## Default

Undefined

## See Also

REQUEUE_EXIT_VALUES, LSB_MIG2PEND

# LSB_SBD_PORT

See LSF_LIM_PORT, LSF_RES_PORT, LSB_MBD_PORT, LSB_SBD_PORT.

# LSB_SET_TMPDIR

## Syntax

**LSB_SET_TMPDIR=**\[**y**\|**n**\]

If y, Lava sets the TMPDIR environment variable, overwriting the current
value with /*tmp*/*job_ID*.

## Default

n

# LSB_SHAREDIR

## Syntax

**LSB_SHAREDIR=***dir* .SS Description

Directory in which Lava Batch maintains job history and accounting logs
for each cluster. These files are necessary for correct operation of the
system. Like the organization under LSB_CONFDIR, there is one
subdirectory for each cluster.

The LSB_SHAREDIR directory must be owned by the Lava administrator. It
must be accessible from all hosts that can potentially become the master
host, and must allow read and write access from the Lava master host.

The LSB_SHAREDIR directory typically resides on a reliable file server.

## Default

LSF_INDEP/work

## See Also

LSB_LOCALDIR

# LSB_SIGSTOP

## Syntax

**LSB_SIGSTOP=***signal_name* \| *signal_value* .SS Description

Specifies the signal sent by the SUSPEND action in Lava. You can specify
a signal name or a number.

If LSB_SIGSTOP is set to anything other than SIGSTOP, the SIGTSTP signal
that is normally sent by the SUSPEND action is not sent.

If this parameter is undefined, by default the SUSPEND action in Lava
sends the following signals to a job:

> · Parallel or interactive jobs\--1. SIGTSTP is sent first to allow
> user programs to catch the signal and clean up. 2. SIGSTOP is sent 10
> seconds after SIGTSTP. SIGSTOP cannot be caught by user programs.
>
> · Other jobs\--SIGSTOP is sent. SIGSTOP cannot be caught by user
> programs.

The same set of signals is not supported on all UNIX systems. To display
a list of the symbolic names of the signals (without the SIG prefix)
supported on your system, use the **kill -l** command.

## Example

LSB_SIGSTOP=SIGKILL

In this example, the SUSPEND action sends the three default signals sent
by the TERMINATE action (SIGINT, SIGTERM, and SIGKILL) 10 seconds apart.

## Default

Undefined. Default SUSPEND action in Lava is sent.

# LSB_SHORT_HOSTLIST

## Syntax

**LSB_SHORT_HOSTLIST=1** .SS Description

Displays an abbreviated list of hosts in **bjobs** and **bhist** for a
parallel job where multiple processes of a job are running on a host.
Multiple processes are displayed in the following format:

*processes*\*hostA

For example, if a parallel job is running 5 processes on hostA, the
information is displayed in the following manner:

5\*hostA

Setting this parameter may improve MBD restart performance and
accelerate event replay.

## Default

Undefined

# LSB_TIME_CMD

## Syntax

**LSB_TIME_CMD=***timimg_level* .SS Description

The timing level for checking how long batch commands run.

Time usage is logged in milliseconds; specify a positive integer.

Example: LSB_TIME_CMD=1

## Default

Undefined

## See Also

LSB_TIME_MBD, LSB_TIME_SBD, LSF_TIME_LIM, LSF_TIME_RES

# LSB_TIME_MBD

## Syntax

**LSB_TIME_MBD=***timing_level* .SS Description

The timing level for checking how long MBD routines run.

Time usage is logged in milliseconds; specify a positive integer.

Example: LSB_TIME_MBD=1

## Default

Undefined

## See Also

LSB_TIME_CMD, LSB_TIME_SBD, LSF_TIME_LIM, LSF_TIME_RES

# LSB_TIME_SBD

## Syntax

**LSB_TIME_SBD=***timing_level* .SS Description

The timing level for checking how long SBD routines run.

Time usage is logged in milliseconds; specify a positive integer.

Example: LSB_TIME_SBD=1

## Default

Undefined

## See Also

LSB_TIME_CMD, LSB_TIME_MBD, LSF_TIME_LIM, LSF_TIME_RES

# LSB_UTMP

## Syntax

**LSB_UTMP=y** \| **Y** .SS Description

If set, enables registration of user and account information for
interactive batch jobs submitted with **bsub -Ip** or **bsub -Is**. To
disable utmp file registration, set LSB_UTMP to any value other than y
or Y; for example, LSB_UTMP=N.

Lava registers interactive batch jobs the job by adding a entries to the
utmp file on the execution host when the job starts. After the job
finishes, Lava removes the entries for the job from the utmp file.

## Default

Undefined

LSF_SERVERDIR, where the default for LSF_SERVERDIR is
/usr/share/lsf/etc.

## See Also

LSF_SERVERDIR

# LSF_AM_OPTIONS

## Syntax

**LSF_AM_OPTIONS=AMFIRST** \| **AMNEVER** .SS Description

Determines the order of file path resolution when setting the user\'s
home directory.

This variable is rarely used but sometimes Lava does not properly change
the directory to the user\'s home directory when the user\'s home
directory is automounted. Setting LSF_AM_OPTIONS forces the Batch system
to change directory to \$HOME before attempting to automount the user\'s
home.

When this parameter is undefined or set to AMFIRST, Lava:

> · Sets the user\'s \$HOME directory from the automount path. If it
> cannot do so, Lava sets the user\'s \$HOME directory from the passwd
> file.

When this parameter is set to AMNEVER, Lava:

> · Never uses automount to set the path to the user\'s home. Lava sets
> the user\'s \$HOME directory directly from the passwd file.

## Valid Values

The two values are AMFIRST and AMNEVER

## Default

Undefined; same as AMFIRST

# LSF_API_CONNTIMEOUT

## Syntax

**LSF_API_CONNTIMEOUT=***seconds* .SS Description

Timeout when connecting to LIM.

## Default

5

## See Also

LSF_API_RECVTIMEOUT

# LSF_API_RECVTIMEOUT

## Syntax

**LSF_API_RECVTIMEOUT=***seconds*

## Description

Timeout when receiving a reply from LIM.

## Default

20

## See Also

LSF_API_CONNTIMEOUT

# LSF_AUTH

## Syntax

**LSF_AUTH=eauth** \| **setuid** .SS Description

Optional. Determines the type of authentication used by Lava.

By default, external user authentication is used, and LSF_AUTH is
defined to be eauth.

If this parameter is changed, all Lava daemons must be shut down and
restarted by running **lsf_daemons start** on each of the Lava server
hosts so that the daemons will use the new authentication method.

If LSF_AUTH is not defined, RES will only accept requests from
privileged ports. When Lava uses privileged ports for user
authentication, Lava commands must be installed setuid to root to
operate correctly. If the Lava commands are installed in an NFS mounted
shared file system, the file system must be mounted with setuid
execution allowed (that is, without the nosuid option). See the man page
for **mount** for more details.

## Valid Values

> · eauth

> For site-specific external authentication.

> · setuid

> For privileged ports (**setuid**) authentication. This is the
> mechanism most UNIX remote utilities use. The Lava commands must be
> installed as **setuid** programs and owned by root.

## Default

eauth

# LSF_AUTH_DAEMONS

## Syntax

**LSF_AUTH_DAEMONS=***any_value* .SS Description

Enables daemon authentication, as long as LSF_AUTH in lsf.conf is set to
**eauth**. Daemons will call **eauth** to authenticate each other.

## Default

Undefined

# LSF_BINDIR

## Syntax

**LSF_BINDIR=***dir*

## Description

Directory in which all Lava user commands are installed.

## Default

LSF_MACHDEP/bin

# LSF_CMD_LOGDIR

## Syntax

**LSF_CMD_LOGDIR=***path* .SS Description

The path to the log files used for debugging Lava commands.

This parameter can also be set from the command line.

## Default

/tmp

## See Also

LSB_DEBUG_CMD, LSB_CMD_LOGDIR

# LSF_CONF_RETRY_INT

## Syntax

**LSF_CONF_RETRY_INT=***seconds* .SS Description

The number of seconds to wait between unsuccessful attempts at opening a
configuration file (only valid for LIM). This allows LIM to tolerate
temporary access failures.

## Default

30

## See Also

LSF_CONF_RETRY_MAX

# LSF_CONF_RETRY_MAX

## Syntax

**LSF_CONF_RETRY_MAX=***integer* .SS Description

The maximum number of unsuccessful attempts at opening a configuration
file (only valid for LIM). This allows LIM to tolerate temporary access
failures.

## Default

0

## See Also

LSF_CONF_RETRY_INT

# LSF_CONFDIR

## Syntax

**LSF_CONFDIR=***dir*

## Description

Directory in which all Lava configuration files are installed. These
files are shared throughout the system and should be readable from any
host. This directory can contain configuration files for more than one
cluster.

The files in the LSF_CONFDIR directory must be owned by the primary Lava
administrator, and readable by all Lava server hosts.

## Default

LSF_INDEP/conf

## See Also

LSB_CONFDIR

# LSF_DEBUG_LIM

## Syntax

**LSF_DEBUG_LIM=***log_class* .SS Description

Sets the log class for debugging LIM.

Specifies the log class filtering that will be applied to LIM. Only
messages belonging to the specified log class are recorded.

The LSF_DEBUG_LIM (which sets the log class) is used in combination with
LSF_LOG_MASK (which sets the log level). For example:

LSF_LOG_MASK=LOG_DEBUG\
LSF_DEBUG_LIM=LC_TRACE

You need to restart the daemons after setting LSF_DEBUG_LIM for your
changes to take effect.

If you use the command lsadmin limdebug to temporarily change this
parameter without changing lsf.conf, you will not need to restart the
daemons.

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

To specify multiple log classes, use a space-separated list enclosed in
quotation marks. For example:

LSF_DEBUG_LIM=\"LC_TRACE LC_EXEC\"

This parameter can also be defined from the command line.

## Valid Values

Valid log classes are:

> · LC_AUTH - Log authentication messages
>
> · LC_CHKPNT - log checkpointing messages
>
> · LC_COMM - Log communication messages
>
> · LC_EXEC - Log significant steps for job execution
>
> · LC_FILE - Log file transfer messages
>
> · LC_HANG - Mark where a program might hang
>
> · LC_PIM - Log PIM messages
>
> · LC_SIGNAL - Log messages pertaining to signals
>
> · LC_TRACE - Log significant program walk steps
>
> · LC_XDR - Log everything transferred by XDR

## Default

Undefined

## See Also

LSF_DEBUG_RES, LSF_LOG_MASK, LSF_LOGDIR

# LSF_DEBUG_RES

## Syntax

**LSF_DEBUG_RES=***log_class* .SS Description

Sets the log class for debugging RES.

Specifies the log class filtering that will be applied to RES. Only
messages belonging to the specified log class are recorded.

LSF_DEBUG_RES (which sets the log class) is used in combination with
LSF_LOG_MASK (which sets the log level). For example:

LSF_LOG_MASK=LOG_DEBUG\
LSF_DEBUG_RES=LC_TRACE

To specify multiple log classes, use a space-separated list enclosed in
quotation marks. For example:

LSF_DEBUG_RES=\"LC_TRACE LC_EXEC\"

You need to restart the daemons after setting LSF_DEBUG_RES for your
changes to take effect.

If you use the command lsadmin resdebug to temporarily change this
parameter without changing lsf.conf, you will not need to restart the
daemons.

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

This parameter can also be defined from the command line.

## Valid Values

For a list of valid log classes see LSF_DEBUG_LIM

## Default

Undefined

## See Also

LSF_DEBUG_LIM, LSF_LOG_MASK, LSF_LOGDIR

# LSF_DEFAULT_INSTALL

## Syntax

**LSF_DEFAULT_INSTALL=y**\|**n** .SS Description

This parameter is set to y if the default installation is used; set to n
otherwise.

## Valid Values

y \| n

# LSF_ENVDIR

## Syntax

**LSF_ENVDIR=***dir* .SS Description

Directory containing the lsf.conf file.

By default, lsf.conf is installed by creating a shared copy in
LSF_CONFDIR and adding a symbolic link from /etc/lsf.conf to the shared
copy. If LSF_ENVDIR is set, the symbolic link is installed in
LSF_ENVDIR/lsf.conf.

The lsf.conf file is a global environment configuration file for all
Lava services and applications. The Lava default installation places the
file in LSF_CONFDIR.

## Default

/etc

# LSF_INCLUDEDIR

## Syntax

**LSF_INCLUDEDIR=***dir* .SS Description

Directory under which the Lava API header files lsf.h and lsbatch.h are
installed.

## Default

LSF_INDEP/include

## See Also

LSF_INDEP

# LSF_INDEP

## Syntax

**LSF_INDEP=***dir* .SS Description

Specifies the default top-level directory for all machine-independent
Lava files.

This includes man pages, configuration files, working directories, and
examples. For example, defining LSF_INDEP as /usr/share/lsf/mnt places
man pages in /usr/share/lsf/mnt/man, configuration files in
/usr/share/lsf/mnt/conf, and so on.

The files in LSF_INDEP can be shared by all machines in the cluster.

As shown in the following list, LSF_INDEP is incorporated into other
Lava environment variables.

> · LSB_SHAREDIR=\$LSF_INDEP/work
>
> · LSF_CONFDIR=\$LSF_INDEP/conf
>
> · LSF_INCLUDEDIR=\$LSF_INDEP/include
>
> · LSF_MANDIR=\$LSF_INDEP/man

## Default

/usr/share/lsf/mnt

## See Also

LSF_MACHDEP, LSB_SHAREDIR, LSF_CONFDIR, LSF_INCLUDEDIR, LSF_MANDIR

# LSF_INTERACTIVE_STDERR

## Syntax

**LSF_INTERACTIVE_STDERR=y** \| **n** .SS Description

Separates stderr from stdout for interactive tasks and interactive batch
jobs.

This parameter can also be enabled or disabled as an environment
variable.

## Warning

## If you enable this parameter globally in lsf.conf, check any custom scripts that manipulate stderr and stdout.

When this parameter is undefined or set to n, the following are written
to stdout on the submission host for interactive tasks and interactive
batch jobs:

> · Job standard output messages
>
> · Job standard error messages

The following are written to stderr on the submission host for
interactive tasks and interactive batch jobs:

> · Lava messages
>
> · NIOS standard messages
>
> · NIOS debug messages (if LSF_NIOS_DEBUG=1 in lsf.conf)

When this parameter is set to y, the following are written to stdout on
the submission host for interactive tasks and interactive batch jobs:

> · Job standard output messages

The following are written to stderr on the submission host:

> · Job standard error messages
>
> · Lava messages
>
> · NIOS standard messages
>
> · NIOS debug messages (if LSF_NIOS_DEBUG=1 in lsf.conf)

## Default

Undefined

## Notes

When this parameter is set, the change affects interactive tasks and
interactive batch jobs run with the following commands:

> · **bsub -I** .HP 2 · **bsub -Ip** .HP 2 · **bsub -Is** .HP 2

## Limitations

> · Pseudo-terminal\--Do not use this parameter if your application
> depends on stderr as a terminal. This is because Lava must use a
> non-pseudo-terminal connection to separate stderr from stdout.
>
> · Synchronization\--Do not use this parameter if you depend on
> messages in stderr and stdout to be synchronized and jobs in your
> environment are continuously submitted. A continuous stream of
> messages causes stderr and stdout to not be synchronized. This can be
> emphasized with parallel jobs. This situation is similar to that of
> rsh.
>
> · NIOS standard and debug messages\--NIOS standard messages, and debug
> messages (when LSF_NIOS_DEBUG=1 in lsf.conf or as an environment
> variable) are written to stderr. NIOS standard messages are in the
> format \<\<*message*\>\>, which makes it easier to remove them if you
> wish. To redirect NIOS debug messages to a file, define LSF_CMD_LOGDIR
> in lsf.conf or as an environment variable.

## See Also

LSF_NIOS_DEBUG, LSF_CMD_LOGDIR

# LSF_LIBDIR

## Syntax

**LSF_LIBDIR=***dir* .SS Description

Specifies the directory in which the Lava libraries are installed.
Library files are shared by all hosts of the same type.

## Default

LSF_MACHDEP/lib

# LSF_LIM_DEBUG

## Syntax

**LSF_LIM_DEBUG=1** \| **2** .SS Description

Sets Lava to debug mode.

If LSF_LIM_DEBUG is defined, LIM operates in single user mode. No
security checking is performed, so LIM should not run as root.

LIM will not look in the services database for the LIM service port
number. Instead, it uses port number 36000 unless LSF_LIM_PORT has been
defined.

Specify 1 for this parameter unless you are testing Lava.

## Valid Values

> · LSF_LIM_DEBUG=1

> LIM runs in the background with no associated control terminal.

> · LSF_LIM_DEBUG=2

> LIM runs in the foreground and prints error messages to tty.

## Default

Undefined

## See Also

LSF_RES_DEBUG

# LSF_LIM_PORT, LSF_RES_PORT, LSB_MBD_PORT, LSB_SBD_PORT

## Syntax

Example: **LSF_LIM_PORT=***port_number* .SS Description

TCP service ports to use for communication with the Lava daemons.

If port parameters are undefined, Lava obtains the port numbers by
looking up the Lava service names in the /etc/services file or the NIS
(UNIX). If it is not possible to modify the services database, you can
define these port parameters to set the port numbers.

With careful use of these settings along with the LSF_ENVDIR and PATH
environment variables, it is possible to run two versions of the Lava
software on a host, selecting between the versions by setting the PATH
environment variable to include the correct version of the commands and
the LSF_ENVDIR environment variable to point to the directory containing
the appropriate lsf.conf file.

## Default

Default port number values for Linux are:

> · LSF_LIM_PORT=6879
>
> · LSF_RES_PORT=6878
>
> · LSB_MBD_PORT=6881
>
> · LSB_SBD_PORT=6882

## Syntax

**LSF_LOG_MASK=***message_log_level* .SS Description

Logging level of messages for Lava daemons.

This is similar to syslog. All messages logged at the specified level or
higher are recorded; lower level messages are discarded. The
LSF_LOG_MASK value can be any log priority symbol that is defined in
syslog.h (see syslog(8)).

The log levels in order from highest to lowest are:

> · LOG_EMERG
>
> · LOG_ALERT
>
> · LOG_CRIT
>
> · LOG_ERR
>
> · LOG_WARNING
>
> · LOG_NOTICE
>
> · LOG_INFO
>
> · LOG_DEBUG
>
> · LOG_DEBUG1
>
> · LOG_DEBUG2
>
> · LOG_DEBUG3

The most important Lava log messages are at the LOG_ERR or LOG_WARNING
level. Messages at the LOG_INFO and LOG_DEBUG level are only useful for
debugging.

Although message log level implements similar functionalities to Linux
syslog, there is no dependency on syslog. It works even if messages are
being logged to files instead of syslog.

Lava logs error messages in different levels so that you can choose to
log all messages, or only log messages that are deemed critical. The
level specified by LSF_LOG_MASK determines which messages are recorded
and which are discarded. All messages logged at the specified level or
higher are recorded, while lower level messages are discarded.

For debugging purposes, the level LOG_DEBUG contains the fewest number
of debugging messages and is used only for very basic debugging. The
level LOG_DEBUG3 records all debugging messages, and is not often used.
Most debugging is done at the level LOG_DEBUG2.

The daemons log to the syslog facility unless LSF_LOGDIR is defined.

## Default

LOG_WARNING

## See Also

LSF_DEBUG_LIM, LSF_DEBUG_RES, LSB_DEBUG_MBD, LSB_DEBUG_SBD,
LSB_DEBUG_CMD, LSB_DEBUG_CMD, LSF_CMD_LOGDIR

# LSF_LOGDIR

## Syntax

**LSF_LOGDIR=***dir* .SS Description

This is an optional directory parameter

Error messages from all servers are logged into files in this directory.
If a server is unable to write in this directory, the error logs are
created in /tmp on UNIX.

If LSF_LOGDIR is not defined, then syslog is used to log everything to
the system log using the LOG_DAEMON facility. The syslog facility is
available by default on most UNIX systems. The /etc/syslog.conf file
controls the way messages are logged and the files they are logged to.
See the man pages for the syslogd daemon and the syslog function for
more information.

To effectively use debugging, set LSF_LOGDIR to a directory such as
/tmp. This can be done in your own environment from the shell or in
lsf.conf.

## Default

On UNIX, if undefined, log messages go to syslog.

## See Also

LSF_LOG_MASK

## Files

> · lim.log.*host_name* .HP 2 · res.log.*host_name* .HP 2 ·
> sbatchd.log.*host_name* .HP 2
>
> · mbatchd.log.*host_name* .HP 2 · pim.log.*host_name* .RE

# LSF_MACHDEP

## Syntax

**LSF_MACHDEP=***dir* .SS Description

Specifies the directory in which machine-dependent files are installed.
These files cannot be shared across different types of machines.

In clusters with a single host type, LSF_MACHDEP is usually the same as
LSF_INDEP. The machine dependent files are the user commands, daemons,
and libraries. You should not need to modify this parameter.

As shown in the following list, LSF_MACHDEP is incorporated into other
Lava variables.

> · LSF_BINDIR=\$LSF_MACHDEP/bin
>
> · LSF_LIBDIR=\$LSF_MACHDEP/lib
>
> · LSF_SERVERDIR=\$LSF_MACHDEP/etc
>
> · XLSF_UIDDIR=\$LSF_MACHDEP/lib/uid

## Default

/usr/share/lsf

## See Also

LSF_INDEP

# LSF_MANDIR

## Syntax

**LSF_MANDIR=***dir* .SS Description

Directory under which all man pages are installed.

The man pages are placed in the man1, man3, man5, and man8
subdirectories of the LSF_MANDIR directory. This is created by the Lava
installation process, and you should not need to modify this parameter.

Man pages are installed in a format suitable for BSD-style **man**
commands.

For most versions of UNIX, you should add the directory LSF_MANDIR to
your MANPATH environment variable. If your system has a man command that
does not understand MANPATH, you should either install the man pages in
the /usr/man directory or get one of the freely available man programs.

## Default

LSF_INDEP/man

# LSF_MASTER_LIST

## Syntax

**LSF_MASTER_LIST=\"***host_name \...***\"** .SS Description

Optional. Defines a list of hosts that are candidates to become the
master host for the cluster.

Listed hosts must be defined in lsf.cluster.*cluster_name*.

Host names are separated by spaces.

Whenever you reconfigure, only master LIM candidates read lsf.shared and
lsf.cluster.*cluster_name* to get updated information. The elected
master LIM sends configuration information to slave LIMs.

## Default

Undefined

# LSF_MISC

## Syntax

**LSF_MISC=***dir* .SS Description

Directory in which miscellaneous machine independent files, such as Lava
example source programs and scripts, are installed.

## Default

LSF_CONFDIR/misc

# LSF_NIOS_DEBUG

## Syntax

**LSF_NIOS_DEBUG=1** .SS Description

Turns on NIOS debugging for interactive jobs.

If LSF_NIOS_DEBUG=1, NIOS debug messages are written to standard error.

This parameter can also be defined as an environment variable.

When LSF_NIOS_DEBUG and LSF_CMD_LOGDIR are defined, NIOS debug messages
are logged in nios.log.*host_name*. in the location specified by
LSF_CMD_LOGDIR.

If LSF_NIOS_DEBUG is defined, and the directory defined by
LSF_CMD_LOGDIR is inaccessible, NIOS debug messages are logged to
/tmp/nios.log.*host_name* instead of stderr.

## Default

Undefined

## See Also

LSF_CMD_LOGDIR

# LSF_NIOS_JOBSTATUS_INTERVAL

## Syntax

**LSF_NIOS_JOBSTATUS_INTERVAL=***minutes* .SS Description

Applies only to interactive batch jobs.

Time interval at which NIOS polls MBD to check if a job is still
running. Used to retrieve a job\'s exit status in the case of an
abnormal exit of NIOS, due to a network failure for example.

Use this parameter if you run interactive jobs and you have scripts that
depend on an exit code being returned.

When this parameter is undefined and a network connection is lost, MBD
cannot communicate with NIOS and the return code of a job is not
retrieved.

When this parameter is defined, before exiting, NIOS polls MBD on the
interval defined by LSF_NIOS_JOBSTATUS_INTERVAL to check if a job is
still running. NIOS continues to poll MBD until it receives an exit code
or MBD responds that the job does not exist (if the job has already been
cleaned from memory for example).

If an exit code cannot be retrieved, NIOS generates an error message and
the code -11.

## Valid Values

Any integer greater than zero

## Default

Undefined

## Notes

Set this parameter to large intervals such as 15 minutes or more so that
performance is not negatively affected if interactive jobs are pending
for too long. NIOS always calls MBD on the defined interval to confirm
that a job is still pending and this may add load to MBD.

## Product

Lava Batch

## See Also

Environment variable LSF_NIOS_PEND_TIMEOUT

# LSF_NIOS_RES_HEARTBEAT

## Syntax

**LSF_NIOS_RES_HEARTBEAT=***minutes* .SS Description

Applies only to interactive non-parallel batch jobs.

Defines how long NIOS waits before sending a message to RES to determine
if the connection is still open.

Use this parameter to ensure NIOS exits when a network failure occurs
instead of waiting indefinitely for notification that a job has been
completed. When a network connection is lost, RES cannot communicate
with NIOS and as a result, NIOS does not exit.

When this parameter is defined, if there has been no communication
between RES and NIOS for the defined period of time, NIOS sends a
message to RES to see if the connection is still open. If the connection
is no longer available, NIOS exits.

## Valid Values

Any integer greater than zero

## Default

Undefined

## Notes

The time you set this parameter to depends how long you want to allow
NIOS to wait before exiting. Typically, it can be a number of hours or
days. Too low a number may add load to the system.

## Product

Lava Base, Lava Batch

# 

# LSF_PIM_INFODIR

## Syntax

**LSF_PIM_INFODIR=***path* .SS Description

The path to where PIM writes the pim.info.*host_name* file.

Specifies the path to where the process information is stored. The
process information resides in the file pim.info.*host_name*. The PIM
also reads this file when it starts up so that it can accumulate the
resource usage of dead processes for existing process groups.

## Default

Undefined. If undefined, the system uses /tmp.

# LSF_PIM_SLEEPTIME

## Syntax

**LSF_PIM_SLEEPTIME=***seconds* .SS Description

The reporting period for PIM.

PIM updates the process information every 15 minutes unless an
application queries this information. If an application requests the
information, PIM will update the process information every
LSF_PIM_SLEEPTIME seconds. If the information is not queried by any
application for more than 5 minutes, the PIM will revert back to the 15
minute update period.

## Default

15

# LSF_RES_ACCT

## Syntax

**LSF_RES_ACCT=***milliseconds *\| **0** .SS Description

If this parameter is defined, RES will log information for completed and
failed tasks by default (see **lsf.acct**(5)).

The value for LSF_RES_ACCT is specified in terms of consumed CPU time
(milliseconds). Only tasks that have consumed more than the specified
CPU time will be logged.

If this parameter is defined as LSF_RES_ACCT=0, then all tasks will be
logged.

For those tasks that consume the specified amount of CPU time, RES
generates a record and appends the record to the task log file
lsf.acct.*host_name*. This file is located in the LSF_RES_ACCTDIR
directory.

If this parameter is not defined, the Lava administrator must use the
**lsadmin** command (see **lsadmin**(8)) to turn task logging on after
RES has started up.

## Default

Undefined

## See Also

LSF_RES_ACCTDIR

# LSF_RES_ACCTDIR

## Syntax

**LSF_RES_ACCTDIR=***dir* .SS Description

The directory in which the RES task log file lsf.acct.*host_name* is
stored.

If LSF_RES_ACCTDIR is not defined, the log file is stored in the /tmp
directory.

## Default

(UNIX)/tmp

## See Also

LSF_RES_ACCT

# LSF_RES_DEBUG

## Syntax

**LSF_RES_DEBUG=1*** \| ***2** .SS Description

Sets RES to debug mode.

If LSF_RES_DEBUG is defined, the Remote Execution Server (RES) will
operate in single user mode. No security checking is performed, so RES
should not run as root. RES will not look in the services database for
the RES service port number. Instead, it uses port number 36002 unless
LSF_RES_PORT has been defined.

Specify 1 for this parameter unless you are testing RES.

## Valid Values

> · LSF_RES_DEBUG=1

> RES runs in the background with no associated control terminal.

> · LSF_RES_DEBUG=2

> RES runs in the foreground and prints error messages to tty.

## Default

Undefined

## See Also

LSF_LIM_DEBUG

# LSF_RES_PLUGINDIR

## Syntax

**LSF_RES_PLUGINDIR=***path* .SS Description

The path to lsbresvcl.so. Used only with SUN HPC.

## Default

\$LSF_LIBDIR

## See Also

# LSF_RES_PORT

See LSF_LIM_PORT, LSF_RES_PORT, LSB_MBD_PORT, LSB_SBD_PORT.

# LSF_RES_RLIMIT_UNLIM

## Syntax

**LSF_RES_RLIMIT_UNLIM=cpu** \| **fsize** \| **data** \| **stack** \|
**core** \| **vmem** .SS Description

(Lava Base only) By default, RES sets the hard limits for a remote task
to be the same as the hard limits of the local process. This parameter
specifies those hard limits which are to be set to unlimited, instead of
inheriting those of the local process.

Valid values are cpu, fsize, data, stack, core, and vmem, for cpu, file
size, data size, stack, core size, and virtual memory limits,
respectively.

## Example

The following example sets the cpu, core size, and stack hard limits to
be unlimited for all remote tasks:

LSF_RES_RLIMIT_UNLIM=\"cpu core stack\"

## Default

Undefined

## See Also

# LSF_RES_TIMEOUT

## Syntax

**LSF_RES_TIMEOUT=***seconds*

## Description

Timeout when communicating with RES.

## Default

15

# LSF_SERVER_HOSTS

## Syntax

**LSF_SERVER_HOSTS=\"***host_name* \...**\"** .SS Description

Defines one or more Lava server hosts that the application should
contact to find a Load Information Manager (LIM). This is used on client
hosts on which no LIM is running on the local host. The Lava server
hosts are hosts that run Lava daemons and provide loading-sharing
services. Client hosts are hosts that only run Lava commands or
applications but do not provide services to any hosts.

If LSF_SERVER_HOSTS is not defined, the application tries to contact the
LIM on the local host.

The host names in LSF_SERVER_HOSTS must be enclosed in quotes and
separated by white space. For example:

LSF_SERVER_HOSTS=\"hostA hostD hostB\"

## Default

Undefined

# LSF_SERVERDIR

## Syntax

**LSF_SERVERDIR=***dir* .SS Description

Directory in which all server binaries and shell scripts are installed.

These include lim, res, nios, sbatchd, mbatchd. If you use elim, eauth,
eexec, esub, etc, they are also installed in this directory.

## Default

LSF_MACHDEP/etc

## See Also

LSB_ECHKPNT_METHOD_DIR

# LSF_STRIP_DOMAIN

## Syntax

# LSF_TIME_CMD

## Syntax

**LSF_TIME_CMD=***timimg_level* .SS Description

The timing level for checking how long Lava commands run. Time usage is
logged in milliseconds; specify a positive integer.

## Default

Undefined

## See Also

LSB_TIME_MBD, LSB_TIME_SBD, LSB_TIME_CMD, LSF_TIME_LIM, LSF_TIME_RES

# LSF_TIME_LIM

## Syntax

**LSF_TIME_LIM=***timing_level* .SS Description

The timing level for checking how long LIM routines run.

Time usage is logged in milliseconds; specify a positive integer.

## Default

Undefined

## See Also

LSB_TIME_CMD, LSB_TIME_MBD, LSB_TIME_SBD, LSF_TIME_RES

# LSF_TIME_RES

## Syntax

**LSF_TIME_RES=***timing_level* .SS Description

The timing level for checking how long RES routines run.

Time usage is logged in milliseconds; specify a positive integer.

## Default

Undefined

## See Also

LSB_TIME_CMD, LSB_TIME_MBD, LSB_TIME_SBD, LSF_TIME_LIM

# LSF_TMPDIR

## Syntax

**LSF_TMPDIR=***dir* .SS Description

Specifies the path and directory for temporary job output.

When LSF_TMPDIR is defined in lsf.conf, Lava creates a temporary
directory under the directory specified by LSF_TMPDIR on the execution
host when a job is started and sets the temporary directory environment
variable for the job.

When LSF_TMPDIR is defined as an environment variable, it overrides the
LSF_TMPDIR specified in lsf.conf. Lava removes the temporary directory
and the files that it contains when the job completes.

The name of the temporary directory has the following format:

\$LSF_TMPDIR/job_ID.tmpdir

On UNIX, the directory has the permission 0700.

After adding LSF_TMPDIR to lsf.conf, use **badmin hrestart all** to
reconfigure your cluster.

This parameter can also be specified from the command line.

## Valid Values

Specify any valid path up to a maximum length of 256 characters. The 256
character maximum path length includes the temporary directories and
files that Lava Batch creates as jobs run. The path that you specify for
LSF_TMPDIR should be as short as possible to avoid exceeding this limit.

On UNIX, specify an absolute path. For example:

LSF_TMPDIR=/usr/share/lsf_tmp

## Default

By default, LSF_TMPDIR is not enabled. If LSF_TMPDIR is not specified
either in the environment or in lsf.conf, this parameter is defined as
follows:

> · On UNIX: \$TMPDIR or /tmp
