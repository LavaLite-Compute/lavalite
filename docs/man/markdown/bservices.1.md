---
title: BSERVICES
section: 1
header: LavaLite User Commands
footer: LavaLite
date: 2026
---

# NAME

bservices - list, start, and delete service instances

# SYNOPSIS

**bservices**

**bservices** [**-t** [*hours*:]*minutes*] *name*

**bservices** **-l** [*name*...]

**bservices** **-d** *job_id*|*endpoint*

# DESCRIPTION

Manage instances of services defined in **llb.services**.

Without arguments, lists configured services and their active instances.

Each service instance runs as a job in its configured queue.
Scheduling depends on queue availability, job limits, and available
host resources.

Starting a service prints its HTTP URL when the instance is running.
Multiple instances of the same service may run simultaneously.

# OPTIONS

*name*
:   Start an instance of the named service defined in **llb.services**.

**-t**, **--time** [*hours*:]*minutes*
:   Run limit of the instance, at most the service's **RUNLIMIT**.
    Without it the instance gets **RUNLIMIT**. The limit counts from the
    first start and is not reset when the instance restarts; once it is
    reached the instance is stopped and not restarted.

**-l**, **--long** [*name*...]
:   One block per service: queue, restart policy, instance counts and
    all its instances. Names select the services to show; an unknown
    name is reported on stderr and the exit status is non-zero.

**-a**, **--all**
:   List configured services and all retained instances, including finished ones.

**-d**, **--delete** *job_id*|*endpoint*
:   Delete the service instance identified by its job ID, or by its
    execution host and external port (**HOST:PORT** or
    **http://HOST:PORT**). A port alone is not sufficient. Pending
    instances can be deleted by job ID. Prints
    **Service <...> is being deleted**: a running instance ends when
    sbd reports it finished.

Interrupting **bservices** *name* (**^C**) while the instance is still
pending deletes the instance. Once dispatched it keeps running.

**-h**, **--help**
:   Print usage to stderr and exit.

**-v**, **--version**
:   Print version to stderr and exit.

# OUTPUT

## Starting a service

Prints the URL of the running instance:

    http://worker4:30000

The port is the external port on the execution host. It may differ
from the application port inside the service's network namespace.

## Listing services

Services are grouped by name and queue. Each instance has the
following fields:

**USER**
:   User who owns the instance.

**PORT**
:   External port on the execution host.

**JOB_ID**
:   Job ID assigned to the instance.

**RUN_HOST**
:   Host running the instance.

**STATUS**
:   Current instance status.

# EXAMPLES

Start an instance of **echotest**:

    bservices echotest

List configured services and their instances:

    bservices

Delete an instance using its host and external port:

    bservices -d worker4:30000

Delete the same instance using its URL:

    bservices -d http://worker4:30000

Inspect the job associated with an instance:

    bjobs 3
    bhist 3

# SEE ALSO

**bjobs**(1), **bhist**(1), **bqueues**(1), **mbd**(8)
