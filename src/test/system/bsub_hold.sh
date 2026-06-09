#!/bin/bash
# tests/system/bsub_hold.sh

NAME="bsub_hold"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub --hold -o /dev/null -e /dev/null sleep 5 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"

echo "RUN: $NAME jobid=$JID"

STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
[ "$STATE" != "HELD" ] && fail "expected HELD got $STATE"

bkill -s CONT $JID 2>/dev/null || fail "bkill -s CONT failed"

for i in $(seq 1 15); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && echo "PASS: $NAME" && exit 0
    sleep 1
done

fail "timeout waiting for DONE, last state=$STATE"
