# LavaLite Signal Handling Test Plan (MVP)

Replace JOBID with the real numeric job id.

---

## A. Pending Job Tests

### A1 – Submit Held Job (Atomic PSUSP)

Command:
bsub -H -o /dev/null sleep 86400

Verify:
bjobs JOBID

Expected:
- STAT=PSUSP
- No EXEC_HOST

---

### A2 – Resume Held Job (PSUSP → PEND)

Command:
bkill -s CONT JOBID

Verify:
bjobs JOBID

Expected:
- STAT=PEND

---

### A3 – STOP on Plain PEND (PEND → PSUSP)

Precondition:
Job is STAT=PEND

Command:
bkill -s STOP JOBID

Verify:
bjobs JOBID

Expected:
- STAT=PSUSP

---

### A4 – Duplicate STOP on PSUSP

Precondition:
Job is STAT=PSUSP

Command:
bkill -s STOP JOBID

Expected:
- LSBE_JOB_SUSP
- State remains PSUSP

---

### A5 – CONT on Plain PEND (No-op)

Precondition:
Job is STAT=PEND

Command:
bkill -s CONT JOBID

Verify:
bjobs JOBID

Expected:
- Still STAT=PEND
- No newstatus event

---

### A6 – Kill Pending Job

Command:
bkill -s KILL JOBID

Verify:
bjobs JOBID

Expected:
- Transition to EXIT/DONE
- No EXEC_HOST

---

### A7 – Unsupported Signal on Pending

Command:
bkill -s HUP JOBID

Expected:
- LSBE_BAD_SIGNAL (or equivalent)
- No crash
- State unchanged

---

## B. Running Job Tests

### B1 – Submit Running Job

Command:
bsub -o /dev/null sleep 86400

Wait until:
bjobs JOBID

Expected:
- STAT=RUN
- EXEC_HOST populated

---

### B2 – STOP Running Job (RUN → USUSP)

Command:
bkill -s STOP JOBID

Verify:
bjobs JOBID

Expected:
- STAT=USUSP

---

### B3 – CONT Running Job (USUSP → RUN)

Command:
bkill -s CONT JOBID

Verify:
bjobs JOBID

Expected:
- STAT=RUN

---

### B4 – Duplicate CONT on RUN

Command:
bkill -s CONT JOBID

Expected:
- No crash
- State remains RUN

---

### B5 – Kill Running Job

Command:
bkill -s KILL JOBID

Verify:
bjobs JOBID

Expected:
- Transition to EXIT/DONE

---

## C. Finished Job Tests

### C1 – Signal Finished Job

Precondition:
Job is DONE or EXIT

Command:
bkill -s STOP JOBID

Expected:
- LSBE_JOB_FINISH
- No crash

---

## Transport Stability Checks

During all tests verify:

- No core dumps
- No unintended disconnects
- No UNKNOWN opcode logs
- No duplicate newstatus events for no-op actions
- LSBE_* codes represent semantic result
- -1 is only used for transport/internal failures
