#!/bin/bash
# tests/system/bservices_basic.sh
# Start, bkill refused, delete by job id and by endpoint.
# SVC names a service in llb.services, default echotest.

NAME="bservices_basic"
SVC=${SVC:-echotest}

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# instance lines: USER HOST:PORT JOB_ID STATUS TOKEN
svc_stat() {
    bservices -a 2>/dev/null | awk -v j="$1" '$3==j {print $4}'
}

wait_stat() {
    for i in $(seq 1 "$3"); do
        [ "$(svc_stat $1)" = "$2" ] && return 0
        sleep 1
    done
    return 1
}

bservices 2>/dev/null | grep -q "^$SVC " \
    || { echo "SKIP: $NAME service $SVC not configured"; exit 0; }

OUT=$(timeout 60 bservices -s $SVC 2>&1)
JID=$(echo "$OUT" | grep -oP 'Job <\K[0-9]+')
EP=$(echo "$OUT" | grep -oP 'http://\K[^/ ]+')
[ -z "$JID" ] && fail "start failed: $OUT"
echo "RUN: $NAME jobid=$JID endpoint=$EP"

[ "$(svc_stat $JID)" = "RUN" ] || fail "expected RUN got $(svc_stat $JID)"
STATE=$(bjobs $JID 2>/dev/null | awk 'NR==2 {print $3}')
[ "$STATE" = "RUN" ] || fail "bjobs expected RUN got $STATE"

bkill $JID >/dev/null 2>&1 && fail "bkill accepted a service job"

bservices -d $JID >/dev/null 2>&1 || fail "delete by job id failed"
wait_stat $JID FINISH 15 || fail "expected FINISH got $(svc_stat $JID)"

# delete by host:port
OUT=$(timeout 60 bservices -s $SVC 2>&1)
JID=$(echo "$OUT" | grep -oP 'Job <\K[0-9]+')
EP=$(echo "$OUT" | grep -oP 'http://\K[^/ ]+')
[ -z "$JID" ] && fail "second start failed: $OUT"
echo "RUN: $NAME jobid=$JID endpoint=$EP"

bservices -d $EP >/dev/null 2>&1 || fail "delete by endpoint $EP failed"
wait_stat $JID FINISH 15 || fail "expected FINISH got $(svc_stat $JID)"

echo "PASS: $NAME"
exit 0
