\

# NAME

**badmin** - administrative tool for Lava

# SYNOPSIS

**badmin ***subcommand* .PP **badmin **\[**-h** \| **-V**\]

# SUBCOMMAND LIST

\
**ckconfig** \[**-v**\]\
**reconfig** \[**-v**\] \[**-f**\]\
**mbdrestart** \[**-v**\] \[**-f**\]\
**qopen** \[*queue_name* \... \| **all**\]\
**qclose** \[*queue_name* \... \| **all**\]\
**qact** \[*queue_name* \... \| **all**\]\
**qinact** \[*queue_name* \... \| **all**\]\
**qhist** \[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\]
\[*queue_name *\...\]\
**hopen** \[*host_name* \... \| *host_group* \... \| **all**\]\
**hclose** \[*host_name* \... \| *host_group* \... \| **all**\]\
**hrestart** \[**-f**\] \[*host_name* \...\| **all**\]\
**hshutdown** \[**-f**\] \[*host_name* \... \| **all**\]\
**hstartup** \[**-f**\] \[*host_name* \... \| **all**\]\
**hhist** \[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\]
\[*host_name* \...\]\
**mbdhist **\[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\]\
**hist** \[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\]\
**help** \[*command* \...\] \| **?** \[*command* \...\]\
**quit** .br **sbddebug** \[**-c** *class_name \...*\] \[**-l**
*debug_level*\] \[**-f** *logfile_name*\] \[**-o**\] \[*host_name
\...*\]\
**mbddebug** \[**-c** *class_name \...*\] \[**-l** *debug_level*\]
\[**-f** *logfile_name*\] \[**-o**\]\
**sbdtime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\]
\[**-o**\] \[*host_name \...*\]\
**mbdtime** \[**-l** *timing_level*\] \[**-f** *logfile_name\]*
\[**-o**\]

# DESCRIPTION

This command can only be used by Lava administrators.

badmin provides a set of commands to control and monitor Lava. If no
subcommands are supplied for badmin, badmin prompts for a command from
standard input. Commands bqc(8), breconfig(8) and breboot(8) are
superceded by badmin(8).

Information about each command is available through the help command.

The badmin commands consist of a set of privileged commands and a set of
non**-**privileged commands. Privileged commands can only be invoked by
root or Lava administrators as defined in the configuration file (see
lsf.cluster.cluster(5) for ClusterAdmin). Privileged commands are:

reconfig

mbdrestart

qopen

qclose

qact

qinact

hopen

hclose

hrestart

hshutdown

hstartup

All other commands are non**-**privileged commands and can be invoked by
any Lava user. If the privileged commands are to be executed by an Lava
administrator, badmin must be installed setuid root, because it needs to
send the request using a privileged port.

For subcommands for which multiple host names or host groups can be
specified, do not enclose the multiple names in quotation marks.

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

    Prints Lava release version to stderr and exits.

# USAGE

**ckconfig** \[**-v**\]

:   

    Checks Lava configuration files. Configuration files are located in
    the LSB_CONFDIR/*cluster_name*/configdir directory.

> The LSB_CONFDIR variable is defined in lsf.conf (see lsf.conf(5))
> which is in LSF_ENVDIR or /etc (if LSF_ENVDIR is not defined).

> By default, badmin ckconfig displays only the result of the
> configuration file check. If warning errors are found, badmin prompts
> you to display detailed messages.

> **-v**

> > Verbose mode. Displays detailed messages about configuration file
> > checking to stderr.

**reconfig** \[**-v**\] \[**-f**\]

:   

    Dynamically reconfigures Lava without restarting MBD. Configuration
    files are checked for errors and the results displayed to stderr. If
    no errors are found in the configuration files, a reconfiguration
    request is sent to MBD and configuration files are reloaded.

> With this command, MBD is not restarted and lsb.events is not
> replayed. To restart MBD and replay lsb.events, use badmin mbdrestart.

> When you issue this command, MBD is available to service requests
> while reconfiguration files are reloaded. Configuration changes made
> since system boot or the last reconfiguration take effect.

> If warning errors are found, badmin prompts you to display detailed
> messages. If fatal errors are found, reconfiguration is not performed,
> and badmin exits.

