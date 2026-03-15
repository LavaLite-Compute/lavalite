\

# NAME

**lsadmin** - administrative tool for LSF

# SYNOPSIS

\
**lsadmin** *subcommand*\
**lsadmin** \[**-h** \| **-V**\]

# SUBCOMMAND LIST

\
**ckconfig** \[**-v**\]\
**reconfig** \[**-f**\] \[**-v**\]\
**limstartup** \[**-f**\] \[*host_name* \... \| **all**\]\
**limshutdown** \[**-f**\] \[*host_name* \... \| **all**\]\
**limrestart** \[**-v**\] \[**-f**\] \[*host_name* \... \| **all**\]\
**limlock** \[**-l** *time_seconds*\]\
**limunlock** .br **limdebug** \[**-c** *class_name* \...\] \[**-l**
*debug_level*\] \[**-f*** logfile_name*\] \[**-o**\] \[*host_name*\]\
**limtime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\]
\[**-o**\] \[*host_name*\]\
**resstartup** \[**-f**\] \[*host_name* \... \| **all**\]\
**resshutdown** \[**-f**\] \[*host_name* \... \| **all**\]\
**resrestart** \[**-f**\] \[*host_name* \... \| **all**\]\
**reslogon** \[*host_name* \... \| **all**\] \[**-c** *cpu_time*\]\
**reslogoff** \[*host_name* \... \| **all**\]\
**resdebug** \[**-c** *class_name* \...\] \[**-l** *debug_level*\]
\[**-f** *logfile_name*\] \[**-o**\] \[*host_name*\]\
**restime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\]
\[**-o**\] \[*host_name*\]\
**help** \[*subcommand* \...\]\
**quit** .SH DESCRIPTION

This command can only be used by LSF administrators.

lsadmin is a tool that executes privileged commands to control LIM and
RES operations in an LSF cluster.

If no subcommands are supplied for lsadmin, lsadmin prompts for
subcommands from the standard input.

> For subcommands for which multiple host names or host groups can be
> specified, do not enclose the multiple names in quotation marks.

# OPTIONS

*subcommand*

:   

    Executes the specified subcommand. See Usage section.

```{=html}
<!-- -->
```

**-h** 

:   

    Prints command usage to stderr and exits.

```{=html}
<!-- -->
```

**-V** 

:   

    Prints LSF release version and exits.

# USAGE

**ckconfig **\[**-v**\]

:   

    Checks LSF configuration files.

> **-v **

> > Displays detailed messages about configuration file checking.

**reconfig **\[**-f**\] \[**-v**\]

:   

    Restarts LIMs on all hosts in the cluster. You should use reconfig
    after changing configuration files. The configuration files are
    checked before all LIMs in the cluster are restarted. If the
    configuration files are not correct, reconfiguration will not be
    initiated.

> **-f **

> > Disables user interaction and forces LIM to restart on all hosts in
> > the cluster if no fatal errors are found. This option is useful in
> > batch mode.

> **-v**

> > Displays detailed messages about configuration file checking.

**limstartup **\[**-f**\] \[*host_name *\... \|**all**\] 

:   

    Starts up the LIM on the local host if no arguments are specified.

> Starts up LIMs on the specified hosts or on all hosts in the cluster
> if the word all is the only argument provided. You will be asked for
> confirmation.

> **-f**

> > Disables interaction and does not ask for confirmation for starting
> > up LIMs.

**limshutdown **\[**-f**\] \[*host_name *\... \| **all**\]

:   

    Shuts down LIM on the local host if no arguments are supplied.

> Shuts down LIMs on the specified hosts or on all hosts in the cluster
> if the word all is specified. You will be asked for confirmation.

> **-f**

> > Disables interaction and does not ask for confirmation for shutting
> > down LIMs.

**limrestart** \[**-v**\] \[**-f**\] \[*host_name *\... \| **all**\]

:   

    Restarts LIM on the local host if no arguments are supplied.

> Restarts LIMs on the specified hosts or on all hosts in the cluster if
> the word all is specified. You will be asked for confirmation.

> limrestart should be used with care. Do not make any modifications
> until all the LIMs have completed the startup process. If you execute
> limrestart *host_name\...* to restart some of the LIMs after changing
> the configuration files, but other LIMs are still running the old
> configuration, confusion will arise among these LIMs. To avoid this
> situation, use reconfig instead of limrestart.

> **-v**

> > Displays detailed messages about configuration file checking.

> **-f **

> > Disables user interaction and forces LIM to restart if no fatal
> > errors are found. This option is useful in batch mode. limrestart -f
> > all is the same as reconfig -f.

**limlock** \[**-l** *time_seconds*\]

:   

    Locks LIM on the local host until it is explicitly unlocked if no
    time is specified.When a host is locked, LIM\'s load status becomes
    lockU. No job will be sent to a locked host by LSF.

> **-l ***time_seconds*

