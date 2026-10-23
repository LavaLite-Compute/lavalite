#!/bin/bash
# tests/system/bservices_time.sh
# A service stopped by its run limit is not restarted.

NAME="bservices_time"
SVC=${SVC:-echotest}

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

svc_stat() {
    bservices -a 2>/dev/null | awk -v j="$1" '$3==j {print $4}'
}

bservices 2>/dev/null | grep -q "^$SVC " \
    || { echo "SKIP: $NAME service $SVC not configured"; exit 0; }

OUT=$(timeout 60 bservices -s $SVC --time 1 2>&1)
JID=$(echo "$OUT" | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "start failed: $OUT"
echo "RUN: $NAME jobid=$JID"

for i in $(seq 1 90); do
    [ "$(svc_stat $JID)" = "FINISH" ] && break
    sleep 1
done
[ "$(svc_stat $JID)" = "FINISH" ] || fail "expected FINISH got $(svc_stat $JID)"

# give a relocation the chance to happen, it must not
sleep 5
[ "$(svc_stat $JID)" = "FINISH" ] || fail "restarted after run limit"

N=$(bhist -l $JID 2>/dev/null | grep -c "Dispatched")
[ "$N" = "1" ] || fail "expected 1 dispatch got $N"
bhist -l $JID 2>/dev/null | grep -q "Run limit: *0:01" \
    || fail "run limit not in bhist"

echo "PASS: $NAME"
exit 0
