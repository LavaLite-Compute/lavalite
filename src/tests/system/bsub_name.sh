#!/bin/bash
# tests/system/bsub_name.sh

NAME="bsub_name"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub --name myjobname -o /dev/null -e /dev/null true \
     2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && break
    sleep 1
done
[ "$STATE" != "DONE" ] && fail "timeout waiting for DONE, last state=$STATE"

bhist $JID 2>/dev/null | grep -q "myjobname" || fail "name not in bhist"

echo "PASS: $NAME"
exit 0
