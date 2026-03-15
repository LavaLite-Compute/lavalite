# NAME

mbatchd, sbatchd - master and slave daemons of the Lava system

# SYNOPSIS

**LSF_SERVERDIR/mbatchd \[ -h \] \[ -V \] \[ -C \] \[ -d ***env_dir***
\] \[ -***debug_level*** \]**

**LSF_SERVERDIR/sbatchd \[ -h \] \[ -V \] \[ -d ***env_dir*** \] \[
-***debug_level*** \]**

# DESCRIPTION

Lava is a load sharing batch system that supports distributed batch job
processing. There is a slave batch daemon, sbatchd, on every server
host. The slave batch daemon on the same host as the master Load
Information Manager (LIM) of the underlying Lava Base is responsible for
starting a master batch daemon, mbatchd, on its host for the local Lava
cluster. The sbatchds are typically started at system boot time by
**rc.local**(8). You must never start the mbatchd daemon unless you are
running it with the **-C** option.

In order to use the Lava system, mbatchd and sbatchd must be registered
in the services database, such as **/etc/services** or the NIS services
map, or **LSB_MBD_PORT** and **LSB_SBD_PORT** must be defined in the
file **lsf.conf**. The service names are \`**mbatchd**\' and
\`**sbatchd**\' respectively, and the protocol is \`**tcp**\' (see
**services**(8)). You can select any unused port numbers.

You can submit batch jobs from any host to the mbatchd. mbatchd contacts
the LIM to obtain current load information about hosts that satisfy the
resource requirements provided at job submission (see **bsub**(1)). If
no suitable hosts are found, the jobs are held by the mbatchd (that is,
they are pending). Once suitable execution hosts are found, mbatchd
dispatches the jobs to one or more sbatchds on the selected hosts for
execution.

An sbatchd accepts job execution requests from the mbatchd, starts the
jobs and monitors the progress of the jobs. Based on the execution
conditions specified by the mbatchd at job dispatch time, an sbatchd may
stop and resume a job in response to changes in the host load
conditions, interactive user activities and queue or host run windows.
Such actions by an sbatchd are reported back to the mbatchd. When a job
is finished (either completed successfully or with an error, or
terminated by a user, a Lava administrator or the queue administrator),
the job output is either mailed to the user who submitted the job, or
stored in a file specified at job submission (see **bsub**(1)).

Besides processing user requests, mbatchd performs most of the
scheduling functions of Lava. Lava is fault tolerant: a new mbatchd is
started if the host of the current mbatchd fails, and no jobs are lost,
except those running on the failed host. These jobs may be rerun from
begin on some other hosts or, if they are checkpointable, restarted from
their last checkpoint on some other hosts.

Most of the user\'s environment (such as the current working directory,
umask, and user groups) at the time of batch job submission is retained
and used for batch job execution.

mbatchd and sbatchd read the file **lsf.conf** (see **-d** option) to
get the environment information. The file **lsf.conf** is a generic
configuration file shared by Lava and applications built on top of it.
It contains environment information as to where the specific
configuration files and data files are and other information that
dictates the behavior of the software. The information of interest to
Lava includes: **LSB_CONFDIR**, **LSB_SHAREDIR**, **LSB_LOCALDIR**,
**LSB_MAILPROG**, **LSB_MAILTO**, **LSF_SERVERDIR**, **LSF_LOG_MASK**,
**LSB_DEBUG**, **LSB_MBD_PORT**, **LSB_SBD_PORT** and **LSF_LOGDIR**.
See **lsf.conf**(5) for details of **LSF_SERVERDIR**, **LSF_LOG_MASK**,
and **LSF_LOGDIR**. The other parameters are explained below:

**LSB_CONFDIR**

:   The directory in which all batch configuration files are stored. The
    files used by Lava are organized on a cluster basis, i.e., each
    cluster has its unique set of directories. The actual files for the
    cluster \<*clustername*\> are stored in directory
    **LSB_CONFDIR**/\<*clustername*\>/**configdir**. The directory
    **LSB_CONFDIR** should be accessible from all Lava server hosts in
    the cluster. The directory **LSB_CONFDIR** must be owned by root and
    accessible by all users. The directories
    **LSB_CONFDIR**/\<*clustername*\> and
    **LSB_CONFDIR**/\<*clustername*\>/**configdir** must be owned by the
    Lava primary (first) administrator and readable by all users. The
    Lava installation procedure already does this automatically for you.
    If you change the Lava administrators after installation, you should
    make sure that all the file ownership is correct and change the
    section **ClusterAdmin** in your Lava configuration file (see
    **lsf.cluster**(5)).

**LSB_SHAREDIR**

:   The directory in which the master batch daemon (mbatchd) stores job
    data and accounting files. The actual files for the cluster
    \<*clustername*\> are stored in directory
    **LSB_SHAREDIR**/\<*clustername*\>/**logdir**. The directory
    **LSB_SHAREDIR** should be accessible from all Lava server hosts in
    the cluster. The directory **LSB_SHAREDIR** must be owned by root
    and accessible by all users. The directories
    **LSB_SHAREDIR**/\<*clustername*\> and
    **LSB_SHAREDIR**/\<*clustername*\>/**logdir** must be owned and
    writable by the Lava primary (first) administrator and accessible by
    all users.

**LSB_LOCALDIR**

:   The directory in which the replicated event logging process stores
    the event files. The replicated event logging feature provides added
    fault-tolerance to mbatchd/sbatchd as it will be able to tolerate
    failures of the file server where **LSB_SHAREDIR** is located. To
    enable this feature **LSB_LOCALDIR** must be defined in lsf.conf.
    **LSB_LOCALDIR** should be a local directory which exists ONLY on
    the first master master (i.e. first host configured in
    **lsf.cluster**(5)). Two mbatchd daemons will be started. One of
    these daemons will use **LSB_LOCALDIR** to store the primary copy of
    the **lsb.events**(5) files, which will than be replicated in the
    **LSB_SHAREDIR** directory.

**LSB_MAILPROG**

:   This parameter is optional. If defined, it specifies the mail
    transport program. Default is **/usr/lib/sendmail**. If this
    parameter is changed, the sbatchd must be restarted so that the new
    value will be picked up (see **badmin**(1)).

**LSB_MAILTO**

:   This parameter is optional. If defined, it specifies the mail
    address format. Lava uses mails to send job output or error message
    to users. The possible formats are:

        !U                         (the default. Send to user's account name)
        !U@!H                  (Send to user@job_submission_hostname)
        !U@company_name.com    (Send to user@company_name.com)

If this parameter is changed, the sbatchd must be restarted so that the
new value will be picked up (see **badmin**(1)).

**LSB_CRDIR**

:   This parameter specifies the directory containing the
    **chkpnt/restart** utility programs that the sbatchd daemon is to
    use to checkpoint or restart a job. This parameter applies to
    platforms that have system support checkpoint/restart by means of
    the **chkpnt/restart** utility programs. Currently, this parameter
    should only be defined on convex systems. If **LSB_CRDIR** is not
    defined, then the default is **/bin**.

**LSB_MBD_PORT/LSB_SBD_PORT**

:   This defines the TCP port number that mbatchd/sbatchd uses to serve
    all requests. If **LSB_MBD_PORT**/**LSB_SBD_PORT** is defined,
    mbatchd/sbatchd will not look into system services database for port
    numbers. Any unused port numbers can be specified.

**LSB_DEBUG**

:   If this is defined, Lava will run in single user mode. In this mode,
    security checking are not performed and therefore Lava daemons
    should not run as root. When **LSB_DEBUG** is defined, Lava will not
    look into system services database for port numbers. Instead, it
    uses port number 40000 for mbatchd and port number 40001 for sbatchd
    unless **LSB_MBD_PORT/LSB_SBD_PORT** is defined in the file
    **lsf.conf**.

# OPTIONS

**-h**

:   Print command usage to stderr and exit.

**-V**

:   Print Lava release version to stderr and exit.

**-C**

:   This option applies to mbatchd only. If specified, mbatchd will do
    the syntax checking on the Lava configuration files and prints
    verbose messages to stdout. After the checking, mbatchd will exit.
    Mbatchd with **-C** option does not have to be run on the master
    host (as returned by **lsid**(1)). You must never start mbatchd
    manually unless the **-C** option is used.

**-d *env_dir***

:   Read **lsf.conf** file from the directory *env_dir,* rather than
    from the default directory **/etc**, or from the directory
    **LSF_ENVDIR** that is set in the environment variable.

**-***debug_level*

:   Debug mode level. Possible values are either 1 or 2. When debug mode
    is set, the daemons are run in debug mode and can be started by a
    normal user (non-root). If debug level is 1, sbatchd will go to
    background when started. If debug level is 2, sbatchd will stay in
    foreground. mbatchd is always started by sbatchd with the same
    options, and therefore has the same debug level. If Lava daemons are
    running in debug mode, **LSB_DEBUG** must be defined in the file
    **lsf.conf** in order for Lava commands to talk with the daemons.

# QUEUES AND LOGS

Visible to users are a number of job queues to which jobs can be
submitted. Job queues are defined by the Lava administrator in the
cluster configuration file **lsb.queues**. See **lsb.queues**(5) for a
description of the queue configuration.

Two types of log files are maintained by mbatchd: event log and job log.
The files are named **lsb.events** and **lsb.acct**, respectively. See
**lsb.events**(5) and **lsb.acct**(5) for the description.

# ERROR REPORTING

mbatchd and sbatchd have no controlling tty. Serious errors are mailed
to the Lava administrator. Less serious errors are sent to syslog with
log level **LOG_ERR**, or written to the file
**LSF_LOGDIR/mbatchd.log.**\<*hostname*\> or
**LSF_LOGDIR/sbatchd.log.**\<*hostname*\>, if **LSF_LOGDIR** is defined
in the file **lsf.conf**.

# FILES

**LSB_CONFDIR/\<***clustername***\>/configdir/lsb.params**

:   

**LSB_CONFDIR/\<***clustername***\>/configdir/lsb.queues**

:   

**LSB_CONFDIR/\<***clustername***\>/configdir/lsb.hosts**

:   

**LSB_CONFDIR/\<***clustername***\>/configdir/lsb.users**

:   

**LSB_SHAREDIR/\<***clustername***\>/logdir/lsb.events**\[.?\]

:   

**LSB_SHAREDIR/\<***clustername***\>/logdir/lsb.acct**

:   

# SEE ALSO

**lsf.conf**(5), **lsf.cluster**(5), **lsb.queues**(5),
**lsb.events**(5), **lsb.acct**(5), **bsub**(1), **lsid**(1),
**lim**(8), **rc.local**(8), **services**(8)
