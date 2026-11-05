#!/bin/bash
# tests/system/bsub_time.sh

NAME="bsub_time"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# invalid limits are rejected by bsub
bsub --time 0 true >/dev/null 2>&1 && fail "--time 0 accepted"
bsub --time 1:xx true >/dev/null 2>&1 && fail "--time 1:xx accepted"

JID=$(bsub --time 1 -o /dev/null -e /dev/null sleep 3600 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done
[ "$STATE" != "RUN" ] && fail "timeout waiting for RUN, last state=$STATE"

# 60s limit, SIGTERM ends sleep, margin for the scheduler tick
for i in $(seq 1 90); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "EXIT" ] && break
    sleep 1
done
[ "$STATE" != "EXIT" ] && fail "expected EXIT after run limit got $STATE"

bhist -l $JID 2>/dev/null | grep -q "Run limit: *0:01" \
    || fail "run limit not in bhist"
bhist -l $JID 2>/dev/null | grep -q "signal: 15" \
    || fail "no SIGTERM in bhist"

echo "PASS: $NAME"
exit 0
