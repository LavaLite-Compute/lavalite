#!/bin/bash
# tests/system/bsub_exclusive.sh

NAME="bsub_exclusive"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# submit exclusive job on one host
JID1=$(bsub --exclusive --machines worker1 -o /dev/null -e /dev/null sleep 30 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID1" ] && fail "no jobid for job1"

echo "RUN: $NAME jobdid=$JID1"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID1 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done

[ "$STATE" != "RUN" ] && fail "job1 not running, state=$STATE"

# submit second job to same host - should stay PEND
JID2=$(bsub --machines worker1 -o /dev/null -e /dev/null sleep 5 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID2" ] && fail "no jobid for job2"

sleep 3
STATE2=$(bjobs $JID2 2>/dev/null | awk 'NR==2 {print $3}')
[ "$STATE2" != "PEND" ] && fail "expected job2 PEND got $STATE2"

bkill $JID1 $JID2 2>/dev/null
echo "PASS: $NAME"
exit 0
