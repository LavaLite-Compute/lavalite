#!/bin/bash
# tests/system/bsub_machines.sh

NAME="bsub_machines"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub --machines worker1 -o /dev/null -e /dev/null sleep 5 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done

[ "$STATE" != "RUN" ] && fail "timeout waiting for RUN, last state=$STATE"

# verify dispatched to worker1
bjobs $JID 2>/dev/null | grep -q "worker1" || fail "not dispatched to worker1"

bkill $JID 2>/dev/null
echo "PASS: $NAME"
exit 0
