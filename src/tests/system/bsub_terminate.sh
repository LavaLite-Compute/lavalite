#!/bin/bash
# tests/system/bsub_terminate.sh

NAME="bsub_terminate"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# terminate 1 minute from now
TERM=$(date -d "+1 minute" "+%H:%M")

JID=$(bsub --terminate "$TERM" -o /dev/null -e /dev/null \
     "sleep 3600" 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

# wait for RUN
for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done
[ "$STATE" != "RUN" ] && fail "timeout waiting for RUN, last state=$STATE"

# wait for EXIT — up to 90s (60s grace + margin)
for i in $(seq 1 90); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "EXIT" ] && break
    sleep 1
done
[ "$STATE" != "EXIT" ] && fail "expected EXIT after terminate got $STATE"

echo "PASS: $NAME"
exit 0
