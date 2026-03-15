\

# NAME

**bbot** - moves a pending job relative to the last job in the queue

# SYNOPSIS

**bbot** *job_ID* \| **\"***job_ID***\[***index_list***\]\"**
\[*position*\]

**bbot** \[**-h** \| **-V**\]

# DESCRIPTION

Changes the queue position of a pending job, or a pending job array
element, to affect the order in which jobs are considered for dispatch.

By default, Lava dispatches jobs in a queue in the order of arrival
(that is, first-come-first-served), subject to availability of suitable
server hosts.

The bbot command allows users and the Lava administrator to manually
change the order in which jobs are considered for dispatch. Users can
only operate on their own jobs, whereas the Lava administrator can
operate on any user\'s jobs. Users can only change the relative position
of their own jobs.

If invoked by the Lava administrator, **bbot** moves the selected job
after the last job with the same priority submitted to the queue. The
positions of all users\' jobs in the queue can be changed by the Lava
administrator.

If invoked by a regular user, **bbot** moves the selected job after the
last job with the same priority submitted by the user to the queue.

Pending jobs are displayed by bjobs in the order in which they will be
considered for dispatch.

# OPTIONS

*job_ID* \| **\"***job_ID***\[***index_list***\]\"**

:   

    Required. Job ID of the job or job array on which to operate.

> For a job array, the index list, the square brackets, and the
> quotation marks are required. An index list is used to operate on a
> job array. The index list is a comma separated list whose elements
> have the syntax *start_index*\[-*end_index*\[**:***step*\]\] where
> *start_index*, *end_index* and *step* are positive integers. If the
> step is omitted, a step of one is assumed. The job array index starts
> at one. The maximum job array index is 1000. All jobs in the array
> share the same job_ID and parameters. Each element of the array is
> distinguished by its array index.

*position*

:   

    Optional. The *position* argument can be specified to indicate where
    in the queue the job is to be placed. *position* is a positive
    number that indicates the target position of the job from the end of
    the queue. The positions are relative to only the applicable jobs in
    the queue, depending on whether the invoker is a regular user or the
    Lava administrator. The default value of 1 means the position is
    after all other jobs with the same priority.

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

# SEE ALSO

bjobs(1), bswitch(1), btop(1)
