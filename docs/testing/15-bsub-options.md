# bsub Option Combination Tests

## Purpose

Verify that `bsub` correctly handles resource requests, placement
constraints, I/O options, scheduling options, and common combinations of
submission options.

These tests validate the command-line contract documented in `bsub(1)`.

## Prerequisites

- Environment validation completed.
- Basic `bsub` tests completed.
- At least one queue is open.
- At least one execution host is available.
- The test user can submit jobs.
- The commands `bsub`, `bjobs`, `bhist`, `bhosts`, `bqueues`,
  `bgroups`, `btokens`, and `bkill` are available in `PATH`.

Some tests require optional cluster features such as GPUs, token pools,
multiple hosts, or host groups. If a feature is not configured in the
test cluster, mark the corresponding test as skipped.

## TEST-150: Submit with queue, name, project, and comment

### Commands

```sh
bsub \
    --queue cpu \
    --name option_test \
    --project test_project \
    --comment "bsub option combination test" \
    sleep 3600

bjobs <jobid>
bhist <jobid>
```

### Expected Result

- Job is submitted to queue `cpu`.
- Job name is `option_test`.
- Project and comment are stored if displayed by `bhist`.

### Pass Criteria

The job is accepted and visible through `bjobs` and `bhist`.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-151: Submit with CPU request

### Commands

```sh
bsub --cpus 2 sleep 3600

bjobs <jobid>
bhist <jobid>
bhosts inspect the counters
```

### Expected Result

The job requests 2 CPU slots per host.

### Pass Criteria

`bhist <jobid>` shows:

```text
2 cpu(s)/host
```

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-152: Submit with multiple hosts and CPUs per host

### Commands

```sh
bsub --nhosts 2 --cpus 2 sleep 3600

bjobs <jobid>
bhist <jobid>
bhosts inspect the counters
```

### Expected Result

The job requests:

```text
2 host(s)
2 cpu(s)/host
```

If two hosts are available, the job may dispatch across two hosts.

### Pass Criteria

`bhist <jobid>` shows the requested host and CPU counts.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-153: Submit with memory request

### Commands

```sh
bsub --mem 512M sleep 3600

bhist <jobid>
```

### Expected Result

The job requests 512 MB memory per host.

### Pass Criteria

`bhist <jobid>` shows the requested memory.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-154: Submit with storage request

### Commands

```sh
bsub --storage 1G sleep 3600

bhist <jobid>
```

### Expected Result

The job requests 1 GB local storage per host.

### Pass Criteria

`bhist <jobid>` shows the requested storage if storage is displayed.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-155: Submit with combined CPU, memory, and storage requests

### Commands

```sh
bsub --cpus 2 --mem 512M --storage 1G sleep 3600

bjobs <jobid>
bhist <jobid>
bhosts
```

### Expected Result

- The job is accepted if resources are available.
- Requested resources are visible in `bhist`.
- Host usage counters reflect the running job.

### Pass Criteria

The job runs only on a host with sufficient CPU, memory, and storage.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-156: Reject impossible CPU request

### Commands

```sh
bsub --cpus 999999 sleep 3600

bjobs --pend
```

### Expected Result

The job remains pending with a pending reason.

### Pass Criteria

`bjobs --pend` shows the job in `PEND` state and explains that resources
are not available.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-157: Submit with GPU request

### Prerequisite

At least one host has GPUs configured in `llb.hosts`.

### Commands

```sh
bsub --gpus 1 sleep 3600

bjobs <jobid>
bhist <jobid>
bhosts
```

### Expected Result

The job runs only on a host with at least one available GPU.

### Pass Criteria

`bhist` shows:

```text
1 gpu(s)/host
```

and the selected execution host has GPUs configured.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-158: Submit with GPU model

### Prerequisite

At least one GPU model is configured in `llb.hosts`.

### Commands

```sh
bsub --gpus 1 --gpu-model a100 sleep 3600

bjobs <jobid>
bhist <jobid>
```

### Expected Result

The job runs only on a host with the requested GPU model.

### Pass Criteria

The job is dispatched to a matching GPU host, or remains pending if no
matching GPU is available.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-159: Reject GPU model without GPU count

### Commands

```sh
bsub --gpu-model a100 sleep 3600
```

### Expected Result

The command fails because `--gpu-model` requires `--gpus`.

### Pass Criteria

`bsub` exits with non-zero status and displays a useful error message.

---

## TEST-170: Submit exclusive job

### Commands

```sh
bsub --exclusive sleep 3600

bjobs <jobid>
bhist <jobid>
bhosts
```

### Expected Result

The job runs in exclusive mode.

### Pass Criteria

The selected host is not shared with other jobs while the exclusive job
is running.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-171: Verify exclusive job blocks sharing

### Setup

Submit an exclusive job:

```sh
bsub --exclusive sleep 3600
```

Then submit a second job that could otherwise run on the same host:

```sh
bsub sleep 3600
```

### Expected Result

The second job is not dispatched to the host occupied by the exclusive
job.

### Pass Criteria

The second job either dispatches to another host or remains pending.

### Cleanup

```sh
bkill --signal kill <exclusive_jobid>
bkill --signal kill <second_jobid>
```

---

## TEST-172: Submit with host placement constraint

### Prerequisite

At least one known host is available.

Use:

```sh
bhosts
```

to select a valid host.

### Commands

```sh
bsub --machines "sim1" sleep 3600

bjobs <jobid>
bhist <jobid>
```

### Expected Result

The job is dispatched only to `sim1`.

### Pass Criteria

`bhist <jobid>` shows `sim1` as the execution host.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-173: Submit with host group placement constraint

