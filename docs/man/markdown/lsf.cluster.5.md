\

# NAME

**lsf.cluster**

## Overview

This is the cluster configuration file. There is one for each cluster,
called lsf.cluster.*cluster_name*. The *cluster_name* suffix is the
cluster name defined in the Cluster section of lsf.shared.

## Contents

> · Parameters Section
>
> · ClusterAdmins Section
>
> · Host Section
>
> · ResourceMap Section
>
> · RemoteClusters Section

## 

# Parameters Section

(Optional) This section contains miscellaneous parameters for the LIM.

# ADJUST_DURATION

## Syntax

**ADJUST_DURATION =** *integer* .SS Description

Integer reflecting a multiple of EXINTERVAL that controls the time
period during which load adjustment is in effect.

The **lsplace**(**1**) and **lsloadadj**(**1**) commands artificially
raise the load on a selected host. This increase in load decays linearly
to 0 over time.

## Default

3

# ELIM_POLL_INTERVAL

## Syntax

**ELIM_POLL_INTERVAL =** *time_in_seconds* .SS Description

Time interval, in seconds, in which the LIM daemon samples load
information

This parameter only needs to be set if an ELIM is being used to report
information more frequently than every 5 seconds.

## Default

5 seconds

# ELIMARGS

## Syntax

**ELIMARGS =** *cmd_line_args* .SS Description

Specifies any necessary command-line arguments for the external LIM on
startup

This parameter is ignored if no external load indices are configured.

## Default

None

# EXINTERVAL

## Syntax

**EXINTERVAL =** *time_in_seconds* .SS Description

Time interval, in seconds, at which the LIM daemons exchange load
information

On extremely busy hosts or networks, or in clusters with a large number
of hosts, load may interfere with the periodic communication between LIM
daemons. Setting EXINTERVAL to a longer interval can reduce network load
and slightly improve reliability, at the cost of slower reaction to
dynamic load changes.

## Default

15 seconds

# HOST_INACTIVITY_LIMIT

## Syntax

**HOST_INACTIVITY_LIMIT =** *integer* .SS Description

Integer reflecting a multiple of EXINTERVAL that controls the maximum
time a slave LIM will take to send its load information to the master
LIM as well as the frequency at which the master LIM will send a
heartbeat message to its slaves.

A slave LIM can send its load information any time from EXINTERVAL to
(HOST_INACTIVITY_LIMIT-2)\*EXINTERVAL seconds. A master LIM will send a
master announce to each host at least every
EXINTERVAL\*HOST_INACTIVITY_LIMIT seconds.

## Default

5

# LSF_ELIM_BLOCKTIME

## Syntax

**LSF_ELIM_BLOCKTIME=***seconds* .SS Description

UNIX only.

Maximum amount of time LIM waits for a load update string from the ELIM
if it is not immediately available.

Use this parameter to add fault-tolerance to LIM when using ELIMs. If
there is an error in the ELIM or some situation arises that the ELIM
cannot send the entire load update string to the LIM, LIM will not wait
indefinitely for load information from ELIM. After the time period
specified by LSF_ELIM_BLOCKTIME, the LIM writes the last string sent by
ELIM in its log file (lim.log.*host_name*) and restarts the ELIM.

For example, if LIM is expecting 3 name-value-pairs, such as:

3 tmp2 49.5 nio 367.0 licenses 3

If after the time period specified by LSF_ELIM_BLOCKTIME LIM has only
received the following:

3 tmp2 47.5

LIM writes whatever was received last (3 tmp2 47.5) in the log file and
restarts the ELIM.

## Valid Values

Non-negative integers

A value of 0 indicates that LIM will not wait at all to receive
information from ELIM\--it expects to receive the entire load string at
once.

So, if for example, your ELIM writes value-pairs with 1 second intervals
between them, and you collect 12 load indices, you need to allow at
least 12 seconds for the ELIM to complete writing an entire load string.
So you would define LSF_ELIM_BLOCKTIME to 15 or 20 seconds for example.

## Default

Undefined\--LIM waits indefinitely to receive load information from ELIM

# LSF_ELIM_DEBUG

## Syntax

**LSF_ELIM_DEBUG=y** .SS Description

UNIX only.

This parameter is useful to view which load information an ELIM is
collecting and to add fault-tolerance to LIM.

When this parameter is set to y:

> · All load information received by LIM from the ELIM is logged in the
> LIM log file (lim.log.*host_name*).
>
> · If LSF_ELIM_BLOCKTIME is undefined, whenever there is an error in
> the ELIM or some situation arises that the ELIM cannot send the entire
> load update string to the LIM, LIM does not wait indefinitely for load
> information from ELIM. After 2 seconds, the LIM restarts the ELIM.

> For example, LIM is expecting 3 name-value-pairs, such as:

