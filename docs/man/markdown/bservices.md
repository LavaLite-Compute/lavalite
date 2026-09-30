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

**bservices** *name*

**bservices** **-l**

**bservices** **-d** *endpoint*

# DESCRIPTION

Manage instances of services defined in **llb.services**.

Each service instance runs as a job in its configured queue.
Scheduling depends on queue availability, job limits, and available
host resources.

Starting a service prints its HTTP URL when the instance is running.
Multiple instances of the same service may run simultaneously.

# OPTIONS

*name*
:   Start an instance of the named service defined in **llb.services**.

**-l**, **--list**
:   List configured services and their instances.

**-d**, **--delete** *endpoint*
:   Delete the service instance identified by its execution host and
    external port. Accepts **HOST:PORT** or **http://HOST:PORT**.
    A port alone is not sufficient.

**--help**
:   Print usage to stderr and exit.

**--version**
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

    bservices -l

Delete an instance using its host and external port:

    bservices -d worker4:30000

Delete the same instance using its URL:

    bservices -d http://worker4:30000

Inspect the job associated with an instance:

    bjobs -l 3
    bhist 3

# SEE ALSO

**bjobs**(1), **bhist**(1), **bqueues**(1), **mbd**(8)