> > The host is locked for the specified time in seconds. This is useful
> > if a machine is running an exclusive job requiring all the available
> > CPU time and/or memory.

**limunlock**

:   

    Unlocks LIM on the local host.

```{=html}
<!-- -->
```

**resstartup **\[**-f**\]** **\[*host_name* \... \| **all**\]

:   

    Starts up RES on the local host if no arguments are specified.

> Starts up RESs on the specified hosts or on all hosts in the cluster
> if the word all is specified. You will be asked for confirmation.

For root installation to work properly, lsadmin must be installed as a
setuid to root program.

> **-f**

> > Disables interaction and does not ask for confirmation for starting
> > up RESs.

**resshutdown** \[**-f**\] \[*host_name *\... \| **all**\]

:   

    Shuts down RES on the local host if no arguments are specified.

> Shuts down RESs on the specified hosts or on all hosts in the cluster
> if the word all is specified. You will be asked for confirmation.

> If RES is running, it will keep running until all remote tasks exit.

> **-f**

> > Disables interaction and does not ask for confirmation for shutting
> > down RESs.

**resrestart **\[**-f**\] \[*host_name *\... \| **all**\] 

:   

    Restarts RES on the local host if no arguments are specified.

> Restarts RESs on the specified hosts or on all hosts in the cluster if
> the word all is specified. You will be asked for confirmation.

> If RES is running, it will keep running until all remote tasks exit.
> While waiting for remote tasks to exit, another RES is restarted to
> serve the new queries.

> **-f**

> > Disables interaction and does not ask for confirmation for
> > restarting RESs.

**reslogon** \[*host_name *\... \| **all**\] \[**-c** *cpu_time*\]

:   

    Logs all tasks executed by RES on the local host if no arguments are
    specified.

> Logs tasks executed by RESs on the specified hosts or on all hosts in
> the cluster if all is specified.

> RES will write the task\'s resource usage information into the log
> file lsf.acct.*host_name*. The location of the log file is determined
> by LSF_RES_ACCTDIR defined in lsf.conf. If LSF_RES_ACCTDIR is not
> defined, or RES cannot access it, the log file will be created in /tmp
> instead.

> **-c** *cpu_time*

> > Logs only tasks that use more than the specified amount of CPU time.
> > The amount of CPU time is specified by *cpu_time* in milliseconds.

**reslogoff** \[*host_name *\... \|** all**\]

:   

    Turns off RES task logging on the local host if no arguments are
    specified.

> Turns off RES task logging on the specified hosts or on all hosts in
> the cluster if all is specified.