> 3 tmp2 47.5 nio 344.0 licenses 5

> However, LIM only receives the following from ELIM:

> 3 tmp2 47.5

> LIM waits 2 seconds after the last value is received and if no more
> information is received, LIM restarts the ELIM.

> If LSF_ELIM_BLOCKTIME is defined, the LIM waits for the specified
> amount of time before restarting the ELIM instead of the 2 seconds.

## Default

Undefined\--if LSF_ELIM_DEBUG is undefined, load information sent from
ELIM to LIM is not logged. In addition, if LSF_ELIM_BLOCKTIME is
undefined, LIM waits indefinitely to receive load information from ELIM.

## See Also

LSF_ELIM_BLOCKTIME to configure how long LIM waits before restarting the
ELIM, use the parameter LSF_ELIM_BLOCKTIME.

LSF_ELIM_RESTARTS to limit how many times the ELIM can be restarted

# LSF_ELIM_RESTARTS

## Syntax

**LSF_ELIM_RESTARTS=***integer* .SS Description

UNIX only.

LSF_ELIM_BLOCKTIME or LSF_ELIM_DEBUG must be defined in conjunction with
LSF_ELIM_RESTARTS.

Defines the maximum number of times an ELIM can be restarted.

When this parameter is defined:

> · If LIM attempts to retrieve load information from the ELIM and there
> is an error such as an invalid value for example, LIM restarts the
> ELIM.

If the error is consistent and LIM keeps restarting the ELIM,
LSF_ELIM_RESTARTS limits how many times the ELIM can be restarted to
prevent an ongoing loop.

## Valid Values

Non-negative integers

## Default

Undefined; the number of ELIM restarts is unlimited

## See Also

LSF_ELIM_BLOCKTIME, LSF_ELIM_DEBUG

# MASTER_INACTIVITY_LIMIT

## Syntax

**MASTER_INACTIVITY_LIMIT =** *integer* .SS Description

An integer reflecting a multiple of EXINTERVAL. A slave will attempt to
become master if it does not hear from the previous master after
(HOST_INACTIVITY_LIMIT
+*host_number*\*MASTER_INACTIVITY_LIMIT)\*EXINTERVAL seconds, where
*host_number* is the position of the host in lsf.cluster.*cluster_name*.

The master host is *host_number* 0.

## Default

2

# PROBE_TIMEOUT

## Syntax

**PROBE_TIMEOUT =** *time_in_seconds* .SS Description

Specifies the timeout in seconds to be used for the connect(2) system
call

Before taking over as the master, a slave LIM will try to connect to the
last known master via TCP.

## Default

2 seconds

# RETRY_LIMIT

## Syntax

**RETRY_LIMIT =** *integer* .SS Description

Integer reflecting a multiple of EXINTERVAL that controls the number of
retries a master or slave LIM makes before assuming that the slave or
master is unavailable.

If the master does not hear from a slave for HOST_INACTIVITY_LIMIT
exchange intervals, it will actively poll the slave for RETRY_LIMIT
exchange intervals before it will declare the slave as unavailable. If a
slave does not hear from the master for HOST_INACTIVITY_LIMIT exchange
intervals, it will actively poll the master for RETRY_LIMIT intervals
before assuming that the master is down.

## Default

2

# ClusterAdmins Section

(Optional) The ClusterAdmins section defines the Lava administrators for
the cluster. The only keyword is ADMINISTRATORS.

If the ClusterAdmins section is not present, the default Lava
administrator is root. Using root as the primary Lava administrator is
not recommended.

# ADMINISTRATORS

## Syntax

**ADMINISTRATORS =** *administrator_name \...* .SS Description

Specify Linux user and user group names.

