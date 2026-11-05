#!/bin/bash
# tests/system/bsub_array.sh

NAME="bsub_array"
N=5

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

# --array start must be >= 1 (0 collides with the bkill N[0]
# sentinel — see llb_parse_array() in submit.c). Both forms below
# must be rejected before submission, not accepted and expanded.
OUT=$(bsub -o /dev/null --array 0-2 sleep 120 2>&1)
[ $? -eq 0 ] && fail "start=0 array was accepted, should be rejected"
echo "$OUT" | grep -q "invalid spec" || fail "start=0 rejection message unexpected: $OUT"

OUT=$(bsub -o /dev/null --array -1-2 sleep 120 2>&1)
[ $? -eq 0 ] && fail "negative start array was accepted, should be rejected"
echo "$OUT" | grep -q "invalid spec" || fail "negative start rejection message unexpected: $OUT"

JID=$(bsub --array 1-$N -o /dev/null -e /dev/null true 2>&1 \
     | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID"

# bjobs (no id) lists the caller's active jobs; array elements show
# as JID[index]. (bjobs JID / bhist JID also aggregate on the array
# id and would list all elements, but polling active-job output
# avoids parsing bhist's multi-line per-element format here.) Poll
# until none of this array's elements are active any more, i.e. all
# N have left PEND/RUN/HELD/SUSP.
for i in $(seq 1 15); do
    ACTIVE=$(bjobs 2>/dev/null | grep -c "^${JID}\[")
    [ "$ACTIVE" -eq 0 ] && break
    sleep 1
done

[ "$ACTIVE" -ne 0 ] && fail "timeout waiting for array to finish, $ACTIVE elements still active"

# JID is also the first element's own job_id and array_id; bhist on
# an array_id aggregates every element in one call, one "Status <..>"
# line per element.
NDONE=$(bhist "$JID" 2>/dev/null | grep -c "Status <DONE>")
[ "$NDONE" -eq "$N" ] || fail "expected $N elements DONE in bhist, got $NDONE"

echo "PASS: $NAME"
exit 0
