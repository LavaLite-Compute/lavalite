# NAME

nios - network I/O server for the LSF (Load Sharing Facility) system

# SYNOPSIS

**LSF_SERVDIR/nios**

# DESCRIPTION

**nios** is an application server process that is started by the LSF
library LSLIB when remote execution is first initiated. **nios** runs
concurrently with the client application, transparently handling all I/O
and signals to/from the remote tasks. The **nios** is a per-application
server process, that is, if there is more than one remote task belonging
to the same application, the tasks all share the same **nios** on the
client host. **nios** terminates when the application terminates.
**nios** exits when the application exits.

Most application programmers do not need to know that there is a
**nios** process running. Applications call **ls_rtask**(3) to start
remote processes and call **ls_rwait**(3) to collect the remote process
status. When a remote task finishes, a SIGUSR1 signal is sent to the
client application by the **nios**. The application can then call
**ls_rwait**(3) to get a remote child\'s status from the **nios**.
Remote tasks are identified by Task IDs (TID). **ls_rwait**(3) returns
the TID as well as the status of the remote child that has finished.

**I/O Shuffling**

:   **nios** is the I/O agent for all the remote tasks of an
    application. By default, **nios** reads from stdin and forwards the
    stream to all remote tasks. **nios** prints the output from all
    remote tasks to stdout and stderr. It is possible to disable or
    enable the stdin of a particular remote task by calling
    **ls_setstdin**(3). This allows stdin to be forwarded to the
    selected remote tasks. It is also possible to disable or enable
    reading of stdin by remote tasks. This is necessary for some
    applications such as a distributed shell, where the shell reads a
    command line from the stdin, and dispatches the task to remote hosts
    for execution, during which the remote task must read the stdin.

**TTY Mode**

:   When a distributed application is running on remote hosts, **nios**
    can switch the tty back and forth between local mode and remote
    mode. In local mode, all control keys are interpreted locally. In
    remote mode the input stream is delivered directly to remote tasks.
    Users normally do not need to know these details.

**Job Control**

:   **nios** transparently supports job control. It catches signals
    (such as SIGTSTP, SIGINT, and SIGTERM) and passes them to remote
    tasks. Therefore, when the user presses job control keys such as
    Ctrl-Z or Ctrl-C, remote tasks are sent the SIGTSTP or SIGINT
    signals. With the support of **nios**, remote tasks can be suspended
    or resumed, put to the background or foreground, and killed, as if
    they were running locally.

**nios-Client Coordination**

:   For applications that need pseudo-terminals on remote hosts (see
    **ls_rtask**(3)), care must be taken in synchronizing the tty\'s
    local/remote mode setting. When the client is stopped due to a
    SIGTSTP signal, it has to make sure that **nios** is stopped first
    by calling **ls_stoprex**(3). In most cases, this is already handled
    by the LSF library automatically in the default SIGTSTP handler. If
    you define this signal handler explicitly in your program, you must
    call **ls_stoprex**(3) in cases where the client wants to be
    stopped. Failure to do so may cause some temporary misbehavior in
    the terminal environment immediately after stopping a remote
    interactive job. All applications that use pseudo-ttys for remote
    tasks must call **ls_exit**(3) before exiting in order to ensure
    that the local terminal environment is restored correctly.

# NOTE

**nios** must be installed in directory **LSF_SERVERDIR** as defined in
the file **lsf.conf**(5) or as an environment variable.

# SEE ALSO

**lsf.conf**(5), **res**(8), **ls_rtask**(3), **ls_rwait**(3),
**ls_setstdin**(3), **ls_stoprex**(3), **ls_exit**(3), **lslib**(3)
