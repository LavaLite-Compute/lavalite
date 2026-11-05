#!/bin/bash
# tests/system/bsub_expire.sh
# A held job whose --terminate deadline passes is released straight
# to EXIT, it is never dispatched.

NAME="bsub_expire"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

TERM=$(date -d "+1 minute" "+%H:%M")
DEADLINE=$(date -d "$TERM" +%s)

JID=$(bsub --hold --terminate "$TERM" -o /dev/null -e /dev/null \
     sleep 3600 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

# wait past the deadline, at most ~62s
while [ $(date +%s) -le $DEADLINE ]; do
    sleep 1
done

bkill -s CONT $JID 2>/dev/null || fail "bkill -s CONT failed"

for i in $(seq 1 15); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "EXIT" ] && break
    sleep 1
done
[ "$STATE" != "EXIT" ] && fail "expected EXIT got $STATE"

bhist -l $JID 2>/dev/null | grep -q "Dispatched" \
    && fail "expired job was dispatched"

echo "PASS: $NAME"
exit 0
