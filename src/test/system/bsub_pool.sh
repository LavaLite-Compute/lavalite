#!/bin/bash
# tests/system/bsub_pool.sh

NAME="bsub_pool"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# assumes a pool named 'licenses' exists with tokens
JID=$(bsub --tokens xyz=1 -o /dev/null -e /dev/null true 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"

echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && break
    sleep 1
done

[ "$STATE" != "DONE" ] && fail "timeout waiting for DONE, last state=$STATE"

bhist $JID 2>/dev/null | grep -q "licenses" || fail "pool licenses not in bhist"

echo "PASS: $NAME"
exit 0
