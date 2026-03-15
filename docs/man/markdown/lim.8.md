# NAME

lim - Load Information Manager (LIM) for the Load Sharing Facility (LSF)
system

# SYNOPSIS

**LSF_SERVDIR/lim \[ -h \] \[ -V \] \[ -C \] \[ -d ***env_dir*** \] \[
-***debug_level*** \]**

# DESCRIPTION

LIM is a server that runs on every server host which is participating in
load sharing. It provides system configuration information, load
information, and placement advice services. The LIMs on all hosts
coordinate in collecting and transmitting load information. Load
information is transmitted between LIMs in the form of vectors of load
indices. These are described in the **LOAD INDICES** section.

In order to run LIM on a host, the host must be configured as a server
host in the **lsf.cluster.**\<*clustername*\> file. See the description
of the **SERVER** keyword in the **lsf.cluster.**\<*clustername*\> file
in **lsf.cluster**(5) for more information on server hosts and client
hosts.

A master LIM is elected for each LSF cluster. The master LIM receives
load information from all slave **LIM**s and provides services to all
hosts. The slave **LIM**s periodically check their own load conditions
and send a load vector to the master if significant changes in load
condition are observed. The minimum load information exchange interval
is 15 seconds. However, a user site can redefine this by setting the
**EXINTERVAL** parameter in **PARAMETERS** section of **lsf.cluster**
file (see **lsf.cluster**(5)). The actual exchange intervals can be
bigger if the load does not change much.

Slave LIMs also provide some services to the local application. For
example, applications contact the local LIM to get local load
information, the local cluster name, the available resources on any
host, and the name of the host on which the master LIM is running. A
slave LIM accepts application requests to lock or unlock the local host.
Slave LIMs also monitor the status of the master LIM and elect a new
master if the original one becomes unavailable.

LIM reads **lsf.conf**(5) file to get the following parameters:
**LSF_LIM_DEBUG**, **LSF_CONFDIR**, **LSF_SERVERDIR**, **LSF_LIM_PORT**,
**LSF_LOG_MASK** and **LSF_LOGDIR**. If **LSF_LIM_DEBUG** is defined,
LIM runs at the specified debug level. **LSF_CONFDIR** tells LIM where
to find the LSF configuration files. **LSF_SERVERDIR** is used by LIM
during reconfiguration in order to locate the directory where the LIM
binary is stored. **LSF_LIM_PORT** is used to define the UDP port number
that LIM uses for serving all applications. If this is not defined, LIM
will try to get the port number from services database (see
**getservbyname**(3)). If **LSF_LOG_MASK** is defined, the defined log
mask is set so that messages with lower priority than **LSF_LOG_MASK**
are not logged. If **LSF_LOG_MASK** is not defined, a default log mask
**LOG_WARNING** is used. If **LSF_LOGDIR** is defined, error messages
are logged into the file **lim.log.**\<*hostname*\> in the directory
**LSF_LOGDIR**. If LIM cannot write in **LSF_LOGDIR**, errors are logged
into **/tmp**. If **LSF_LOGDIR** is not defined, **syslog** is used to
log error messages with the level **LOG_ERR**. **LSF_CLUSTER_NAME** is a
mandatory parameter which specifies the cluster that the LIM belongs to.

# LOAD INDICES

The load information provided by the LIM consists of the following load
indices:

**r15s**

:   The 15-second exponentially averaged CPU run queue length,
    normalized by the CPU speed.

**r1m**

:   The 1-minute exponentially averaged CPU run queue length, normalized
    by the CPU speed.

**r15m**

:   The 15-minute exponentially averaged CPU run queue length,
    normalized by the CPU speed.

**ut**

:   The CPU utilization exponentially averaged over the last minute,
    between 0 and 1.

**pg**

:   The memory paging rate exponentially averaged over the last minute,
    in pages per second.

**io**

:   The disk I/O rate exponentially averaged over the last minute, in
    KBytes per second.

**ls**

:   The number of current login users.

**it**

:   The idle time of the host (keyboard not touched on all logged in
    sessions), in minutes.

**tmp**

:   The amount of free disk space in **/tmp**, in MBytes.

**swp**

:   The amount of currently available swap space, in MBytes.

**mem**

:   The amount of currently available memory, in MBytes.

In addition, an LSF installation can configure arbitrary external load
indices as explained below.

# EXTERNAL LOAD INFORMATION MANAGER

The load indices monitored at a site can be extended by directing the
LIM to invoke and communicate with an External Load Information Manager
(ELIM). The ELIM is responsible for collecting load indices not managed
by the LIM. These indices are passed on to the LIM by ELIM through a
well defined protocol.

