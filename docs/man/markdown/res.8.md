# NAME

res - remote execution server of the LSF system (Load Sharing Facility)

# SYNOPSIS

**LSF_SERVDIR/res \[ -h \] \[ -V \] \[ -d ***env_dir*** \] \[
-***debug_level*** \]**

# DESCRIPTION

RES is a Remote Execution Server running on every server host
participating in load sharing. It provides remote execution services for
both sequential and parallel jobs.

In order to run RES, it must be registered in the system services table
(for example, **/etc/services** or the NIS services map) or
**LSF_RES_PORT** (see **lsf.conf**(5)) must be defined in the
**lsf.conf** file. The service name is \`**res**\', and it has the
protocol \`**tcp**\' (see **services**(5)). You can specify any unused
port number.

LSF library calls are available for application programmers to contact a
RES (see **lslib**(3)). Using the LSF library, the programmer can start
remote jobs on one or more remote hosts sequentially or in parallel.
Remote processes have the same execution environment as the client, and
status information from remote processes is passed back to the client
with the same data structure that **wait**(2) uses. Signals can be
delivered to all remote jobs from the client transparently, and
pseudo-terminals are used to support remote interactive jobs.

RES reads the **lsf.conf** file (see option **-d**) to get the following
parameters: **LSF_RES_DEBUG**, **LSF_AUTH**, **LSF_RES_PORT**,
**LSF_LOG_MASK**, **LSF_LOGDIR** and **LSF_RES_ACCTDIR**.

If **LSF_RES_DEBUG** is defined, RES will run in the specified debug
level.

**LSF_AUTH** tells RES the name of the authentication server (see
**lsf.conf**(5)). If **LSF_AUTH** is not defined, then RES will only
accept requests from privileged ports.

If **LSF_LOG_MASK** is defined, then the RES log mask will be set so
that any log messages with lower priorities than **LSF_LOG_MASK** will
not be logged. If **LSF_LOG_MASK** is not defined, a default log mask of
**LOG_WARNING** will be used.

If **LSF_LOGDIR** is defined, error messages will be logged in the file
**res.log.**\<*hostname*\> in the directory **LSF_LOGDIR**. If RES fails
to write into **LSF_LOGDIR**, then the log file is created in **/tmp**.
If **LSF_LOGDIR** is not defined, then syslog will be used to log error
messages with level **LOG_ERR**.

If **LSF_RES_ACCTDIR** is defined, task resource usage information will
be logged in the file **lsf.acct.***\<hostname\>* in the directory
**LSF_RES_ACCTDIR**. If **LSF_RES_ACCTDIR** is not defined, or RES fails
to write into **LSF_RES_ACCTDIR**, the log file will be created in
**/tmp**. By default, RES will not write any information to the log file
unless logging is turned on by the **lsadmin reslogon** command.

# OPTIONS

**-h**

:   Print command usage to stderr and exit.

**-V**

:   Print LSF release version to stderr and exit.

**-d *env_dir***

:   Read **lsf.conf** from the directory *env_dir,* rather than from the
    default directory **/etc**, or from the directory **LSF_ENVDIR**
    that is set in the environment variable.

**-***debug_level*

:   Set the debugging mode and level. Valid values are 1 and 2. If
    specified, the normal user can run RES in debugging mode.
    Authentication is not done in debugging mode, therefore RES can only
    serve one user. If the debug_level is 1, RES runs in background
    mode, with no associated control terminal. If *debug_level* is 2,
    RES runs in foreground mode, printing error messages to tty. The
    *debug_level* option overrides the environment variable
    **LSF_RES_DEBUG** defined in **lsf.conf**.

# SEE ALSO

**lsf.conf**(5), **lslib**(3), **syslog**(3), **nios**(8),
**lsadmin**(8), **lsf.acct**(5)