The first administrator of the expanded list is considered the primary
Lava administrator. The primary administrator is the owner of the Lava
configuration files, as well as the working files under
LSB_SHAREDIR/*cluster_name*. If the primary administrator is changed,
make sure the owner of the configuration files and the files under
LSB_SHAREDIR/*cluster_name* are changed as well.

Administrators other than the primary Lava administrator have the same
privileges as the primary Lava administrator except that they do not
have permission to change Lava configuration files. They can perform
clusterwide operations on jobs, queues, or hosts in the system.

For flexibility, each cluster may have its own Lava administrators,
identified by a user name, although the same administrators can be
responsible for several clusters.

Use the **-l** option of the **lsclusters**(**1**) command to display
all of the administrators within a cluster.

## Compatibility

For backwards compatibility, ClusterManager and Manager are synonyms for
ClusterAdmins and ADMINISTRATORS respectively. It is possible to have
both sections present in the same lsf.cluster.*cluster_name* file to
allow daemons from different Lava versions to share the same file.

## Default

lsfadmin

## Example

The following gives an example of a cluster with three Lava
administrators. The user listed first, user2, is the primary
administrator. The user group lsfgrp and the user user7 are secondary
administrators.

Begin ClusterAdmins\
ADMINISTRATORS = user2 lsfgrp user7\
End ClusterAdmins

# Host Section

The Host section is the last section in lsf.cluster.*cluster_name* and
is the only required section. It lists all the hosts in the cluster and
gives configuration information for each host.

The order in which the hosts are listed in this section is important.
The LIM on the first host listed becomes the master LIM if this host is
up; otherwise, that on the second becomes the master if its host is up,
and so on.

Since the master LIM makes all placement decisions for the cluster, it
should be on a fast machine. Also, to avoid the delays involved in
switching masters if the first machine goes down, the master should be
on a reliable machine. It is desirable to arrange the list such that the
first few hosts in the list are always in the same subnet. This avoids a
situation where the second host takes over as master when there are
communication problems between subnets.

Configuration information is of two types:

> · Some fields in a host entry simply describe the machine and its
> configuration.
>
> · Other fields set thresholds for various resources.

# Descriptive Fields

The following fields are required in the Host section:

> · HOSTNAME
>
> · RESOURCES
>
> · type
>
> · server

The following fields are optional:

> · model
>
> · nd
>
> · RUNWINDOW
>
> · REXPRI

# HOSTNAME

## Description

Official name of the host as returned by **hostname**(1)

The name must be listed in lsf.shared as belonging to this cluster.

# model

## Description

Host model

The name must be defined in the HostModel secton of lsf.shared. This
determines the CPU speed scaling factor applied in load and placement
calculations.

If you leave the model or type column blank or enter the ! keyword, you
are indicating that the host model or type is to be automatically
detected by the LIM running on the host.

# nd

## Description

Number of local disks

This corresponds to the ndisks static resource. On most host types, Lava
automatically determines the number of disks, and the nd parameter is
ignored.

nd should only count local disks with file systems on them. Do not count
either disks used only for swapping or disks mounted with NFS.

## Default

The number of disks determined by the LIM, or 1 if the LIM cannot
determine this

# RESOURCES

## Description

The static Boolean resources available on this host

The resource names are strings defined in the Resource section of
lsf.shared. You may list any number of resources, enclosed in
parentheses and separated by blanks or tabs: for example:

(fs frame hpux)

Optionally, you can specify a dedicated resource by prefixing the
resource with an exclamation mark (!). A host with dedicated resources
is not selected by LIM for a job unless a dedicated resource name is
explicitly specified in the resource requirements for the job.

# REXPRI

## Description

(UNIX ONLY) Default execution priority for interactive remote jobs run
under the RES

The range is from -20 to 20. REXPRI corresponds to the BSD-style nice
value used for remote jobs. For hosts with System V-style nice values
with the range 0 - 39, a REXPRI of -20 corresponds to a nice value of 0,
and +20 corresponds to 39. Higher values of REXPRI correspond to lower
execution priority; -20 gives the highest priority, 0 is the default
priority for login sessions, and +20 is the lowest priority.

## Default

0

# RUNWINDOW

## Description

Dispatch window during this host is accepts remote interactive tasks

When the host is not available for remote execution, the host status is
lockW (locked by run window). LIM does not schedule interactive tasks on
hosts locked by dispatch windows. Note that run windows only apply to
interactive tasks placed by LIM. Lava Batch uses its own (optional) host
dispatch windows to control batch job processing on batch server hosts.

## Format

A dispatch window consists of one or more time windows in the format
*begin_time*-*end_time*. No blanks can separate *begin_time* and
*end_time*. Time is specified in the form \[*day*:\]*hour*\[:*minute*\].
If only one field is specified, Lava assumes it is an *hour*. Two fields
are assumed to be *hour*:*minute*. Use blanks to separate time windows.

## Default

Always accept remote jobs

# server

## Description

Indicates whether the host can receive jobs from other hosts

Specify 1 if the host can receive jobs from other hosts; specify 0
otherwise. If server is set to 0, the host is an Lava client. Client
hosts do not run the Lava daemons. Client hosts can submit interactive
and batch jobs to an Lava cluster, but they cannot execute jobs sent
from other hosts.

## Default

1

# type

## Description

Host type as defined in the HostType section of lsf.shared

The strings used for host types are determined by the system
administrator: for example, SUNSOL, DEC, or HPPA. The host type is used
to identify binary-compatible hosts.

The host type is used as the default resource requirement. That is, if
no resource requirement is specified in a placement request, the task is
run on a host of the same type as the sending host.

Often one host type can be used for many machine models. For example,
the host type name SUNSOL6 might be used for any computer with a SPARC
processor running SunOS 6. This would include many Sun models and quite
a few from other vendors as well.

If you leave the model or type column blank or enter the ! keyword, you
are indicating that the host model or type is to be automatically
detected by the LIM running on the host.

# Threshold Fields

The LIM uses these thresholds in determining whether to place remote
jobs on a host. If one or more Lava load indices exceeds the
corresponding threshold (too many users, not enough swap space, etc.),
then the host is regarded as busy, and LIM will not recommend jobs to
that host.

The CPU run queue length threshold values (r15s, r1m, and r15m) are
taken as effective queue lengths as reported by **lsload -E**.

All of these fields are optional; you only need to configure thresholds
for load indices that you wish to use for determining whether hosts are
busy. Fields that are not configured are not considered when determining
host status. The keywords for the threshold fields are not case
sensitive.

Thresholds can be set for any of the following:

> · The built-in Lava load indexes (r15s, r1m, r15m, ut, pg, it, io, ls,
> swp, mem, tmp)
>
> · External load indexes defined in the Resource section of lsf.shared

# Example of a Host Section

This example Host section contains descriptive and threshold information
for two hosts:

Begin Host\
HOSTNAME model type server r1m pg tmp RESOURCES RUNWINDOW\
hostA SparcIPC Sparc 1 3.5 15 0 (sunos frame ()\
hostD Sparc10 Sparc 1 3.5 15 0 (sunos) (5:18:30-1:8:30)\
End Host

# ResourceMap Section

The ResourceMap section defines shared resources in your cluster. This
section specifies the mapping between shared resources and their sharing
hosts. When you define resources in the Resources section of lsf.shared,
there is no distinction between a shared and non-shared resource. By
default, all resources are not shared and are local to each host. By
defining the ResourceMap section, you can define resources that are
shared by all hosts in the cluster or define resources that are shared
by only some of the hosts in the cluster.

This section must appear after the Host section of
lsf.cluster.*cluster_name*, because it has a dependency on host names
defined in the Host section. The following parameters must be defined in
the ResourceMap section:

# ResourceMap Section Structure

The first line consists of the keywords RESOURCENAME and LOCATION.
Subsequent lines describe the hosts that are associated with each
configured resource.

# LOCATION

## Description

Defines the hosts that share the resource

For a static resource, you must define a value here as well. Do not
define a value for a dynamic resource.

*instance* is a list of host names that share an instance of the
resource. The reserved words all, others, and default can be specified
for the instance:

> · all\--Indicates that there is only one instance of the resource in
> the whole cluster and that this resource is shared by all of the hosts

> Use the not operator (\~) to exclude hosts from the all specification.
> For example:

> (2@\[all \~host3 \~host4\])

> means that 2 units of the resource are shared by all server hosts in
> the cluster made up of host1 host2 \... host*n*, except for host3 and
> host4. This is useful if you have a large cluster but only want to
> exclude a few hosts.

> The parentheses are required in the specification. The not operator
> can only be used with the all keyword. It is not valid with the
> keywords others and default.

> · others\--Indicates that the rest of the server hosts not explicitly
> listed in the LOCATION field comprise one instance of the resource

> For example:

> 2@\[host1\] 4@\[others\]

> indicates that there are 2 units of the resource on apple and 4 units
> of the resource shared by all other hosts.

> · default\--Indicates an instance of a resource on each host in the
> cluster

> This specifies a special case where the resource is in effect not
> shared and is local to every host. default means at each host.
> Normally, you should not need to use default, because by default all
> resources are local to each host. You might want to use ResourceMap
> for a non-shared static resource if you need to specify different
> values for the resource on different hosts.

# RESOURCENAME

## Description

Name of the resource

This resource name must be defined in the Resource section of
lsf.shared. You must specify at least a name and description for the
resource, using the keywords RESOURCENAME and DESCRIPTION.

> · A resource name cannot begin with a number.
>
> · A resource name cannot contain any of the following characters:
>
> > : . ( ) \[ + - \* / ! & \| \< \> @ =

· A resource name cannot be any of the following reserved names:

> cpu cpuf io logins ls idle maxmem maxswp maxtmp type model status it
> mem ncpus ndisks pg r15m r15s r1m swap swp tmp ut

· Resource names are case sensitive

· Resource names can be up to 29 characters in length

# Example of a ResourceMap Section

Begin ResourceMap\
RESOURCENAME LOCATION\
verilog \[5@all\]\
local (\[host1 host2\] \[others\])\
End ResourceMap

The resource verilog must already be defined in the RESOURCE section of
the lsf.shared file. It is a static numeric resource shared by all
hosts. The value for verilog is 5. The resource local is a numeric
shared resource that contains two instances in the cluster. The first
instance is shared by two machines, host1 and host2. The second instance
is shared by all other hosts.

Resources defined in the ResourceMap section can be viewed by using the
**-s** option of the **lshosts** (for static resource) and **lsload**
(for dynamic resource) commands.