> If you add a host to a host group, or a host to a queue, the new host
> will not be recognized by jobs that were submitted before you
> reconfigured. If you want the new host to be recognized, you must use
> the command badmin mbdrestart.

> **-v**

> > Verbose mode. Displays detailed messages about the status of the
> > configuration files. Without this option, the default is to display
> > the results of configuration file checking. All messages from the
> > configuration file check are printed to stderr.

> **-f**

> > Disables interaction and proceeds with reconfiguration if
> > configuration files contain no fatal errors.

**mbdrestart** \[**-v**\] \[**-f**\]

:   

    Dynamically reconfigures Lava and restarts MBD. Configuration files
    are checked for errors and the results printed to stderr. If no
    errors are found, configuration files are reloaded, MBD is
    restarted, and events in lsb.events are replayed to recover the
    running state of the last MBD. MBD is unavailable to service
    requests while it restarts.

> If warning errors are found, badmin prompts you to display detailed
> messages. If fatal errors are found, MBD restart is not performed, and
> badmin exits.

> If lsb.events is large, or many jobs are running, restarting MBD can
> take several minutes. If you only need to reload the configuration
> files, use badmin reconfig.

> **-v**

> > Verbose mode. Displays detailed messages about the status of
> > configuration files. All messages from configuration checking are
> > printed to stderr.

> **-f**

> > Disables interaction and forces reconfiguration and MBD restart to
> > proceed if configuration files contain no fatal errors.

**qopen** \[*queue_name \... *\| **all**\]

:   

    Opens specified queues, or all queues if the reserved word all is
    specified. If no queue is specified, the system default queue is
    assumed (see lsb.queues(5) for DEFAULT_QUEUE). A queue can accept
    batch jobs only if it is open.

```{=html}
<!-- -->
```

**qclose** \[*queue_name* \... \| **all**\]

:   

    Closes specified queues, or all queues if the reserved word all is
    specified. If no queue is specified, the system default queue is
    assumed. A queue will not accept any job if it is closed.

```{=html}
<!-- -->
```

**qact **\[*queue_name* \... \| **all**\]

:   

    Activates specified queues, or all queues if the reserved word all
    is specified. If no queue is specified, the system default queue is
    assumed. Jobs in a queue can be dispatched if the queue is
    activated.

> A queue inactivated by its run windows cannot be reactivated by this
> command (see lsb.queues(5) for RUN_WINDOW).

**qinact** \[*queue_name* \... \| **all**\]

:   

    Inactivates specified queues, or all queues if the reserved word all
    is specified. If no queue is specified, the system default queue is
    assumed. No job in a queue can be dispatched if the queue is
    inactivated.

```{=html}
<!-- -->
```

**qhist** \[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\] \[*queue_name* \...\]

:   

    Displays historical events for specified queues, or for all queues
    if no queue is specified. Queue events are queue opening, closing,
    activating and inactivating.

> **-t** *time0*,*time1*

> > Displays only those events that occurred during the period from
> > *time0* to *time1*. See bhist(1) for the time format. The default is
> > to display all queue events in the event log file (see below).

> **-f** *logfile_name*

> > Specify the file name of the event log file. Either an absolute or a
> > relative path name may be specified. The default is to use the event
> > log file currently used by the Lava system:
> > LSB_SHAREDIR/cluster_name/logdir/lsb.events. Option -f is useful for
> > offline analysis.

**hopen** \[*host_name *\... \| *host_group *\... \| **all**\]

:   

    Opens batch server hosts. Specify the names of any server hosts or
    host groups (see bmgroup(1)). All batch server hosts will be opened
    if the reserved word all is specified. If no host or host group is
    specified, the local host is assumed. A host accepts batch jobs if
    it is open.

```{=html}
<!-- -->
```

**hclose** \[*host_name *\... \| *host_group *\... \| **all**\]

:   

    Closes batch server hosts. Specify the names of any server hosts or
    host groups (see bmgroup(1)). All batch server hosts will be closed
    if the reserved word all is specified. If no argument is specified,
    the local host is assumed. A closed host will not accept any new
    job, but jobs already dispatched to the host will not be affected.
    Note that this is different from a host closed by a window **-** all
    jobs on it are suspended in that case.

