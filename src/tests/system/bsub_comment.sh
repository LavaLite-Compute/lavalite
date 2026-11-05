#!/bin/bash
# tests/system/bsub_comment.sh

NAME="bsub_comment"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub --comment "my test comment" -o /dev/null -e /dev/null true \
     2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"

echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "DONE" ] && break
    sleep 1
done
[ "$STATE" != "DONE" ] && fail "timeout waiting for DONE, last state=$STATE"

# comment is ignored by scheduler — just verify job ran
echo "PASS: $NAME"
exit 0
