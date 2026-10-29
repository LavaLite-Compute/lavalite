---
title: BSERVICES
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bservices - show, start, and delete service instances

# SYNOPSIS

**bservices** [**-a**] [**-l**] [*name*...]

**bservices** **-s** *name* [**-t** [*hours*:]*minutes*]

**bservices** **-d** *job_id*|*endpoint*

# DESCRIPTION

Show the services defined in **llb.services** and their instances,
start an instance, or delete one. Without **-s** or **-d** the command
only shows, like **bqueues** and **bhosts**.

Each service instance runs as a job in its configured queue.
Scheduling depends on queue availability, job limits, and available
host resources. Multiple instances of the same service may run
simultaneously.

# OPTIONS

*name*...
:   Show only the named services. An unknown name is reported on
    stderr and the exit status is non-zero.

**-a**, **--all**
:   Include finished instances still retained by mbd.

**-l**, **--long**
:   One block per service with its definition: queue, type, port,
    restart policy, run limit, token environment variable, instance
    counts and command. Instances are not listed, use **bservices** for
    them.

**-s**, **--start** *name*
:   Start an instance of the named service. The command blocks until
    the instance runs, then prints its job ID and URL. Interrupting it
    (**^C**) while the instance is still pending deletes the instance;
    once dispatched it keeps running.

**-t**, **--time** [*hours*:]*minutes*
:   With **-s** only. Run limit of the instance, at most the service's
    **RUNLIMIT**. Without it the instance gets **RUNLIMIT**. The limit
    counts from the first start and is not reset when the instance
    restarts; once it is reached the instance is stopped and not
    restarted.

**-d**, **--delete** *job_id*|*endpoint*
:   Delete the service instance identified by its job ID, or by its
    execution host and external port (**HOST:PORT** or
    **http://HOST:PORT**). A port alone is not sufficient. Pending
    instances can be deleted by job ID. Prints
    **Service <...> is being deleted**: a running instance ends when
    sbd reports it finished.

**-h**, **--help**
:   Print usage to stderr and exit.

**-v**, **--version**
:   Print version to stderr and exit.

# OUTPUT

## Starting a service

Prints the job ID and the URL of the running instance, with the token
when the service has **TOKEN_ENV**:

    Job <3> http://worker4:30000/?token=3adf689f99445585f4564b12f98de424

The port is the external port on the execution host. It may differ
from the application port inside the service's network namespace.

## Listing services

One line per service, **NAME**, **QUEUE** and **RESTART**, followed by
its instances. A user sees their own instances, a manager all of them.
Each instance has the following fields:

**USER**
:   User who owns the instance.

**HOST:PORT**
:   Execution host and external port, **-** unless the instance runs.

**JOB_ID**
:   Job ID of the instance.

**STATUS**
:   **PEND**, **DISP** (dispatched, not started yet, as in **bjobs**),
    **RUN** or **FINISH**.

**TOKEN**
:   The instance token, shown to its owner only, **-** otherwise.

## Long format (-l)

    SERVICE: echotest
      Queue:       service-2.0
      Type:        user
      Port:        8888
      Restart:     always
      Run limit:   0:10
      Token env:   ECHO_TOKEN
      Instances:   running: 1    pending: 0    finished: 3
      Command:     apptainer exec ... $LL_SVC_PORT

Empty settings are shown as **-**. The instance counts are for all
users; a dispatched instance not started yet counts as pending.

# NOTES

Start instances and connect to their endpoints from a host that does
not run them, normally the login node. The endpoint is a DNAT rule in
the execution host's `prerouting` chain, which only sees traffic from
other hosts: from the execution host itself the endpoint does not
answer.

# EXAMPLES

Start an instance of **echotest**:

    bservices -s echotest

List configured services and their instances:

    bservices

Show the definition of **echotest**:

    bservices -l echotest

Delete an instance using its host and external port:

    bservices -d worker4:30000

Delete the same instance using its URL:

    bservices -d http://worker4:30000

Inspect the job associated with an instance:

    bjobs 3
    bhist 3

# SEE ALSO

**bjobs**(1), **bhist**(1), **bqueues**(1), **mbd**(8)
