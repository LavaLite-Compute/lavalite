#!/bin/bash
# tests/system/bsub_queue.sh
NAME="bsub_queue"
fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

QUEUES=$(bqueues | awk 'NR>1 {print $1}')
[ -z "$QUEUES" ] && fail "no queues returned by bqueues"

for Q in $QUEUES; do
    JID=$(bsub --queue "$Q" -o /dev/null -e /dev/null true 2>&1 | grep -oP 'Job <\K[0-9]+')
    [ -z "$JID" ] && fail "no jobid returned for queue $Q"
    echo "RUN: $NAME queue=$Q jobid=$JID"

    STATE=""
    for i in $(seq 1 10); do
        STATE=$(bjobs "$JID" 2>/dev/null | awk 'NR==2 {print $3}')
        [ "$STATE" = "DONE" ] && break
        sleep 1
    done
    [ "$STATE" != "DONE" ] && fail "timeout waiting for DONE on queue $Q, last state=$STATE"

    bhist "$JID" 2>/dev/null | grep -q "Queue <$Q>" || fail "queue $Q not found in bhist"
done

echo "PASS: $NAME"
exit 0
