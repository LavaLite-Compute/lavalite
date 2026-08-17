# LavaLite Service Jobs --- Initial Architecture

## Overview

A LavaLite service is implemented as an ordinary LavaLite job, while
exposing a stable network endpoint to the user through a separate
`service_proxy` process.

The service configuration and the list of configured services are
managed initially by `service.c`.

The central idea is:

-   the **service endpoint is stable**
-   the **backing job and execution host are ephemeral**
-   `mbd` remains responsible for scheduling and job lifecycle
-   `service_proxy` is responsible only for network forwarding

## Service Startup

The user starts a configured service with:

``` text
bservice jupyter
```

The request reaches the service manager in `mbd`, currently implemented
in `service.c`.

Before creating the backing job, the service manager allocates an
external port from the configured service port range. For example:

``` text
32100
```

The service instance can therefore be identified internally as:

``` text
uid:32100
```

The service manager tells the separate `service_proxy` process to create
a listener on that port:

``` text
LISTEN 32100
```

At this point there is no backend yet.

## Backing Job

The service manager creates an ordinary LavaLite job and places it in
the pending job list.

Normally this job is submitted to a dedicated service queue whose hosts
are reserved for service workloads.

From the scheduler's point of view, this remains a normal job:

``` text
PENDING -> RUNNING -> DONE/EXIT
```

The existing LavaLite scheduling and dispatch machinery therefore
remains responsible for selecting a host and starting the job.

## Backend Registration

Assume the scheduler dispatches the service job to:

``` text
r9c01
```

and the configured service listens inside the job environment on:

``` text
8888
```

Once the job has been scheduled and forked successfully, the service
manager is notified and registers the current backend with
`service_proxy`:

``` text
32100 -> r9c01:8888
```

`r9c01` is a LavaLite internal host identifier used to identify the
backend to the proxy.

The proxy does not participate in scheduling. It only maintains
forwarding mappings supplied by the service manager.

## User Endpoint

Once the backing job is running and the proxy mapping has been
installed, `bservice` returns:

``` text
http://cluster:32100
```

where `cluster` is a stable name resolving to the LavaLite
master/service endpoint.

The complete data path is now:

``` text
client
   |
   v
cluster:32100
   |
   v
service_proxy
   |
   v
r9c01:8888
```

From this point onward, `service_proxy` forwards client traffic arriving
at `cluster:32100` to the current backend `r9c01:8888`.

## Stable Service Identity

The important separation is between the stable service instance and its
current backing job:

``` text
service_id      = uid:32100
external_port   = 32100
job_id          = 4711
backend         = r9c01:8888
```

The service identity and external port survive changes to the backing
job.

For example, if the service job dies and the restart policy causes it to
restart on `r9c03`, the service manager only needs to update the proxy
mapping:

``` text
32100 -> r9c03:8888
```

The user's endpoint remains unchanged:

``` text
http://cluster:32100
```

This gives LavaLite a stable service abstraction while allowing the
scheduler to continue treating the actual execution as an ordinary job.

## `bservice` Completion Semantics

The intended behavior is for:

``` text
bservice jupyter
```

to wait while the service job is pending and being started.

It returns successfully only after:

1.  the backing job reaches the running state,
2.  its execution host is known,
3.  the backend service endpoint is known, and
4.  the proxy mapping has been installed.

It then prints:

``` text
http://cluster:32100
```

Therefore, when `bservice` returns successfully, the service endpoint is
expected to be usable.

```
bservice jupyter
      |
      | MBD_NEW_SERVICE
      |
      +-- wire_service_req
      |      name = "jupyter"
      |      uid
      |
      +-- wire_job_submit
      |
      +-- wire_job_script
             script launches configured image
```