To configure an external load index, the index name must first be
defined in the **Resources** section of the **lsf.shared**(5) file. The
location of the resource must then be defined in the **ResourceMap**
section of the **lsf.cluster**(5) file. A load index is either a shared
or a non-shared resource. A non-shared resource is defined on each host
in the cluster, and the value is specific to each host. A shared
resource is a resource whose value is shared by more than one host, and
the resource may be defined only on a subset of the hosts. The ELIM can
report both shared and non-shared resources. The location specification
of the resource in the **ResourceMap** section of the **lsf.cluster**(5)
file defines whether a resource is shared or non-shared.

If a non-shared external index is defined, the LIM on each host will
invoke the ELIM on start up. If a shared external index is defined, the
LIM will start the ELIM only if the index applies to the current host,
and the current host is the first host from the hostlist defined for the
resource instance, which is alive. (see **lsf.cluster**(5)). The
executable for the ELIM must be in **LSF_SERVDIR** and must have the
name \'**elim**\'. The ELIM will run with the same user ID and file
access permissions as the LIM. If the ELIM dies, it will be restarted by
the LIM. If the LIM dies, the master LIM will ensure that the ELIM on
the next host on the hostlist defined for the resource instance is
started. If this dead LIM comes back alive again however, the original
ELIM will be restarted and the backup ELIM terminated.

When the LIM terminates, it will send a SIGTERM signal to the ELIM. The
ELIM is expected to terminate upon receiving this signal.

The LIM communicates with the ELIM through 2 environment variables:
LSF_MASTER and LSF_RESOURCES. LSF_MASTER is set to an empty string if
the ELIM is being started by the LIM on the master host. It is left
unset otherwise. LSF_RESOURCES is set to a space separated string of
dynamic shared resources indices for which the ELIM on that host is
responsible to collect. Thus if a host is defined in the resource
instances for the dynamic shared resource indices, \`**work**\',
\`**netio**\', and \`**users**\', then LSF_RESOURCES will be set with
the string \"**work** **netio** **users**\".

The ELIM communicates with the LIM by periodically writing a load update
string to its stdout. The load update string contains the number of
indices followed by a list of name-value pairs in the following format:
\"*numIndx indexname1 value1 indexname2 value2 \.... indexnameN
valueN*\". For example \"**3 work 47.5 netio 344.0 users 5**\" is a
valid load update string in which the ELIM is reporting 3 indices,
\`**work**\', \`**netio**\', and \`**users**\' with values **47.5**,
**344.0**, and **5**, respectively. Index names corresponding to the
built-in indices are also accepted. In this case the value produced by
the ELIM overrides the value produced by LIM. It is up to the ELIM to
ensure that the semantics of all indices it samples correspond to those
returned by **lsinfo**(1) or **ls_info**(3).

The ELIM should ensure that the entire load update string is written
successfully to stdout. This can be done by checking the return value of
**printf**(3) or **fprintf**(3) if the ELIM is implemented as C program
or the return code of **echo**(1) from a shell script. Failure to write
the load update string should cause the ELIM to terminate.

# CUSTOMIZATION OF PARAMETERS

You can customize LIM by changing the configuration files in the
**LSF_CONFDIR** directory (defined in **lsf.conf**(5)). The
configuration file **lsf.cluster**(5) located in that directory define
LSF clusters, the resources on individual hosts, the CPU speeds of
individual hosts, whether a host is a server host or client host, the
load threshold values beyond which a host is considered to be
overloaded, the run windows during which a host is available for load
sharing, and so on.

# OPTIONS

**-h**

:   Print command usage to stderr and exit.

**-V**

:   Print LSF release version to stderr and exit.

**-C**

:   Check the configuration file content with verbose error reporting
    and exit.

**-d *env_dir***

:   Read **lsf.conf** from the directory *env_dir,* rather than the
    default directory **/etc**, or the directory specified by the
    **LSF_ENVDIR** environment variable.

**-***debug_level*

:   Set the debug level. Valid values are 1 and 2. If specified, LIM
    runs in debugging mode. In debugging mode, LIM uses a hardcoded port
    number rather than the one registered in system services. Also,
    privileged operations such as reconfiguration and host lock or
    unlock can be done by any user. If debug mode is not enabled, only
    root and **LSF_MANAGER** (defined in **lsf.conf**(5)) can do these
    privileged operations. If *debug_level* is 1, LIM runs in the
    background, with no associated control terminal. If *debug_level* is
    2, LIM runs in the foreground, printing error messages on to tty.
    The *debug_level* option overrides the environment variable
    **LSF_LIM_DEBUG** defined in **lsf.conf**(5).

# NOTE

LIM needs read access to **/dev/kmem** or its equivalent.

# FILES

**/etc/lsf.conf** (by default) or **LSF_ENVDIR/lsf.conf**

:   

**LSF_CONFDIR/lsf.shared**

:   

**LSF_CONFDIR/lsf.cluster.\<*clustername*\>**

:   

# SEE ALSO

**lsf.conf**(5), **lsf.cluster**(5), **lsinfo**(1), **ls_info**(3),
**syslog**(3)
