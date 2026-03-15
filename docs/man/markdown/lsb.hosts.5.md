\

# NAME

**lsb.hosts**

## Overview

The lsb.hosts file contains host-related configuration information for
the server hosts in the cluster. This file is optional. All sections are
optional.

## Contents

> · Host Section
>
> · HostGroup Section

# Host Section

# Description

Optional. Defines the hosts, host types, and host models used as server
hosts, and contains per-host configuration information. If this section
is not configured, Lava uses all hosts in the cluster as server hosts.

Each host, host model or host type can be configured to:

> · Limit the maximum number of jobs run in total
>
> · Limit the maximum number of jobs run by each user
>
> · Run jobs only under specific load conditions
>
> · Run jobs only under specific time windows

The entries in a line for a host override the entries in a line for its
model or type.

When you modify the cluster by adding or removing hosts, no changes are
made to lsb.hosts. This does not affect the default configuration, but
if hosts, host models, or host types are specified in this file, you
should check this file whenever you make changes to the cluster and
update it manually if necessary.

# Host Section Structure

The first line consists of keywords identifying the load indices that
you wish to configure on a per-host basis. The keyword HOST_NAME must be
used; the others are optional. Load indices not listed on the keyword
line do not affect scheduling decisions.

Each subsequent line describes the configuration information for one
host, host model or host type. Each line must contain one entry for each
keyword. Use empty parentheses ( ) or a dash (-) to specify the default
value for an entry.

# HOST_NAME

## 

Required. Specify the name, model, or type of a host, or the keyword
default.

## host name

The name of a host defined in lsf.cluster.*cluster_name*. The official
host name returned by **gethostbyname(3)**.

## host model

A host model defined in lsf.shared.

## host type

A host type defined in lsf.shared.

## default

The reserved host name default indicates all hosts in the cluster not
otherwise referenced in the section (by name or by listing its model or
type).

# DISPATCH_WINDOW

## Description

The time windows in which jobs from this host, host model, or host type
are dispatched. Once dispatched, jobs are no longer affected by the
dispatch window.

## Default

Undefined (always open).

# JL/U

## Description

Per-user job slot limit for the host. Maximum number of job slots that
each user can use on this host.

## Example

HOST_NAME JL/U\
hostA 2

## Default

Unlimited

# MIG

## Description

Enables job migration and specifies the migration threshold, in minutes.

If a checkpointable or rerunnable job dispatched to the host is
suspended (SSUSP state) for longer than the specified number of minutes,
the job is migrated. A value of 0 specifies that a suspended job should
be migrated immediately.

If a migration threshold is defined at both host and queue levels, the
lower threshold is used.

## Example

HOST_NAME MIG\
hostA 10

In this example, the migration threshold is 10 minutes.

## Default

Undefined (no migration)

# MXJ

## Description

The number of job slots on the host.

Use \"!\" to make the number of job slots equal to the number of CPUs on
a host.

Use \"!\" for the reserved host name default to make the number of
jobslots equal to the number of CPUs on all hosts in a cluster not
defined in the host section of the lsb.hosts file.

By default, the number of running and suspended jobs on a host cannot
exceed the number of job slots. If preemptive scheduling is used, the
suspended jobs are not counted as using a job slot.

On multiprocessor hosts, to fully use the CPU resource, make the number
of job slots equal to or greater than the number of processors.

## Default

Unlimited

# load_index

## Syntax

