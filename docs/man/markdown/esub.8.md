# NAME

esub, eexec - External executables at job submission and execution time

# SYNOPSIS

**LSF_SERVERDIR/esub**

**LSF_SERVERDIR/eexec**

# DESCRIPTION

Administrators can write these external submission (esub) and execution
time (eexec) executables to perform site-specific actions on jobs.

When a job is submitted (see **bsub**(1) and **brestart**(1)), the esub
is executed if it is found in LSF_SERVERDIR. On the execution host,
eexec is run at job startup and completion time, and when checkpointing
is initiated. The environment variable **LS_EEXEC_T** is set to
**START**, **END**, and **CHKPNT**, respectively, to indicate when the
eexec is invoked. If esub needs to pass some data to eexec, esub can
simply write the data to stdout; eexec can get the data via its stdin.
Thus, LSF is effectively implementing the pipe in \"esub \| eexec\".

eexec is executed as the user after the job\'s environment variables
have been set. The parent job process waits for the eexec to complete
before proceeding; thus, the eexec is expected to complete.

The pid of the process which invoked the eexec is stored in the
environment variable **LS_JOBPID**. If the eexec is intended to monitor
the execution of the job, the eexec must fork a child and then have the
parent eexec process exit. While monitoring job execution, the eexec
child should periodically check that the job process is still alive by
testing the **LS_JOBPID** variable. When the job is finished, the eexec
child should exit.

In addition to bsub and brestart, esub is also invoked for bmodify. When
the esub is invoked, the environment variables, **LSB_SUB_ABORT_VALUE**
and **LSB_SUB_PARM_FILE**, are set. If the esub exits with the
**LSB_SUB_ABORT_VALUE** value, the job request will be aborted.
**LSB_SUB_PARM_FILE** gives the name of the temporary file which
contains the job request parameters. See the section **PARAMETER FILE**
for the contents of this file. A typical use of the
**LSB_SUB_ABORT_VALUE** and **LSB_SUB_PARM_FILE** variables is to
disallow certain submission options.

The **LSB_SUB_ABORT_VALUE** and **LSB_SUB_PARM_FILE** variables are not
defined when the esub is invoked in interactive remote execution.

# PARAMETER FILE

The job submission and modification parameter file contains a line of
the form, **option_name=value**, for each job option specified. A
**value** of \"SUB_RESET\" indicates this option is reset by bmodify. If
the esub is a Bourne Shell script, then running \".
\$LSB_SUB_PARM_FILE\" would make these variables available to the esub.
The **option_name**s are:

**LSB_SUB_JOB_NAME**

:   **value** is the specified job name.

**LSB_SUB_QUEUE**

:   **value** is the specified queue name.

**LSB_SUB_IN_FILE**

:   **value** is the specified standard input file name.

**LSB_SUB_OUT_FILE**

:   **value** is the specified standard output file name.

**LSB_SUB_ERR_FILE**

:   **value** is the specified standard error file name.

**LSB_SUB_EXCLUSIVE**

:   **value** of \"Y\" specifies exclusive execution.

**LSB_SUB_NOTIFY_END**

:   **value** of \"Y\" specifies email notification when job ends.

**LSB_SUB_NOTIFY_BEGIN**

:   **value** of \"Y\" specifies email notification when job begins.

**LSB_SUB_USER_GROUP**

:   **value** is the specified user group name.

**LSB_SUB_CHKPNT_PERIOD**

:   **value** is the specified checkpoint period.

**LSB_SUB_CHKPNT_DIR**

:   **value** is the specified checkpoint directory.

**LSB_SUB_RESTART_FORCE**

:   **value** of \"Y\" specifies forced restart job.

**LSB_SUB_RESTART**

:   **value** of \"Y\" specifies a restart job.

**LSB_SUB_RERUNNABLE**

:   **value** of \"Y\" specifies a rerunnable job.

**LSB_SUB_WINDOW_SIG**

:   **value** is the specified window signal number.

**LSB_SUB_HOST_SPEC**

:   **value** is the specified hostspec.

**LSB_SUB_DEPEND_COND**

:   **value** is the specified dependency condition.

**LSB_SUB_RES_REQ**

:   **value** is the specified resource requirement string.

**LSB_SUB_PRE_EXEC**

:   **value** is the specified pre-execution command.

**LSB_SUB_LOGIN_SHELL**

:   **value** is the specified login shell.

**LSB_SUB_MAIL_USER**

:   **value** is the specified user for sending email.

**LSB_SUB_MODIFY**

:   **value** of \"Y\" specifies a modification request.

**LSB_SUB_MODIFY_ONCE**

:   **value** of \"Y\" specifies a modification-once request.

**LSB_SUB_PROJECT_NAME**

:   **value** is the specified project name.

**LSB_SUB_INTERACTIVE**

:   **value** of \"Y\" specifies an interactive job.

**LSB_SUB_PTY**

:   **value** of \"Y\" specifies an interactive job with PTY support.

**LSB_SUB_PTY_SHELL**

:   **value** of \"Y\" specifies an interactive job with PTY shell
    support..

**LSB_SUB_TIME_EVENT**

:   **value** is the time event expression.

**LSB_SUB_HOSTS**

:   **value** is the list of execution host names.

**LSB_SUB_NUM_PROCESSORS**

:   **value** is the minimum number of processors requested.

**LSB_SUB_MAX_NUM_PROCESSORS**

:   **value** is the maximum number of processors requested.

**LSB_SUB_BEGIN_TIME**

:   **value** is the begin time, in seconds since 00:00:00 GMT, Jan. 1,
    1970.

**LSB_SUB_TERM_TIME**

:   **value** is the termination time, in seconds since 00:00:00 GMT,
    Jan. 1, 1970.

**LSB_SUB_OTHER_FILES**

:   **value** is always \"SUB_RESET\" if defined to indicate a bmodify
    is being done to reset the number of file to be transferred.

**LSB_SUB_OTHER_FILES_nn**

:   **nn** is an index number indicating the particular file transfer.
    **value** is the specified file transfer expression. E.g., for
    \'bsub -f \"a \> b\" -f \"c \< d\"\', the following would be
    defined:

```{=html}
<!-- -->
```
    LSB_SUB_OTHER_FILES_0="a > b"
    LSB_SUB_OTHER_FILES_1="c < d"

**LSB_SUB_ALARM**

:   **value** is the specified alarm condition.

**LSB_SUB_RLIMIT_CPU**

:   **value** is the specified cpu limit.

**LSB_SUB_RLIMIT_FSIZE**

:   **value** is the specified file limit.

**LSB_SUB_RLIMIT_DATA**

:   **value** is the specified data size limit.

**LSB_SUB_RLIMIT_STACK**

:   **value** is the specified stack size limit.

**LSB_SUB_RLIMIT_CORE**

:   **value** is the specified core file size limit.

**LSB_SUB_RLIMIT_RSS**

:   **value** is the specified resident size limit.

**LSB_SUB_RLIMIT_RUN**

:   **value** is the specified wall clock run limit.

# SEE ALSO

**lsfbatch**(5), **bsub**(1), **brestart**(1), **bmodify**(1),
**lsfintro**(1) **sbatchd**(8), **res**(8)
