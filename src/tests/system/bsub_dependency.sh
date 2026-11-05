#!/bin/bash
# tests/system/bsub_dependency.sh
NAME="bsub_dependency"

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

jobid_of() {
    grep -oP 'Job <\K[0-9]+'
}

# Wait until JID no longer shows up in the active bjobs listing
# (PEND/RUN/HELD/SUSP) -- same technique as bsub_array.sh.
wait_active() {
    local jid=$1
    local timeout=$2
    for i in $(seq 1 "$timeout"); do
        bjobs 2>/dev/null | grep -q "^${jid} " || return 0
        sleep 1
    done
    return 1
}

# NOTE: assumes bjobs' STAT column is field 3, standard LSF-style
# layout (JOBID USER STAT QUEUE ...). Adjust the awk field number
# here if this build's bjobs differs.
job_stat() {
    bjobs "$1" 2>/dev/null | awk -v j="$1" '$1==j{print $3}'
}

# -----------------------------------------------------------------
# 1. basic done(): B must not run before A finishes
# -----------------------------------------------------------------
JID_A=$(bsub -o /dev/null -e /dev/null sleep 5 2>&1 | jobid_of)
[ -z "$JID_A" ] && fail "no jobid returned for A"

JID_B=$(bsub -o /dev/null -e /dev/null --dependency "done($JID_A)" true 2>&1 \
       | jobid_of)
[ -z "$JID_B" ] && fail "no jobid returned for B"

echo "RUN: $NAME jobA=$JID_A jobB=$JID_B (basic done())"

[ "$(job_stat "$JID_B")" = "PEND" ] \
    || fail "job=$JID_B expected PEND immediately after submit"

wait_active "$JID_A" 15 || fail "timeout waiting for A=$JID_A to finish"
wait_active "$JID_B" 15 || fail "timeout waiting for B=$JID_B to finish"

bhist "$JID_A" 2>/dev/null | grep -q "Status <DONE>" \
    || fail "job=$JID_A expected DONE in bhist"
bhist "$JID_B" 2>/dev/null | grep -q "Status <DONE>" \
    || fail "job=$JID_B expected DONE in bhist"

# -----------------------------------------------------------------
# 2. bkill on a dependency target: the depender must never unblock.
# This is the LSF-family behavior confirmed during design -- an
# impossible dependency stays PEND, it does not auto-fail.
# -----------------------------------------------------------------
JID_C=$(bsub -o /dev/null -e /dev/null sleep 60 2>&1 | jobid_of)
[ -z "$JID_C" ] && fail "no jobid returned for C"

JID_D=$(bsub -o /dev/null -e /dev/null --dependency "done($JID_C)" true 2>&1 \
       | jobid_of)
[ -z "$JID_D" ] && fail "no jobid returned for D"

echo "RUN: $NAME jobC=$JID_C jobD=$JID_D (bkill on dependency target)"

bkill "$JID_C" >/dev/null 2>&1 || fail "bkill C=$JID_C failed"
wait_active "$JID_C" 15 || fail "timeout waiting for C=$JID_C to exit"
bhist "$JID_C" 2>/dev/null | grep -q "Status <EXIT>" \
    || fail "job=$JID_C expected EXIT in bhist"

# give the scheduler a few cycles, then confirm D is still stuck --
# not silently unblocked by a missing/purged target
sleep 3
[ "$(job_stat "$JID_D")" = "PEND" ] \
    || fail "job=$JID_D expected to stay PEND forever after target killed"

bkill "$JID_D" >/dev/null 2>&1 || fail "cleanup bkill D=$JID_D failed"
wait_active "$JID_D" 15 || fail "timeout waiting for D=$JID_D cleanup"

# -----------------------------------------------------------------
# 3. bkill on the depender itself while still pending. Exercises
# finish_pending_job()'s job_deps_release() path -- E's dep_refcnt
# must drop back so E is purgeable like any other finished job with
# nothing left depending on it.
# -----------------------------------------------------------------
JID_E=$(bsub -o /dev/null -e /dev/null sleep 60 2>&1 | jobid_of)
[ -z "$JID_E" ] && fail "no jobid returned for E"

JID_F=$(bsub -o /dev/null -e /dev/null --dependency "done($JID_E)" true 2>&1 \
       | jobid_of)
[ -z "$JID_F" ] && fail "no jobid returned for F"

echo "RUN: $NAME jobE=$JID_E jobF=$JID_F (bkill on depender while pending)"

[ "$(job_stat "$JID_F")" = "PEND" ] \
    || fail "job=$JID_F expected PEND before bkill"

bkill "$JID_F" >/dev/null 2>&1 || fail "bkill F=$JID_F failed"
wait_active "$JID_F" 15 || fail "timeout waiting for F=$JID_F to exit"
bhist "$JID_F" 2>/dev/null | grep -q "Status <EXIT>" \
    || fail "job=$JID_F expected EXIT in bhist"

bkill "$JID_E" >/dev/null 2>&1 || fail "cleanup bkill E=$JID_E failed"
wait_active "$JID_E" 15 || fail "timeout waiting for E=$JID_E cleanup"

# -----------------------------------------------------------------
# 4. compound &&: G must wait for both H and I, not just the first
# one to finish.
# -----------------------------------------------------------------
JID_H=$(bsub -o /dev/null -e /dev/null sleep 3 2>&1 | jobid_of)
JID_I=$(bsub -o /dev/null -e /dev/null sleep 6 2>&1 | jobid_of)
[ -z "$JID_H" ] && fail "no jobid returned for H"
[ -z "$JID_I" ] && fail "no jobid returned for I"

JID_G=$(bsub -o /dev/null -e /dev/null \
       --dependency "done($JID_H) && done($JID_I)" true 2>&1 | jobid_of)
[ -z "$JID_G" ] && fail "no jobid returned for G"

echo "RUN: $NAME jobH=$JID_H jobI=$JID_I jobG=$JID_G (compound &&)"

wait_active "$JID_H" 10 || fail "timeout waiting for H=$JID_H to finish"

# H is done, I is not yet -- G must still be PEND
[ "$(job_stat "$JID_G")" = "PEND" ] \
    || fail "job=$JID_G expected PEND with only one of two deps done"

wait_active "$JID_I" 15 || fail "timeout waiting for I=$JID_I to finish"
wait_active "$JID_G" 15 || fail "timeout waiting for G=$JID_G to finish"

bhist "$JID_G" 2>/dev/null | grep -q "Status <DONE>" \
    || fail "job=$JID_G expected DONE in bhist"

echo "PASS: $NAME"
exit 0