```{=html}
<!-- -->
```

**hrestart** \[**-f**\] \[*host_name *\... \| **all**\]

:   

    Restarts SBD on the specified hosts, or on all server hosts if the
    reserved word all is specified. If no host is specified, the local
    host is assumed. SBD will re**-**execute itself from the beginning.
    This allows new SBD binaries to be used.

> **-f**

> > Disables interaction and does not ask for confirmation for
> > restarting SBDs.

**hshutdown** \[**-f**\] \[*host_name *\... \| **all**\]

:   

    Shuts down SBD on the specified hosts, or on all batch server hosts
    if the reserved word all is specified. If no host is specified, the
    local host is assumed. SBD will exit upon receiving the request.

> **-f**

> > Disables interaction and does not ask for confirmation for shutting
> > down SBDs.

**hstartup** \[**-f**\] \[*host_name *\... \| **all**\]

:   

    Starts up SBD on the specified hosts, or on all batch server hosts
    if the reserved word all is specified. Only root can use this
    option, and those users must be able to use rsh on all Lava hosts.
    If no host is specified, the local host is assumed.

> **-f**

> > Disables interaction and does not ask for confirmation for starting
> > up SBDs.

**hhist** \[**-t** *time0*,*time1*\] \[**-f** *logfile_name*\] \[*host_name *\...\]

:   

    Displays historical events for specified hosts, or for all hosts if
    no host is specified. Host events are host opening and closing.
    Options -t and -f are exactly the same as those of qhist (see
    above).

```{=html}
<!-- -->
```

**mbdhist** \[**-t ***time0*,*time1*\] \[**-f** *logfile_name*\]

:   

    Displays historical events for MBD. Events describe the starting and
    exiting of MBD. Options -t and -f are exactly the same as those of
    qhist (see above).

```{=html}
<!-- -->
```

**hist** \[**-t** *time0,time1*\] \[**-f** *logfile_name*\]

:   

    Displays historical events for all the queues, hosts and MBD.
    Options -t and -f are exactly the same as those of qhist (see
    above).

```{=html}
<!-- -->
```

**help** \[*command \...*\] \| **?** \[*command \...*\]

:   

    Displays the syntax and functionality of the specified commands.

```{=html}
<!-- -->
```

**quit**

:   

    Exits the badmin session.

```{=html}
<!-- -->
```

**sbddebug** \[**-c** *class_name \...*\] \[**-l** *debug_level*\] \[**-f** *logfile_name*\] \[**-o**\] 

:   \[*host_name \...*\]

    Sets the message log level for SBD to include additional information
    in log files. You must be root or the Lava administrator to use this
    command.

> If the command is used without any options, the following default
> values are used:

> *class_name* = 0 (no additional classes are logged)

> *debug_level* = 0 (LOG_DEBUG level in parameter LSF_LOG_MASK)

> *logfile_name* = current Lava system log file in the directory
> specified by LSF_LOGDIR in the format *daemon_name*.log.*host_name*
>
> *host_name* = local host (host from which command was submitted)

> **-c** *class_name \...*

> > Specifies software classes for which debug messages are to be
> > logged.
>
> > Format of *class_name *is the name of a class, or a list of class
> > names separated by spaces and enclosed in quotation marks.
>
> > Possible classes:
>
> > LC_AUTH - Log authentication messages
>
> > LC_CHKPNT - Log checkpointing messages
>
> > LC_COMM - Log communication messages
>
> > LC_EXEC - Log significant steps for job execution
>
> > LC_FILE - Log file transfer messages
>
> > LC_HANG - Mark where a program might hang
>
> > LC_JLIMIT - Log job slot limit messages
>
> > LC_LOADINDX - Log load index messages
>
> > LC_PEND - Log messages related to job pending reasons
>
> > LC_PERFM - Log performance messages
>
> > LC_PIM - Log PIM messages
>
> > LC_SIGNAL - Log messages pertaining to signals
>
> > LC_SYS - Log system call messages
>
> > LC_TRACE - Log significant program walk steps
>
> > LC_XDR - Log everything transferred by XDR
>
> > Note: Classes are also listed in lsf.h.
>
> > Default: 0 (no additional classes are logged)