**limdebug** \[**-c** **\"***class_name *\...**\"**\] 

:   \
    \[**-l** *debug_level*\] \[**-f** *logfile_name*\]\
    \[**-o**\] \[**\"***host_name *\...**\"**\]

    Sets the message log level for LIM to include additional information
    in log files. You must be root or the LSF administrator to use this
    command.

> If the command is used without any options, the following default
> values are used:

> *class_name* = 0 (no additional classes are logged)

> *debug_level* = 0 (LOG_DEBUG level in parameter LSF_LOG_MASK)

> *logfile_name* = current LSF system log file in the directory
> specified by LSF_LOGDIR in the format *daemon_name.*log*.host_name*
>
> *host_name*= local host (host from which command was submitted)

> **-c** **\"***class_name *\...**\"**

> > Specify software classes for which debug messages are to be logged.
> > If a list of classes is specified, they must be enclosed in
> > quotation marks and separated by spaces.
>
> > Possible classes:
>
> > LC_AUTH - Log authentication messages
>
> > LC_CHKPNT - log checkpointing messages
>
> > LC_COMM - Log communication messages
>
> > LC_EXEC - Log significant steps for job execution
>
> > LC_FILE - Log file transfer messages
>
> > LC_HANG - Mark where a program might hang
>
> > LC_PIM - Log PIM messages
>
> > LC_SCHED - Log JobScheduler messages
>
> > LC_SIGNAL - Log messages pertaining to signals
>
> > LC_TRACE - Log significant program walk steps
>
> > LC_XDR - Log everything transferred by XDR
>
> > Default: 0 (no additional classes are logged)
>
> > Note: Classes are also listed in lsf.h.

> **-l** *debug_level*

> > Specify level of detail in debug messages. The higher the number,
> > the more detail that is logged. Higher levels include all lower
> > levels.
>
> > Possible values:
>
> > 0 - LOG_DEBUG level in parameter LSF_LOG_MASK in lsf.conf.
>
> > 1 - LOG_DEBUG1 level for extended logging. A higher level includes
> > lower logging levels. For example, LOG_DEBUG3 includes LOG_DEBUG2
> > LOG_DEBUG1, and LOG_DEBUG levels.
>
> > 2 - LOG_DEBUG2 level for extended logging. A higher level includes
> > lower logging levels. For example, LOG_DEBUG3 includes LOG_DEBUG2
> > LOG_DEBUG1, and LOG_DEBUG levels.
>
> > 3 - LOG_DEBUG3 level for extended logging. A higher level includes
> > lower logging levels. For example, LOG_DEBUG3 includes LOG_DEBUG2,
> > LOG_DEBUG1, and LOG_DEBUG levels.
>
> > Default: 0 (LOG_DEBUG level in parameter LSF_LOG_MASK)

> **-f** *logfile_name*

> > Specify the name of the file into which debugging messages are to be
> > logged. A file name with or without a full path may be specified.
>
> > If a file name without a path is specified, the file will be saved
> > in the directory indicated by the parameter LSF_LOGDIR in lsf.conf.
>
> > The name of the file that will be created will have the following
> > format:
>
> > *logfile_name*.*daemon_name*.log.*host_name*
> >
> > If the specified path is invalid, on UNIX, the log file is created
> > in the /tmp directory.
>
> > If LSF_LOGDIR is not defined, daemons log to the syslog facility.
>
> > Default: current LSF system log file in the directory specified by
> > LSF_LOGDIR in the format *daemon_name*.log.*host_name*.

> **-o**

> > Turns off temporary debug settings and reset them to the daemon
> > starting state. The message log level is reset back to the value of
> > LSF_LOG_MASK and classes are reset to the value of LSF_DEBUG_RES,
> > LSF_DEBUG_LIM.
>
> > Log file is reset back to the default log file.

> **\"***host_name *\...**\"**

> > Sets debug settings on the specified host or hosts.
>
> > Default: local host (host from which command was submitted)

**resdebug** \[**-c** **\"***class_name***\"**\] \[**-l** *debug_level*\] \[**-f** *logfile_name*\] \[**-o**\] 

:   \[**\"***host_name *\...**\"**\]

    Sets the message log level for RES to include additional information
    in log files. You must be the LSF administrator to use this command,
    not root.

> See description of limdebug for an explanation of options.

**limtime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\] \[**-o**\] \[**\"***host_name \...***\"**\]

:   

    Sets timing level for LIM to include additional timing information
    in log files. You must be root or the LSF administrator to use this
    command.

> If the command is used without any options, the following default
> values are used:

> *timing_level* = no timing information is recorded

> *logfile_name* = current LSF system log file in the directory
> specified by LSF_LOGDIR in the format *daemon_name*.log.*host_name*
>
> *host_name *= local host (host from which command was submitted)

> **-l** *timing_level*

> > Specifies detail of timing information that is included in log
> > files. Timing messages indicate the execution time of functions in
> > the software and are logged in milliseconds.
>
> > Valid values: 1 \| 2 \| 3 \| 4 \| 5
>
> > The higher the number, the more functions in the software that are
> > timed and whose execution time is logged. The lower numbers include
> > more common software functions. Higher levels include all lower
> > levels.
>
> > Default: undefined (no timing information is logged)

> **-f** *logfile_name*

> > Specify the name of the file into which timing messages are to be
> > logged. A file name with or without a full path may be specified.
>
> > If a file name without a path is specified, the file will be saved
> > in the directory indicated by the parameter LSF_LOGDIR in lsf.conf.
>
> > The name of the file that will be created will have the following
> > format:
>
> > *logfile_name*.*daemon_name*.log.*host_name*
> >
> > If the specified path is invalid, on UNIX, the log file is created
> > in the /tmp directory.
>
> > If LSF_LOGDIR is not defined, daemons log to the syslog facility.
>
> > **Note: **Both timing and debug messages are logged in the same
> > files.
>
> > Default: current LSF system log file in the directory specified by
> > LSF_LOGDIR in the format *daemon_name*.log.*host_name*.

> **-o**

> > Turns off temporary timing settings and resets them to the daemon
> > starting state. The timing level is reset back to the value of the
> > parameter for the corresponding daemon (LSF_TIME_LIM, LSF_TIME_RES).
>
> > Log file is reset back to the default log file.

> **\"***host_name \...***\"**

> > Sets the timing level on the specified host or hosts.
>
> > Default: local host (host from which command was submitted)

**restime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\] \[**-o**\] \[**\"***host_name \...***\"**\]

:   

    Sets timing level for RES to include additional timing information
    in log files. You must be the LSF administrator can use this
    command, not root.

> See description of limtime for an explanation of options.

**help** \[*subcommand *\...\] \| **?** \[*subcommand *\...\]

:   

    Displays the syntax and functionality of the specified commands. The
    commands must be explicit to lsadmin.

> From the command prompt, you may use help or ?.

**quit** 

:   

    Exits the lsadmin session.

# SEE ALSO

ls_limcontrol(3), ls_rescontrol(3), ls_readconfenv(3),
ls_gethostinfo(3), ls_connect(3), ls_initrex(3), lsf.conf(5),
lsf.acct(5), bmgroup(1), busers(1)lsreconfig(8), lslockhost(8),
lsunlockhost(8)
