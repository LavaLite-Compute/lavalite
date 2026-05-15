# LavaLite Manual Test Plan: `bsub` Options

## Purpose

This document defines manual tests for validating `bsub` command-line options.

The goal is to verify that `bsub` correctly parses user options, sends the expected submit request to `mbd`, and that the resulting job state, queue accounting, resource request, and event flow are correct.

These tests are intended for interactive validation during the LavaLite 1.0 stabilization phase.

## Test Assumptions

The test environment has:

* `mbd` running
* `sbd` running unless the test explicitly says otherwise
* LavaLite client commands installed:

  * `bsub`
  * `bkill`
  * `bjobs`
  * `bqueues`
  * `bhosts`
* A configured queue, for example `cpu`
* A configured host, for example `buntu24`
* Access to mbd logs
* Access to mbd sysevents
* Debug/assert counter checks enabled in `mbd`

Example long-running job command:

```sh
sleep 86400
```

Long-running jobs are useful for interactive debugging because they keep the system in a stable state while daemons, logs, and event files are inspected. Short-running jobs are better suited for bulk and stress testing.

## General Validation Rules

After each successful submission, verify:

```sh
bjobs
bqueues
bhosts
```

Also check:

* `mbd` logs
* sysevents entries
* queue counters
* host counters
* no assertion failure in `mbd`

For submitted jobs, verify that:

* `JOBID` is assigned
* job queue is correct
* job state is correct
* queue counters are correct
* host counters are correct when the job dispatches
* requested resources are reflected internally or through logs/debug output

## Cleanup

After each test, remove jobs before continuing:

```sh
bkill <jobid>
```

Then verify:

```sh
bjobs -a
bqueues
bhosts
```

Expected result:

* job eventually reaches `DONE` or `EXIT`
* active queue counters return to zero
* active host counters return to zero

---

# 1. Basic Submission

## 1.1 Submit default job

```sh
bsub sleep 86400
```

Expected:

* job is accepted
* job is assigned a job ID
* queue defaults to the system default queue selected by `mbd`
* job enters `PEND` or `RUN` depending on scheduler timing

Verify:

```sh
bjobs
bqueues
```

Expected queue counters:

* `NJOBS` increases by 1
* either `PEND` or `RUN` increases by 1

## 1.2 Submit short command with arguments

```sh
bsub /bin/sh -c 'sleep 86400'
```

Expected:

* full command is preserved
* job starts correctly
* output/error files are created using defaults unless overridden

---

# 2. Job Identity Options

## 2.1 Queue selection

```sh
bsub --queue cpu sleep 86400
```

Expected:

* job is submitted to queue `cpu`
* `bjobs` shows queue `cpu`
* `bqueues` counter for `cpu` increases

## 2.2 Invalid queue

```sh
bsub --queue does-not-exist sleep 86400
```

Expected:

* submission fails
* no job is created
* queue counters do not change
* `bsub` reports an error

## 2.3 Job name

```sh
bsub --name test-name sleep 86400
```

Expected:

* `bjobs` shows job name `test-name`

## 2.4 Project name

```sh
bsub --project test-project sleep 86400
```

Expected:

* job is accepted
* project value is preserved in submit data/event record
* scheduler behavior is unchanged unless project accounting is later added

## 2.5 Comment

```sh
bsub --comment 'manual bsub option test' sleep 86400
```

Expected:

* job is accepted
* comment is preserved in submit data/event record
* scheduler ignores the comment

---

# 3. CPU and Host Resource Options

## 3.1 Default CPU count

```sh
bsub sleep 86400
```

Expected:

* default CPU request is 1 per host

## 3.2 Explicit CPU count

```sh
bsub --cpus 2 sleep 86400
```

Expected:

* job requests 2 CPU slots per host
* host free CPU count decreases by 2 after dispatch
* host run counter increases by 1

## 3.3 Invalid CPU count: zero

```sh
bsub --cpus 0 sleep 86400
```

Expected:

* submission fails or request is rejected
* no job is created

## 3.4 Explicit host count

```sh
bsub --nhosts 1 sleep 86400
```

Expected:

* job requests 1 execution host
* job dispatches if one suitable host is available

## 3.5 Multi-host request

```sh
bsub --nhosts 2 --cpus 1 sleep 86400
```

Expected:

* job requests 2 execution hosts
* job remains pending if insufficient hosts are available
* job dispatches only when the requested number of hosts can be allocated