> **-l** *debug_level*

> > Specifies level of detail in debug messages. The higher the number,
> > the more detail that is logged. Higher levels include all lower
> > levels.
>
> > Possible values:
>
> > 0 LOG_DEBUG level in parameter LSF_LOG_MASK in lsf.conf.
>
> > 1 LOG_DEBUG1 level for extended logging. A higher level includes
> > lower logging levels. For example, LOG_DEBUG3 includes LOG_DEBUG2
> > LOG_DEBUG1, and LOG_DEBUG levels.
>
> > 2 LOG_DEBUG2 level for extended logging. A higher level includes
> > lower logging levels. For example, LOG_DEBUG3 includes LOG_DEBUG2
> > LOG_DEBUG1, and LOG_DEBUG levels.
>
> > 3 LOG_DEBUG3 level for extended logging. A higher level includes
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
> > *logfile_name.daemon_name.*log*.host_name*
> >
> > If the specified path is invalid, on UNIX, the log file is created
> > in the /tmp directory.
>
> > If LSF_LOGDIR is not defined, daemons log to the syslog facility.
>
> > Default: current Lava system log file in the directory specified by
> > LSF_LOGDIR in the format *daemon_name*.log*.host_name*.

> **-o**

> > Turns off temporary debug settings and resets them to the daemon
> > starting state. The message log level is reset back to the value of
> > LSF_LOG_MASK and classes are reset to the value of LSB_DEBUG_MBD,
> > LSB_DEBUG_SBD.
>
> > The log file is also reset back to the default log file.

> *host_name \...*

> > Optional. Sets debug settings on the specified host or hosts.
>
> > Lists of host names must be separated by spaces and enclosed in
> > quotation marks.
>
> > Default: local host (host from which command was submitted)

**mbddebug** \[**-c** *class_name \...*\] \[**-l** *debug_level*\] \[**-f** *logfile_name*\] \[**-o**\]

:   

    Sets message log level for MBD to include additional information in
    log files. You must be root or the Lava administrator to use this
    command.

> See sbddebug for an explanation of options.

**sbdtime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\] \[**-o**\] \[*host_name \...*\]

:   

    Sets the timing level for SBD to include additional timing
    information in log files. You must be root or the Lava administrator
    to use this command.

> If the command is used without any options, the following default
> values are used:

> *timing_level* = no timing information is recorded

> *logfile_name* = current Lava system log file in the directory
> specified by LSF_LOGDIR in the format *daemon_name.*log*.host_name*
>
> *host_name *= local host (host from which command was submitted)

> **-l ***timing_level*

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
> > *logfile_name.daemon_name.*log*.host_name*
> >
> > If the specified path is invalid, on UNIX, the log file is created
> > in the /tmp directory.
>
> > If LSF_LOGDIR is not defined, daemons log to the syslog facility.
>
> > **Note: **Both timing and debug messages are logged in the same
> > files.
>
> > Default: current Lava system log file in the directory specified by
> > LSF_LOGDIR in the format *daemon_name.*log*.host_name*.

> **-o**

> > Optional. Turn off temporary timing settings and reset them to the
> > daemon starting state. The timing level is reset back to the value
> > of the parameter for the corresponding daemon (LSB_TIME_MBD,
> > LSB_TIME_SBD).
>
> > The log file is also reset back to the default log file.

> *host_name *\...

> > Sets the timing level on the specified host or hosts.
>
> > Lists of hosts must be separated by spaces and enclosed in quotation
> > marks.
>
> > Default: local host (host from which command was submitted)

**mbdtime** \[**-l** *timing_level*\] \[**-f** *logfile_name*\] \[**-o**\]

:   

    Sets timing level for MBD to include additional timing information
    in log files. You must be root or the Lava administrator to use this
    command.

> See sbdtime for an explanation of options.

# SEE ALSO

bqueues(1), bhosts(1), lsb.queues(5), lsb.hosts(5), lsf.conf(5),
lsf.cluster(5), sbatchd(8), mbatchd(8)
