#!/bin/bash
# tests/system/bsub_basic.sh

NAME="bsub_basic"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub -o /dev/null -e /dev/null true 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && echo "PASS: $NAME" && exit 0
    sleep 1
done

fail "timeout waiting for DONE, last state=$STATE"