---

# 4. Memory and Storage Options

## 4.1 Memory in megabytes

```sh
bsub --mem 512M sleep 86400
```

Expected:

* job requests 512 MB per host
* host free memory decreases after dispatch

## 4.2 Memory in gigabytes

```sh
bsub --mem 2G sleep 86400
```

Expected:

* job requests 2048 MB per host
* memory accounting is correct

## 4.3 Invalid memory size

```sh
bsub --mem xyz sleep 86400
```

Expected:

* submission fails
* no job is created

## 4.4 Scratch storage

```sh
bsub --storage 1G sleep 86400
```

Expected:

* job requests 1024 MB local scratch storage per host
* host free storage decreases after dispatch

---

# 5. GPU Options

## 5.1 Request one GPU

```sh
bsub --gpus 1 sleep 86400
```

Expected:

* job requests 1 GPU per host
* job dispatches only to a host with an available GPU
* host free GPU count decreases after dispatch

## 5.2 Request GPU type

```sh
bsub --gpus 1 --gpu-type full sleep 86400
```

Expected:

* job requests 1 GPU of type `full`
* scheduler only selects hosts with available matching GPU type

## 5.3 GPU type without GPU count

```sh
bsub --gpu-type full sleep 86400
```

Expected:

* submission fails
* `--gpu-type` requires `--gpus`

## 5.4 Request unavailable GPU count

```sh
bsub --gpus 999 sleep 86400
```

Expected:

* job remains pending or submission is rejected depending on configured policy
* scheduler must not over-allocate GPUs

---

# 6. Exclusive Host Allocation

## 6.1 Exclusive job

```sh
bsub --exclusive sleep 86400
```

Expected:

* selected host is marked exclusive while job is running
* no other job is dispatched to that host while exclusive job is active

## 6.2 Second job while host is exclusive

In terminal 1:

```sh
bsub --exclusive sleep 86400
```

In terminal 2:

```sh
bsub sleep 86400
```

Expected:

* second job does not dispatch to the exclusive host
* second job remains pending if no other host is available

---

# 7. Token Pool Options

## 7.1 Request token pool resource

```sh
bsub --pool license=1 sleep 86400
```

Expected:

* job requests 1 token from pool `license`
* token count decreases when job dispatches
* token count is restored when job finishes

## 7.2 Request unavailable token count

```sh
bsub --pool license=999 sleep 86400
```

Expected:

* job remains pending or submission fails depending on configured policy
* token pool is not over-allocated

## 7.3 Repeat token pool option

```sh
bsub --pool license=1 --pool dataset=1 sleep 86400
```

Expected:

* job requests tokens from both pools
* dispatch occurs only when both token requests can be satisfied

---

# 8. Placement Options

## 8.1 Restrict to host

```sh
bsub --machines buntu24 sleep 86400
```

Expected:

* job dispatches only to `buntu24`
* job remains pending if `buntu24` is unavailable or lacks resources

## 8.2 Restrict to invalid host

```sh
bsub --machines does-not-exist sleep 86400
```

Expected:

* submission fails or job remains pending depending on current policy
* scheduler must not dispatch to an unknown host

## 8.3 Restrict to multiple hosts

```sh
bsub --machines "buntu24 worker1" sleep 86400
```

Expected:

* scheduler selects only from the listed hosts

---

# 9. I/O Options

## 9.1 Standard output path

```sh
bsub --stdout out.%J sleep 1
```

Expected:

* stdout path is accepted
* output file is created after job runs

## 9.2 Standard error path

```sh
bsub --stderr err.%J /bin/sh -c 'echo error >&2'
```

Expected:

* stderr path is accepted
* stderr file contains expected output

## 9.3 Standard input path

```sh
printf 'hello\n' > input.txt
bsub --stdin input.txt /bin/cat
```

Expected:

* job receives input from `input.txt`
* output contains `hello`

## 9.4 Invalid input path

```sh
bsub --stdin does-not-exist /bin/cat
```

Expected:

* submission fails or job fails at execution depending on current validation policy
* behavior is logged clearly

---

# 10. Hold and State Options

## 10.1 Submit held job

```sh
bsub --hold sleep 86400
```

Expected:

* job is submitted successfully
* job state is `HELD`
* job does not dispatch
* queue `HELD` counter increases by 1

Verify:

