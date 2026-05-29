# bsub Basic Submission Tests

## Purpose

Verify that jobs can be submitted successfully and that the basic
submission options documented in `bsub(1)` behave correctly.

## Prerequisites

- Environment validation completed.
- At least one queue is open.
- At least one execution host is available.

## TEST-010: Submit a simple job

### Commands

```sh
bsub sleep 5
```

### Expected Result

A valid job identifier is returned.

Example:

```text
Job <123> is submitted to queue <cpu>.
```

### Pass Criteria

Job submission succeeds.

---

## TEST-011: Verify job completion

### Commands

```sh
bsub sleep 1
sleep 5
bhist
```

### Expected Result

The submitted job reaches state `DONE`.

### Pass Criteria

The completed job appears in `bhist`.

---

## TEST-012: Verify failed job

### Commands

```sh
bsub /bin/false
sleep 5
bhist
```

### Expected Result

The submitted job reaches state `EXIT`.

### Pass Criteria

The failed job appears in `bhist` with state `EXIT`.

---

## TEST-013: Submit a held job

### Commands

```sh
bsub --hold sleep 60
bjobs
```

### Expected Result

The job appears with state `HELD`.

### Pass Criteria

The held job is visible in `bjobs`.

---

## TEST-014: Release a held job

### Commands

```sh
bsub --hold sleep 60
bkill --signal cont <jobid>
bjobs
```

### Expected Result

The job leaves state `HELD`.

### Pass Criteria

The job becomes eligible for dispatch.

---

## TEST-015: Submit to a specific queue

### Commands

```sh
bsub --queue cpu sleep 5
```

### Expected Result

Job is submitted successfully.

### Pass Criteria

The queue shown by `bjobs` matches the requested queue.

---

## TEST-016: Submit with job name

### Commands

```sh
bsub --name test_job sleep 5
bjobs
```

### Expected Result

The specified job name is displayed.

### Pass Criteria

Job name appears correctly.

---

## TEST-017: Verify stdout redirection

### Commands

```sh
rm -f output.txt

bsub --stdout output.txt hostname

sleep 5

cat output.txt
```

### Expected Result

The file contains the command output.

### Pass Criteria

The output file is created and contains data.

---

## TEST-018: Verify stderr redirection

### Commands

```sh
rm -f error.txt

bsub --stderr error.txt ls /does/not/exist

sleep 5

cat error.txt
```

### Expected Result

The file contains the command error output.

### Pass Criteria

The error file is created and contains data.

---

## TEST-019: Verify %J expansion

### Commands

```sh
bsub --stdout test.%J.out hostname
```

### Expected Result

Output file name contains the assigned job ID.

Example:

```text
test.123.out
```

### Pass Criteria

The `%J` token is expanded correctly.

---

## TEST-020: Verify command arguments

### Commands

```sh
bsub echo hello world
```

### Expected Result

The command executes successfully.

### Pass Criteria

The submitted command receives all arguments unchanged.
