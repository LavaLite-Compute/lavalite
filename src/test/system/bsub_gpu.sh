#!/bin/bash
# tests/system/bsub_gpu.sh

NAME="bsub_gpu"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

echo "RUN: $NAME"

JID=$(bsub --gpus 1 --gpu-type A100 -o /dev/null -e /dev/null sleep 10 2>&1 | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"

echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 10); do
    STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
    [ "$STATE" = "RUN" ] && break
    sleep 1
done

[ "$STATE" != "RUN" ] && fail "timeout waiting for RUN, last state=$STATE"

# verify GPU assigned in bhist
bhist $JID 2>/dev/null | grep -q "GPU devices:" || fail "no GPU devices in bhist"

bkill $JID 2>/dev/null
echo "PASS: $NAME"
exit 0
