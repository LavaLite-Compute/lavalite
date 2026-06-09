#!/bin/bash
# tests/system/bsub_nhosts.sh

NAME="bsub_nhosts"
echo "RUN: $NAME"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

JID=$(bsub --nhosts 2 -o /dev/null -e /dev/null sleep 10 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"

echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done

[ "$STATE" != "RUN" ] && fail "timeout waiting for RUN, last state=$STATE"

NHOSTS=$(bjobs $JID 2>/dev/null | awk 'NR>=2 {count += gsub(/@/,"@")} END {print count}')

bkill $JID 2>/dev/null
echo "PASS: $NAME"
exit 0
