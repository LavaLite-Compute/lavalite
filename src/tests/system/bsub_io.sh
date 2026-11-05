#!/bin/bash
# tests/system/bsub_io.sh

NAME="bsub_io"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

OUTFILE="/export/soft/tmp/bsub_io_test_%J.out"
ERRFILE="/export/soft/tmp/bsub_io_test_%J.err"

JID=$(bsub --stdout "$OUTFILE" --stderr "$ERRFILE" \
     "sh -c 'echo hello_stdout; echo hello_stderr >&2'" \
     2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

REAL_OUT="/export/soft/tmp/bsub_io_test_${JID}.out"
REAL_ERR="/export/soft/tmp/bsub_io_test_${JID}.err"

for i in $(seq 1 15); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && break
    sleep 1
done

[ "$STATE" != "DONE" ] && fail "timeout waiting for DONE, last state=$STATE"

grep -q "hello_stdout" "$REAL_OUT" || fail "stdout file missing or wrong content"
grep -q "hello_stderr" "$REAL_ERR" || fail "stderr file missing or wrong content"

rm -f "$OUTFILE" "$ERRFILE"
echo "PASS: $NAME"
exit 0