### Prerequisite

At least one host group is configured.

Use:

```sh
bgroups
```

to select a valid host group.

### Commands

```sh
bsub --machines "group-cpu" sleep 3600

bjobs <jobid>
bhist <jobid>
```

### Expected Result

The job is dispatched only to a host that belongs to the specified host
group.

### Pass Criteria

The execution host shown by `bhist` is a member of the requested group.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-174: Reject unknown machine

### Commands

```sh
bsub --machines "does_not_exist" sleep 3600
```

### Expected Result

The command fails or the job remains pending with an appropriate pending
reason.

### Pass Criteria

The invalid placement constraint does not dispatch the job to an
unrequested host.

### Cleanup

If the job was accepted:

```sh
bkill --signal kill <jobid>
```

---

## TEST-175: Submit with stdout, stderr, and %J expansion

### Commands

```sh
rm -f stdout.*.txt stderr.*.txt

bsub \
    --stdout stdout.%J.txt \
    --stderr stderr.%J.txt \
    "sh -c 'echo hello stdout; echo hello stderr >&2'"

bhist <jobid>

cat stdout.<jobid>.txt
cat stderr.<jobid>.txt
```

### Expected Result

- Standard output is written to `stdout.<jobid>.txt`.
- Standard error is written to `stderr.<jobid>.txt`.
- `%J` is expanded to the job ID.

### Pass Criteria

Both files are created and contain the expected output.

---

## TEST-176: Submit with stdin

### Commands

```sh
printf 'hello stdin\n' > input.txt

bsub \
    --stdin input.txt \
    --stdout stdin.%J.out \
    cat

cat stdin.<jobid>.out
```

### Expected Result

The job reads from `input.txt` and writes the content to the stdout file.

### Pass Criteria

`stdin.<jobid>.out` contains:

```text
hello stdin
```

---

## TEST-177: Submit with token pool request

### Prerequisite

At least one token pool is configured.

Use:

```sh
btokens
```

to select a valid token pool.

### Commands

```sh
bsub --pool hspice=1 sleep 3600

btokens
bjobs <jobid>
bhist <jobid>
```

### Expected Result

- The job requests one token from the `hspice` pool.
- The token pool usage increases while the job is running.

### Pass Criteria

`btokens` shows the token allocated to the running job.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-178: Submit with multiple token pools

### Prerequisite

At least two token pools are configured.

### Commands

```sh
bsub --pool hspice=1 --pool matlab=1 sleep 3600

btokens
bhist <jobid>
```

### Expected Result

Both token pools are allocated while the job is running.

### Pass Criteria

`btokens` shows usage in both requested pools.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-179: Reject impossible token request

### Commands

```sh
bsub --pool hspice=999999 sleep 3600

bjobs --pend
btokens
```

### Expected Result

The job is reject as Job not submitted: Invalid argument

### Pass Criteria

Jobs is not submitted.

---

## TEST-180: Submit with begin time

### Commands

```sh
bsub --begin 23:59 sleep 3600

bjobs --pend
bhist <jobid>
```

### Expected Result

The job is accepted but does not dispatch before the begin time.

### Pass Criteria

The job remains pending until the begin time condition is satisfied.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-181: Submit with termination deadline

### Commands

```sh
bsub --terminate 23:59 sleep 3600

bhist <jobid>
```

### Expected Result

The job is accepted with a termination deadline.

### Pass Criteria

The termination setting is visible if displayed by `bhist`, and the job
is eligible to be terminated when the deadline is reached.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-182: Combine queue, resources, placement, and I/O

### Prerequisites

- Queue `cpu` exists.
- Host or host group `sim1` exists, or replace it with a valid placement
  target.

### Commands

```sh
bsub \
    --queue cpu \
    --name combo_test \
    --project lavalite_test \
    --cpus 2 \
    --mem 512M \
    --storage 1G \
    --machines "sim1" \
    --stdout combo.%J.out \
    --stderr combo.%J.err \
    "sh -c 'hostname; echo ok; echo err >&2; sleep 60'"

bjobs <jobid>
bhist <jobid>
cat combo.<jobid>.out
cat combo.<jobid>.err
```

The previous submission tests `--machines`.

Submit another job replacing `--machines` with `--nhosts` since the two
options are mutually exclusive.

### Expected Result

- Job is accepted.
- Queue, name, resources, placement, and I/O paths are honored.
- Output and error files are created.
- Job runs only on the requested placement target.

### Pass Criteria

All selected options are reflected in `bjobs`, `bhist`, output files, or
execution host selection.

### Cleanup

```sh
bkill --signal kill <jobid>
```

---

## TEST-183: Reject invalid numeric resource values

### Commands

```sh
bsub --cpus 0 sleep 3600
bsub --nhosts 0 sleep 3600
bsub --gpus -1 sleep 3600
```

### Expected Result

Each command fails.

### Pass Criteria

Each invalid resource request exits with non-zero status and displays a
useful error message.

---

## TEST-184: Reject invalid memory or storage values

### Commands

```sh
bsub --mem banana sleep 3600
bsub --storage banana sleep 3600
```

### Expected Result

Each command fails.

### Pass Criteria

Invalid size strings are rejected.

## Completion Criteria

The `bsub` option test set passes when:

- Identity options are accepted and stored.
- CPU, host, memory, storage, GPU, and exclusive options are honored.
- Placement constraints are enforced.
- Standard input, output, and error options work.
- `%J` is expanded in output paths.
- Token pools are allocated and released correctly.
- Begin and terminate scheduling options are accepted.
- Invalid option combinations are rejected cleanly.
