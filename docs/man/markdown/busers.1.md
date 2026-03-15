\

# NAME

**busers** - displays information about users and user groups

# SYNOPSIS

**busers **\[*user_name *\... \| *user_group* \... \| **all**\]** ** .PP
**busers **\[**-h** \| **-V**\]

# DESCRIPTION

Displays information about users and user groups.

By default, displays information about the user who runs the command.

# OPTIONS

*user_name* \... \|** ***user_group *\... \|** all**

:   

    Displays information about the specified users or user groups, or
    about all users if you specify all.

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

# OUTPUT

A listing of the users and user groups is displayed with the following
fields:

## USER/GROUP

The name of the user or user group.

## JL/P

The maximum number of job slots that can be processed simultaneously for
the specified users on each processor. These job slots are used by
running and suspended jobs or by pending jobs which have jobs slots
reserved for them. This job limit is configured per processor so that
multiprocessor hosts have more job slots. If the dash character (-) is
displayed, there is no limit. JL/P is defined in the Lava configuration
file lsb.users(5).

## MAX

The maximum number of job slots that can be processed concurrently for
the specified users\' jobs. These job slots are used by running and
suspended jobs or by pending jobs which have job slots reserved for
them. If the character \`-\' is displayed, there is no limit. MAX is
defined by the MAX_JOBS parameter in the configuration file
lsb.users(5).

## NJOBS

The current number of job slots used by specified users\' jobs. A
parallel job that is pending is counted as *n* job slots for it will use
*n* job slots in the queue when it is dispatched.

## PEND

The number of pending job slots used by jobs of the specified users.

## RUN

The number of job slots used by running jobs of the specified users.

## SSUSP

The number of job slots used by the system-suspended jobs of the
specified users.

## USUSP

The number of job slots used by user-suspended jobs of the specified
users.

## RSV

The number of job slots used by pending jobs of the specified users
which have job slots reserved for them.

# SEE ALSO

bugroup(1), lsb.users(5), lsb.queues(5)