*load_index*\
loadSched\[**/***loadStop*\]

Specify io, it, ls, mem, pg, r15s, r1m, r15m, swp, tmp, ut, or a non-
shared custom external load index as a column. Specify multiple columns
to configure thresholds for multiple load indices.

## Description

Scheduling and suspending thresholds for dynamic load indices supported
by LIM, including external load indices.

Each load index column must contain either the default entry or two
numbers separated by a slash \`/\', with no white space. The first
number is the scheduling threshold for the load index; the second number
is the suspending threshold.

Queue-level scheduling and suspending thresholds are defined in
lsb.queues. If both files specify thresholds for an index, those that
apply are the most restrictive ones.

## Example

HOST_NAME mem swp\
hostA 100/10 200/30

This example translates into a loadSched condition of

mem\>=100 && swp\>=200

and a loadStop condition of

mem \< 10 \|\| swp \< 30

## Default

Undefined

# Example of a Host Section

Begin Host\
HOST_NAME MXJ JL/U r1m pg DISPATCH_WINDOW\
hostA 1 - 0.6/1.6 10/20 (5:19:00-1:8:30 20:00-8:30)\
SUNSOL 1 - 0.5/2.5 - 23:00-8:00\
default 2 1 0.6/1.6 20/40 ()\
End Host

SUNSOL is a host type defined in lsf.shared. This example Host section
configures one host and one host type explicitly and configures default
values for all other load-sharing hosts.

HostA runs one batch job at a time. A job will only be started on hostA
if the r1m index is below 0.6 and the pg index is below 10; the running
job is stopped if the r1m index goes above 1.6 or the pg index goes
above 20. HostA only accepts batch jobs from 19:00 on Friday evening
until 8:30 Monday morning and overnight from 20:00 to 8:30 on all other
days.

For hosts of type SUNSOL, the pg index does not have host-specific
thresholds and such hosts are only available overnight from 23:00 to
8:00.

The entry with host name default applies to each of the other hosts in
the Lava cluster. Each host can run up to two jobs at the same time,
with at most one job from each user. These hosts are available to run
jobs at all times. Jobs may be started if the r1m index is below 0.6 and
the pg index is below 20, and a job from the lowest priority queue is
suspended if r1m goes above 1.6 or pg goes above 40.

# HostGroup Section

# Description

Optional. Defines host groups.

The name of the host group can then be used in other host group, host
partition, and queue definitions, as well as on the command line.
Specifying the name of a host group has exactly the same effect as
listing the names of all the hosts in the group.

# Structure

Host groups are specified in the same format as user groups in
lsb.users.

The first line consists of two mandatory keywords, GROUP_NAME and
GROUP_MEMBER. Subsequent lines name a group and list its membership.

The sum of host groups and host partitions cannot be more than
MAX_GROUPS (see lsbatch.h for details).

# GROUP_NAME

## Description

An alphanumeric string representing the name of the host group.

You cannot use the reserved name all, and group names must not conflict
with host names.

# GROUP_MEMBER

## Description

A space-separated list of host names or previously defined host group
names, enclosed in parentheses.

The names of hosts and host groups can appear on multiple lines because
hosts can belong to multiple groups. The reserved name all specifies all
hosts in the cluster. Use an exclamation mark (!) to specify that the
group membership should be retrieved via egroup. Use a tilde (\~) to
exclude specified hosts or host groups from the list.

# Examples of HostGroup Sections

## Example 1

Begin HostGroup\
GROUP_NAME GROUP_MEMBER\
groupA (hostA hostD)\
groupB (hostF groupA hostK)\
groupC (!)\
End HostGroup

This example defines three host groups:

> · groupA includes hostsA and hostD.
>
> · groupB includes hostsF and hostK, along with all hosts in groupA.
>
> · the group membership of groupC will be retrieved via egroup.

## Example 2

Begin HostGroup\
GROUP_NAME GROUP_MEMBER\
groupA (all)\
groupB (groupA \~hostA \~hostB)\
groupC (hostX hostY hostZ)\
groupD (groupC \~hostX)\
groupE (all \~groupC \~hostB)\
groupF (hostF groupC hostK)\
End HostGroup

This example defines the following host groups:

> · groupA contains all hosts in the cluster.
>
> · groupB contains all the hosts in the cluster except for hostA and
> hostB.
>
> · groupC contains only hostX, hostY, and hostZ.
>
> · groupD contains the hosts in groupC except for hostX. Note that
> hostX must be a member of host group groupC to be excluded from
> groupD.
>
> · groupE contains all hosts in the cluster excluding the hosts in
> groupC and hostB.
>
> · groupF contains hostF, hostK, and the 3 hosts in groupC.
