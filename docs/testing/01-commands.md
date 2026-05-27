# LavaLite Command Tests

Tests for command-line option parsing and basic behavior.
These tests verify client-side behavior — correct parsing, rejection of
invalid input, and basic output format. Daemon state validation is minimal.

Requires mbd and sbd running unless noted.

---

## bsub

### Basic submission

```sh
bsub sleep 86400
```

Expected: job accepted, job ID printed, job appears in `bjobs`.

### Queue selection

```sh
bsub --queue cpu sleep 86400
```

Expected: `bjobs` shows queue `cpu`.

### Invalid queue

```sh
bsub --queue does-not-exist sleep 86400
```

Expected: submission fails, no job created.

### Job name

```sh
bsub --name myjob sleep 86400
```

Expected: `bjobs` shows name `myjob`.

### Project

```sh
bsub --project myproject sleep 86400
```

Expected: job accepted, project preserved in submit record.

### Comment

```sh
bsub --comment 'test comment' sleep 86400
```

Expected: job accepted, comment preserved.

### CPU count

```sh
bsub --cpus 2 sleep 86400
```

Expected: job requests 2 CPUs, host free CPU decreases by 2 after dispatch.

### Memory

```sh
bsub --mem 512M sleep 86400
bsub --mem 2G sleep 86400
```

Expected: job accepted, memory request preserved.

### Invalid memory

```sh
bsub --mem xyz sleep 86400
```

Expected: submission fails.

### Storage

```sh
bsub --storage 10G sleep 86400
```

Expected: job accepted, storage request preserved.

### GPU

```sh
bsub --gpus 1 sleep 86400
bsub --gpus 1 --gpu-type full sleep 86400
```

Expected: job dispatches to host with available GPU.

### GPU type without GPU count

```sh
bsub --gpu-type full sleep 86400
```

Expected: submission fails.

### Exclusive

```sh
bsub --exclusive sleep 86400
```

Expected: host is marked exclusive while job runs, no other job dispatches there.

### Token pool

```sh
bsub --pool xyz=1 sleep 86400
```

Expected: token count decreases on dispatch, restored on finish.

### Placement

```sh
bsub --machines buntu24 sleep 86400
```

Expected: job dispatches only to `buntu24`.

### stdout/stderr

```sh
bsub --stdout out.%J --stderr err.%J sleep 1
```

Expected: files `out.<jobid>` and `err.<jobid>` created after job finishes.

### Hold

```sh
bsub --hold sleep 86400
```

Expected: job state is HELD, does not dispatch, queue HELD counter increases.

### Begin time

```sh
bsub --begin 23:59 sleep 86400
```

Expected: job accepted, remains pending until begin time.

### Terminate

```sh
bsub --terminate 0:02 sleep 86400
```

Expected: job accepted, terminated at deadline, reaches EXIT.

### Help and version

```sh
bsub --help
bsub --version
```

Expected: output printed, exit 0, no job created.

---

## bjobs

```sh
bjobs                  # active jobs for current user
bjobs --all            # all users (admin)
bjobs --pend           # pending jobs with reason
bjobs --run            # running only
bjobs --done           # finished jobs
bjobs <jobid>          # specific job
```

Expected: correct filtering, correct columns displayed.

---

## bkill

```sh
bkill --signal kill  <jobid>    # terminate
bkill --signal term  <jobid>    # SIGTERM
bkill --signal stop  <jobid>    # suspend
bkill --signal cont  <jobid>    # resume / release hold
bkill --signal hup   <jobid>    # SIGHUP
bkill --signal 10    <jobid>    # numeric signal
```

Expected: each signal produces the correct state transition.

---

## bqueues

```sh
bqueues                        # display all queues
bqueues --close cpu            # close queue (admin)
bqueues --open  cpu            # open queue (admin)
```

Expected: status column reflects open/closed, closed queue stops dispatch.

---

## bhosts

```sh
bhosts                         # display all hosts
bhosts --close buntu24         # close host (admin)
bhosts --open  buntu24         # open host (admin)
```

Expected: state column reflects ok/closed, closed host stops dispatch.

---

## btokens

```sh
btokens
```

Expected: POOL_NAME, TOTAL, USED, FREE columns displayed correctly.

---

## bgroups

```sh
bgroups
```

Expected: GROUP_NAME and GROUP_MEMBER columns displayed correctly.

---

## bhist

```sh
bhist                          # finished jobs for current user
bhist -u <user>                # finished jobs for user
bhist <jobid>                  # specific job
bhist -l <jobid>               # long format with resource usage
```

Expected: correct jobs displayed, resource usage present in long format.
