# echo_server.py

Minimal HTTP echo server, Python stdlib only, no dependencies. Answers
every request with its own method, path, headers, and body.

Purpose: isolate whether a `bservice` failure is in the LavaLite
scaffolding (registration, port allocation, proxy forwarding, dispatch)
or in the container/command being run. If this round-trips end to end
through `bservice`, the scaffolding is proven clean and any remaining
failure (e.g. jupyter exiting immediately) is a container/command
problem, not a LavaLite one.

## Manual test (no LavaLite, just apptainer + curl)

Run this from the user's home directory

```bash
apptainer pull /export/service_images/python-slim.sif docker://python:3.11-slim

apptainer exec --bind /home/david/soft/lavalite/services/src/test/services:/test /export/service_images/python-slim.sif python3 /test/echo_server.py 8888
```

From another terminal:

```bash
curl -d 'hello world' http://localhost:8888/anything
```

Expect the request echoed back — method, path, headers, body.

Use `apptainer exec`, not `apptainer run` -- `run` executes the
image's `%runscript`/default entrypoint and mostly ignores extra args
unless the def file forwards them. `exec` runs an arbitrary command
inside the container, which is exactly what `service.c` synthesizes
(`apptainer exec <image> <command>`) -- `exec` is what actually
matches production behavior.

## Through bservice

`llb.services`:

```
Begin Service
SERVICE_NAME = echotest
TYPE = user
IMAGE = /export/service_images/python-slim.sif
COMMAND = python3 /home/david/soft/lavalite/services/src/test/services/echo_server.py 8888
PORT = 8888
End Service
```

Apptainer auto-binds `$HOME` by default, so the container can see the
script at its path under `/home/david/...` without any extra bind
flags -- editing the script needs no image rebuild.

```bash
./bservice echotest
curl -d 'hello' http://<host>:<ext_port>/anything
```

If this round-trips, the chain (registration -> port alloc -> proxy ->
dispatch -> here) is proven clean.