```sh
bjobs
bqueues
```

Expected example:

```text
JOBID  USER   STAT  QUEUE  EXEC_HOST  JOB_NAME  SUBMIT_TIME
1      david  HELD  cpu    -          -         May 14 17:17
```

```text
QUEUE_NAME  PRIO  STATE  MAX  NJOBS  PEND  HELD  RUN  SUSP
cpu         30    open     0      1     0     1    0     0
```

## 10.2 Submit held job using short option

```sh
bsub -H sleep 86400
```

Expected:

* same behavior as `--hold`

---

# 11. Begin Time

## 11.1 Begin time in future

```sh
bsub --begin 1:00 sleep 86400
```

Expected:

* job is accepted
* job remains pending until begin time is reached
* scheduler does not dispatch before begin time

## 11.2 Invalid begin time

```sh
bsub --begin invalid sleep 86400
```

Expected:

* submission fails
* no job is created

---

# 12. Termination Time

## 12.1 Terminate at deadline

```sh
bsub --terminate 0:02 sleep 86400
```

Expected:

* job is accepted
* job is terminated at deadline
* termination signal flow is logged
* job eventually reaches `EXIT`

---

# 13. Wall Clock Limit

## 13.1 Wall clock limit

```sh
bsub --wall 1 sleep 86400
```

Expected:

* job is accepted
* job is terminated after wall limit
* job eventually reaches `EXIT`
* events show timeout/termination flow

## 13.2 Invalid wall limit

```sh
bsub --wall invalid sleep 86400
```

Expected:

* submission fails
* no job is created

---

# 14. Dependency Option

## 14.1 Dependency expression accepted

```sh
bsub --dependency 'done(1)' sleep 86400
```

Expected:

* job is accepted if dependency syntax is valid
* job remains pending or held until dependency evaluates true

## 14.2 Invalid dependency expression

```sh
bsub --dependency 'bad expression' sleep 86400
```

Expected:

* submission fails or dependency is rejected depending on current parser behavior
* no invalid dependency should cause scheduler corruption

---

# 15. Help and Version

## 15.1 Help

```sh
bsub --help
```

Expected:

* usage text is printed
* command exits with status 0
* no job is submitted

## 15.2 Version

```sh
bsub --version
```

Expected:

* version string is printed
* command exits with status 0
* no job is submitted

---

# 16. Short Option Compatibility

Where short options are supported, verify that they behave identically to the long option form.

Examples:

```sh
bsub -q cpu sleep 86400
bsub -J short-name sleep 86400
bsub -n 2 sleep 86400
bsub -N 1 sleep 86400
bsub -M 1G sleep 86400
bsub -H sleep 86400
```

Expected:

* each short option maps to the same internal submit request as its long option equivalent
* invalid short option usage is rejected

---

# 17. Failure and Restart Validation

For representative submitted jobs, repeat selected tests with daemon restart.

## 17.1 Restart `mbd` after pending submission

```sh
bsub --hold sleep 86400
```

Restart `mbd`.

Expected:

* job is restored after replay
* state remains `HELD`
* queue counters are correct
* no counter assertion fails

## 17.2 Restart `mbd` after running submission

```sh
bsub sleep 86400
```

Wait for job to run, then restart `mbd`.

Expected:

* job is restored after replay
* state is reconstructed correctly
* host and queue counters are correct
* no counter assertion fails

## 17.3 Disconnect `sbd` while job is running

```sh
bsub sleep 86400
```

Stop `sbd` after the job starts.

Expected:

* host is shown unavailable or unknown by `bhosts`
* internal job state is not mutated solely due to host disconnect
* public job view may report `UNKNOWN` if execution host contact is lost
* queue counters remain consistent

Restart `sbd`.

Expected:

* daemon reconnect flow succeeds
* job state/reporting recovers according to actual process state
* no counter assertion fails

---

# 18. Final Validation Checklist

Before accepting the `bsub` option test pass, verify:

* all successful submissions create exactly one job
* failed submissions do not create jobs
* queue counters remain correct
* host counters remain correct
* held jobs use `HELD`, not suspended state
* pending jobs use `PEND`
* running jobs use `RUN`
* stopped jobs use `SUSP`
* disconnected execution hosts do not corrupt internal job state
* replay restores expected state after restart
* sysevents are complete and ordered
* no `mbd` assertion fails
* no daemon crash occurs
