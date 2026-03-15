\

# NAME

**lsb.users**

## Overview

The lsb.users file is used to configure user groups, job slot limits for
users and user groups.

This file is optional.

The lsb.users file is stored in the directory
LSB_CONFDIR/*cluster_name*/configdir, where LSB_CONFDIR is defined in
lsf.conf.

## Contents

> · UserGroup Section
>
> · User Section
>
> · UserMap Section

# UserGroup Section

Optional. Defines user groups.

The name of the user group can be used in other user group and queue
definitions, as well as on the command line. Specifying the name of a
user group has exactly the same effect as listing the names of all users
in the group.

The total number of user groups cannot be more than MAX_GROUPS in
lsbatch.h.

# Structure

The first line consists of two mandatory keywords, GROUP_NAME and
GROUP_MEMBER. The USER_SHARES keyword is optional. Subsequent lines name
a group and list its membership and optionally its share assignments.

Each line must contain one entry for each keyword. Use empty parentheses
() or a dash - to specify the default value for an entry.

# Example of a UserGroup Section

Begin UserGroup\
GROUP_NAME GROUP_MEMBER\
groupA (user1 user2 user3 user4)\
groupB (groupA user5)\
groupC (!)\
End UserGroup

# GROUP_NAME

An alphanumeric string representing the user group name. You cannot use
the reserved name all or a / in a group name, and group names must not
conflict with user names.

# GROUP_MEMBER

A list of user names or user group names that belong to the group,
enclosed in parentheses and separated by spaces. Group names must not
conflict with user names.

User and user group names can appear on multiple lines, because users
can belong to multiple groups.

User groups may be defined recursively but must not create a loop.

## Syntax

**(***user_name* \| *user_group* \...**)** \| **(all)** \| **(!)** .PP
Specify the following, all enclosed in parentheses:

> · *user_name* \| *user_group * .RE
>
> > User and user group names, separated by spaces. User names must be
> > valid login names.
>
> > User group names can be Lava user groups defined previously in this
> > section, or LINUX groups.
>
> > · **all** .RE
> >
> > > The reserved name all specifies all users in the cluster.
> >
> > > · **!** .RE
> > >
> > > > The exclamation mark ! specifies that the group membership
> > > > should be retrieved via egroup.

# User Section

Optional. If this section is not defined, all users and user groups can
run an unlimited number of jobs in the cluster.

This section defines the maximum number of jobs a user or user group can
run concurrently in the cluster. This is to avoid situations in which a
user occupies all or most of the system resources while other users\'
jobs are waiting.

# Structure

All three fields are mandatory: USER_NAME, MAX_JOBS, JL/P.

You must specify a dash (-) to indicate the default value (unlimited) if
a user or user group is specified. Fields cannot be left blank.

# Example of a User Section

Begin User\
USER_NAME MAX_JOBS JL/P\
user1 10 -\
user2 4 1\
user3 - 2\
groupA@ 10 1\
default 6 1\
End User

# USER_NAME

User or user group for which job slot limits are defined.

Use the reserved user name default to specify a job slot limit that
applies to each user and user group not explicitly named. Since the
limit specified with the keyword default applies to user groups also,
ensure you select a limit that is high enough, or explicitly define
limits for user groups.

User group names can be the Lava user groups defined previously, and/or
LINUX user groups.

Job slot limits apply to a group as a whole. Append @ to a group name to
make the job slot limits apply individually to each user in the group.
If a group contains a subgroup, the job slot limit also applies to each
member in the subgroup recursively.

# MAX_JOBS

Per-user or per-group job slot limit for the cluster. Total number of
job slots that each user or user group can use in the cluster.

# JL/P

Per processor job slot limit per user or user group.

Total number of job slots that each user or user group can use per
processor. This job slot limit is configured per processor so that
multiprocessor hosts will automatically run more jobs.

This number can be a fraction such as 0.5, so that it can also serve as
a per-host limit. This number is rounded up to the nearest integer equal
to or greater than the total job slot limits for a host. For example, if
JL/P is 0.5, on a 4-CPU multiprocessor host, the user can only use up to
2 job slots at any time. On a uniprocessor machine, the user can use 1
job slot.

# SEE ALSO

lsf.cluster(5), lsf.conf(5), lsb.params(5), lsb.hosts(5), lsb.queues(5),
bhosts(1), bmgroup(1), busers(1), bugroup(1), bqueues(1), bsub(1),
bchkpnt(1), lsid(1), nice(1), getgrnam(3), mbatchd(8), badmin(8)
