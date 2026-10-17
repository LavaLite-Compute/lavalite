#!/bin/bash
# tests/system/sbd_pid_timeout.sh
#
# A connected sbd that does not report the pid of a dispatched job
# within PID_TIMEOUT is disconnected by mbd: the job goes from DISP
# to UNKNOWN and the host to unavail. When sbd is back it registers,
# resends the pid and the job goes to RUN, on the same host.
#
# sbd is stopped before the job is submitted, so the NEW_JOB sits in
# its socket buffer and there is no timing race.
#
# Needs ssh and sudo to the execution host, like tests/chaos.
# LL_TEST_HOST selects the host, default worker1.

NAME="sbd_pid_timeout"
HOST=${LL_TEST_HOST:-worker1}

fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

job_state() {
    bjobs $1 2>/dev/null | awk 'NR==2 {print $3}'
}

host_state() {
    bhosts 2>/dev/null | awk -v h=$HOST '$1 == h {print $2}'
}

sbd_signal() {
    ssh $HOST sudo pkill -$1 -x sbd
}

sbd_signal STOP || { echo "SKIP: $NAME cannot stop sbd on $HOST"; exit 0; }
# never leave sbd stopped, whatever happens below
trap 'sbd_signal CONT' EXIT

JID=$(bsub --machines $HOST -o /dev/null -e /dev/null sleep 300 2>&1 \
      | grep -oP 'Job <\K[0-9]+')
[ -z "$JID" ] && fail "no jobid returned"
echo "RUN: $NAME jobid=$JID host=$HOST"

# dispatched, no pid: DISP
for i in $(seq 1 10); do
    STATE=$(job_state $JID)
    [ "$STATE" = "DISP" ] && break
    sleep 1
done
[ "$STATE" != "DISP" ] && fail "expected DISP got $STATE"

# PID_TIMEOUT is 20s, checked every 20s: disconnect within 40s
for i in $(seq 1 60); do
    STATE=$(job_state $JID)
    [ "$STATE" = "UNKNOWN" ] && break
    sleep 1
done
[ "$STATE" != "UNKNOWN" ] && fail "expected UNKNOWN got $STATE"

HSTATE=$(host_state)
[ "$HSTATE" != "unavail" ] && fail "expected host unavail got $HSTATE"

# sbd back: reconnect, register, pid resent
sbd_signal CONT || fail "cannot resume sbd on $HOST"

for i in $(seq 1 120); do
    STATE=$(job_state $JID)
    [ "$STATE" = "RUN" ] && break
    sleep 1
done
[ "$STATE" != "RUN" ] && fail "expected RUN after sbd resumed got $STATE"

bjobs $JID 2>/dev/null | grep -q "$HOST" || fail "job not on $HOST anymore"

HSTATE=$(host_state)
[ "$HSTATE" != "ok" ] && fail "expected host ok got $HSTATE"

bkill $JID 2>/dev/null
echo "PASS: $NAME"
exit 0
