#!/bin/bash
# tests/system/bsub_begin.sh

NAME="bsub_begin"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# begin 1 minute from now
BEGIN=$(date -d "+1 minute" "+%H:%M")

JID=$(bsub --begin "$BEGIN" -o /dev/null -e /dev/null true 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

# should be PEND immediately
sleep 2
STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
[ "$STATE" != "PEND" ] && fail "expected PEND got $STATE"

bkill $JID 2>/dev/null
echo "PASS: $NAME"
exit 0
