---
title: LLB.SERVICES
section: 5
header: LavaLite Configuration Files
footer: LavaLite
date: 2026
---

# NAME

llb.services - LavaLite service configuration

# DESCRIPTION

**llb.services** defines the services users can start with
**bservices**(1): long-running applications such as Jupyter or
code-server. Each service is defined in a **Begin Service** /
**End Service** block. **mbd** reads this file at startup.

Every instance of a service is a job in the service's queue. It runs
in its own network namespace on the execution host and is reached
through an endpoint, `http://host:port`, that **bservices -s** prints.

# FORMAT

Each service is defined as:

    Begin Service
    PARAMETER = VALUE
    ...
    End Service

Lines starting with **#** are comments. Blank lines are ignored.
Multiple services may be defined in the same file.

# PARAMETERS

**SERVICE_NAME**
:   The name of the service, used with **bservices -s**. Required.

**COMMAND**
:   The full command line of the application, run as is by the job's
    shell: container runtime, image and binds are the site's choice.
    The application must listen on **$LL_SVC_PORT**. Required.

**PORT**
:   The port the application listens on inside its namespace, passed
    to it as **LL_SVC_PORT**. The endpoint port on the execution host
    is picked by **sbd**, from 30000 up. Required.

**QUEUE**
:   The queue the instances run in, defined in **llb.queues**(5).
    Required.

**RESTART**
:   **always** or **never**. With **always** an instance that ends is
    started again, with the same job id: the application exited, was
    killed, or its host failed. An instance deleted with
    **bservices -d**, or that reached its run limit, is not restarted.
    With **never** an instance ends for good. Default **always**.

**RUNLIMIT**
:   The longest life of an instance, [*hours*:]*minutes*. It caps
    **bservices --time**. Optional, no limit if omitted.

**TOKEN_ENV**
:   The name of an environment variable that receives a random token
    for each instance, also exported as **LL_SVC_TOKEN**. For
    applications that protect their endpoint with a token or a
    password, such as **JUPYTER_TOKEN** or the **PASSWORD** of
    code-server. Optional.

# ENVIRONMENT

The instance's job has, besides the usual job environment:

**LL_SVC_PORT**
:   The **PORT** the application must listen on.

**LL_SVC_TOKEN**
:   The instance token, when **TOKEN_ENV** is set.

Keep the token out of the command line: **ps** shows every process's
arguments to every user. Prefer an application that reads it from
the **TOKEN_ENV** variable.

# ERRORS

An error in a service block never stops **mbd**. The service is not
configured, the reason is logged, and the other services are loaded:

- a missing **SERVICE_NAME**, **COMMAND**, **PORT** or **QUEUE**;
- a **QUEUE** not in **llb.queues**;
- a bad **PORT**, **RESTART**, **RUNLIMIT** or **TOKEN_ENV** value;
- an unknown key.

Instances of a service that is no longer configured come back as
orphans after an **mbd** restart, see the administrator guide,
*Orphan Jobs*. Check the **mbd** log after every restart.

# EXAMPLE

    Begin Service
    SERVICE_NAME = jupyter
    COMMAND      = apptainer exec --bind $HOME /images/jupyter.sif jupyter lab --ip=0.0.0.0 --port=$LL_SVC_PORT --no-browser
    PORT         = 8888
    TOKEN_ENV    = JUPYTER_TOKEN
    RESTART      = always
    RUNLIMIT     = 8:00
    QUEUE        = service
    End Service

    Begin Service
    SERVICE_NAME = code-server
    COMMAND      = apptainer exec --bind $HOME /images/code-server.sif code-server --bind-addr 0.0.0.0:$LL_SVC_PORT --auth password
    PORT         = 8080
    TOKEN_ENV    = PASSWORD
    RESTART      = never
    QUEUE        = service
    End Service

# SEE ALSO

**bservices**(1), **llb.queues**(5), **llb.hosts**(5), **mbd**(8),
**sbd**(8)
