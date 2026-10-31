#!/bin/bash
# tests/sim/simqa.sh
#
# Robustness scenarios on a one box sim cluster: pid timeout, host
# taint, orphan jobs, bkill -s 9, finished job retention, a suspended
# job across mbd restarts, gpus of a multi-host job.
#
# The script builds its own config in a scratch directory, starts mbd
# as SIMQA_USER and two sim sbds, worker1 and worker2, cloned from the
# real host SIMQA_HOST, which has no sbd and stays unavail. Every
# scenario prints PASS or FAIL, the summary is at the end.
#
# Run as root: sim sbds run as root, mbd refuses to.
#   SIMQA_USER   job owner and LavaLite admin, default lavalite
#   SIMQA_HOST   real host, must resolve to the address clients
#                connect from, default localhost
#   SIMQA_BIN    build tree src/batch, default next to this script
#   SIMQA_PORT   base port, default 43124
#   SIMQA_KEEP   1 keeps the scratch directory

U=${SIMQA_USER:-lavalite}
H=${SIMQA_HOST:-localhost}
BIN=${SIMQA_BIN:-$(cd "$(dirname "$0")/../../batch" && pwd)}
P=${SIMQA_PORT:-43124}
QA=$(mktemp -d /tmp/simqa.XXXXXX)
CONF=$QA/conf
PASS=0
FAIL=0

mkdir -p $CONF $QA/state $QA/log
cp "$(dirname "$0")/../../../etc/ll.auth.key" $CONF/ 2>/dev/null ||
    echo "simqa-test-key-0123456789abcdef" > $CONF/ll.auth.key

# ---------------------------------------------------------------- config

conf_ll()   # $1 threshold, $2 period
{
    cat > $CONF/ll.conf <<END
LL_CLUSTER_NAME=simqa
LL_STATE_DIR=$QA/state
LL_LOG_DIR=$QA/log
LL_LOG_MASK=LOG_DEBUG
LL_MBD_PORT=$P
LL_MBD_HOST=$H
LL_MBD_USER=$U
LL_DEFAULT_QUEUE=normal
LL_ASSERT_COUNTERS=1
LL_MBD_JOB_FINISH_THRESHOLD=$1
LL_MBD_JOB_FINISH_PERIOD=$2
END
}

conf_hosts()   # $1 sims, e.g. "worker1 worker2", $2 1 = with token pool
{
    local s
    {
        echo "Begin Host"
        echo "HOST_NAME CPU MEM STORAGE GPU_MODEL GPU_IDS"
        echo "$H 4 8G 10G - -"
        echo "End Host"
        if [ "$2" = 1 ]; then
            echo "Begin TokenPool"
            echo "POOL_NAME AVAILABLE"
            echo "lic 2"
            echo "End TokenPool"
        fi
        echo "Begin Sim"
        echo "NAME REAL_HOST CPU MEM STORAGE GPU_MODEL GPU_IDS"
        for s in $1; do
            echo "$s $H 4 8G 10G H100 0-1"
        done
        echo "End Sim"
    } > $CONF/llb.hosts
}

conf_queues()   # $1 hosts, $2 1 = with queue extra
{
    {
        printf "Begin Queue\nQUEUE_NAME = normal\nPRIORITY = 10\n"
        printf "HOSTS = %s\nDESCRIPTION = normal\nEnd Queue\n" "$1"
        if [ "$2" = 1 ]; then
            printf "Begin Queue\nQUEUE_NAME = extra\nPRIORITY = 20\n"
            printf "HOSTS = %s\nDESCRIPTION = extra\nEnd Queue\n" "$1"
        fi
    } > $CONF/llb.queues
}

conf_full()
{
    conf_hosts "worker1 worker2" 1
    conf_queues "$H worker1 worker2" 1
}

cat > $CONF/ll.cluster.simqa <<END
Begin Cluster
ClusterName
simqa
End Cluster

Begin ClusterAdmin
Administrators
$U
End ClusterAdmin

Begin Host
Hostname Resources Master
$H (cs) y
End Hosts
END
: > $CONF/llb.services

# ---------------------------------------------------------------- daemons

as_user()
{
    su "$U" -s /bin/bash -c \
        "export LL_CONF_DIR=$CONF PATH=$BIN/cmd:\$PATH; cd /tmp; $*"
}

mbd_start()
{
    chown -R "$U" $QA
    su "$U" -s /bin/bash -c \
        "LL_CONF_DIR=$CONF $BIN/mbd/mbd >>$QA/mbd.out 2>&1 & echo \$! > $QA/mbd.pid"
    sleep 3
}

mbd_stop()
{
    kill "$(cat $QA/mbd.pid)" 2>/dev/null
    sleep 1
}

mbd_restart()
{
    mbd_stop
    mbd_start
}

sbd_start()   # $1 sim name
{
    LL_CONF_DIR=$CONF $BIN/sbd/sbd -s "$1" \
        >>$QA/sbd.$1.out 2>&1 &
    echo $! > $QA/sbd.$1.pid
    disown
}

sbd_stop()  { kill "$(cat $QA/sbd.$1.pid)" 2>/dev/null; }
sbd_crash() { kill -KILL "$(cat $QA/sbd.$1.pid)" 2>/dev/null; }
sbd_pause() { kill -STOP "$(cat $QA/sbd.$1.pid)"; }
sbd_cont()  { kill -CONT "$(cat $QA/sbd.$1.pid)"; }

teardown()
{
    for s in worker1 worker2; do
        kill -CONT "$(cat $QA/sbd.$s.pid 2>/dev/null)" 2>/dev/null
        sbd_stop $s
    done
    mbd_stop
    pkill -u "$U" -f "sleep 9[0-9][0-9][0-9]" 2>/dev/null
    [ "$SIMQA_KEEP" = 1 ] && echo "kept $QA" || rm -rf $QA
}
trap teardown EXIT

# ---------------------------------------------------------------- helpers

pass() { echo "PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "FAIL: $1: $2"; FAIL=$((FAIL + 1)); }

submit()   # args to bsub, prints the job id
{
    as_user bsub -o /dev/null -e /dev/null "$@" 2>&1 | grep -oP 'Job <\K[0-9]+'
}

jstate()  { as_user bjobs "$1" 2>/dev/null | awk 'NR==2 {print $3}'; }
jhost()   { as_user bjobs "$1" 2>/dev/null | awk 'NR==2 {print $6}'; }
hstate()  { as_user bhosts 2>/dev/null | awk -v h=$1 '$1 == h {print $2}'; }
hjobs()   { as_user bhosts 2>/dev/null | awk -v h=$1 '$1 == h {print $7}'; }
tused()   { as_user btokens 2>/dev/null | awk '$1 == "lic" {print $3}'; }
mlog()    { cat $QA/log/mbd.log.* 2>/dev/null; }
slog()    { cat $QA/log/$1.log.* 2>/dev/null; }
alive()   { pgrep -u "$U" -f "sleep $1\$" >/dev/null; }

wait_state()   # job state seconds
{
    for i in $(seq 1 $3); do
        [ "$(jstate $1)" = "$2" ] && return 0
        sleep 1
    done
    return 1
}

wait_host()   # host state seconds
{
    for i in $(seq 1 $3); do
        [ "$(hstate $1)" = "$2" ] && return 0
        sleep 1
    done
    return 1
}

wait_log()   # regex seconds [sbd name]
{
    for i in $(seq 1 $2); do
        if [ -n "$3" ]; then
            slog $3 | grep -qE "$1" && return 0
        else
            mlog | grep -qE "$1" && return 0
        fi
        sleep 1
    done
    return 1
}

# ---------------------------------------------------------------- scenarios

t_pid_timeout()
{
    local T=pid_timeout
    sbd_pause worker1
    local j=$(submit --machines worker1 sleep 9101)
    wait_state $j DISP 10 || { fail $T "no DISP, state=$(jstate $j)"; sbd_cont worker1; return; }
    wait_state $j UNKNOWN 60 || { fail $T "no UNKNOWN in 60s"; sbd_cont worker1; return; }
    [ "$(hstate worker1)" = unavail ] || { fail $T "host not unavail"; sbd_cont worker1; return; }
    mlog | grep -q "job=$j no pid from sbd=worker1" || { fail $T "no timeout log"; sbd_cont worker1; return; }
    sbd_cont worker1
    wait_state $j RUN 120 || { fail $T "no RUN after resume, state=$(jstate $j)"; return; }
    [ "$(jhost $j)" = 1@worker1 ] || { fail $T "moved to $(jhost $j)"; return; }
    as_user bkill -s 9 $j >/dev/null
    pass $T
}

t_pid_timeout_mbd_restart()
{
    local T=pid_timeout_mbd_restart
    sbd_pause worker1
    local j=$(submit --machines worker1 sleep 9102)
    wait_state $j DISP 10 || { fail $T "no DISP"; sbd_cont worker1; return; }
    mbd_restart
    local before=$(mlog | grep -c "job=$j no pid")
    sleep 45
    local after=$(mlog | grep -c "job=$j no pid")
    sbd_cont worker1
    [ "$before" = "$after" ] || { fail $T "timed out while sbd disconnected"; return; }
    wait_state $j RUN 120 || { fail $T "no RUN after resume, state=$(jstate $j)"; return; }
    as_user bkill -s 9 $j >/dev/null
    pass $T
}

t_taint()
{
    local T=taint
    local jd=$QA/state/sbd/worker1/jobs
    mv $jd $jd.save && touch $jd
    local j=$(submit --machines worker1 sleep 9201)
    wait_log "job=$j rejected by sbd" 15 ||
        { fail $T "no reject"; rm -f $jd; mv $jd.save $jd; return; }
    wait_host worker1 "ok|tainted" 5 ||
        { fail $T "host=$(hstate worker1)"; rm -f $jd; mv $jd.save $jd; return; }
    sleep 10    # still broken: no retry while tainted
    local n=$(mlog | grep -c "job=$j flags=.* dispatched to host=worker1")
    rm -f $jd; mv $jd.save $jd
    [ "$(jstate $j)" = PEND ] || { fail $T "job not PEND while tainted"; return; }
    [ "$n" = 1 ] || { fail $T "dispatched $n times while tainted"; return; }
    wait_host worker1 ok 75 || { fail $T "taint did not expire"; return; }
    wait_state $j RUN 15 || { fail $T "no RUN after taint, state=$(jstate $j)"; return; }
    as_user bkill -s 9 $j >/dev/null
    pass $T
}

t_kill9_running()
{
    local T=kill9_running
    local j=$(submit --machines worker1 sleep 9301)
    wait_state $j RUN 15 || { fail $T "no RUN"; return; }
    as_user bkill -s 9 $j >/dev/null
    [ "$(jstate $j)" = EXIT ] || { fail $T "not EXIT at once, state=$(jstate $j)"; return; }
    [ "$(hjobs worker1)" = 0 ] || { fail $T "worker1 njobs=$(hjobs worker1)"; return; }
    wait_log "job=$j finish_acked and freed" 15 worker1 || { fail $T "sbd did not free"; return; }
    alive 9301 && { fail $T "process alive"; return; }
    pass $T
}

t_kill9_disp()
{
    local T=kill9_disp
    sbd_pause worker1
    local j=$(submit --machines worker1 sleep 9302)
    wait_state $j DISP 10 || { fail $T "no DISP"; sbd_cont worker1; return; }
    as_user bkill -s 9 $j >/dev/null
    [ "$(jstate $j)" = EXIT ] || { fail $T "not EXIT"; sbd_cont worker1; return; }
    sbd_cont worker1
    wait_log "job=$j finished, pid=.* acked and discarded" 15 ||
        { fail $T "pid reply not acked and discarded"; return; }
    wait_log "job=$j finish_acked and freed" 15 worker1 || { fail $T "sbd did not free"; return; }
    alive 9302 && { fail $T "process alive"; return; }
    pass $T
}

# A paused sbd with only running jobs is not noticed: the pid timeout
# covers DISP jobs only. Disconnect for real: sbd dies, its job lives
# on, sbd restarts and recovers the job from its state files.
t_kill9_disconnected()
{
    local T=kill9_disconnected
    local j=$(submit --machines worker1 sleep 30)
    wait_state $j RUN 15 || { fail $T "no RUN"; return; }
    sbd_crash worker1
    wait_state $j UNKNOWN 15 || { fail $T "no UNKNOWN"; sbd_start worker1; return; }
    as_user bkill -s 9 $j >/dev/null
    [ "$(jstate $j)" = EXIT ] || { fail $T "not EXIT"; sbd_start worker1; return; }
    sbd_start worker1
    wait_host worker1 ok 60 || { fail $T "worker1 not back"; return; }
    # documented: the kill never reached sbd, the job ends on its own
    wait_log "job=$j finish duplicate" 60 || { fail $T "finish not acked"; return; }
    wait_log "job=$j finish_acked and freed" 15 worker1 || { fail $T "sbd did not free"; return; }
    pass $T
}

t_kill9_all()
{
    local T=kill9_all
    local a=$(submit --machines worker1 sleep 9311)
    local b=$(submit --machines worker2 sleep 9312)
    local c=$(submit --machines $H sleep 9313)
    wait_state $a RUN 15 && wait_state $b RUN 15 || { fail $T "no RUN"; return; }
    as_user bkill -s 9 0 >/dev/null
    for j in $a $b $c; do
        [ "$(jstate $j)" = EXIT ] || { fail $T "job=$j state=$(jstate $j)"; return; }
    done
    sleep 3
    alive 9311 || alive 9312 && { fail $T "process alive"; return; }
    pass $T
}

# orphans: one config mistake, checked from several sides, then healed
t_orphans()
{
    local T
    local run_q=$(submit --queue extra --machines worker1 --tokens lic=1 sleep 25)
    local pend_q=$(submit --queue extra --machines $H sleep 9401)
    local run_h=$(submit --queue normal --machines worker2 sleep 9402)
    local tok=$(submit --queue normal --machines worker1 --tokens lic=1 sleep 9403)
    wait_state $run_q RUN 15 && wait_state $run_h RUN 15 && wait_state $tok RUN 15 ||
        { fail orphans "setup no RUN"; return; }

    # the mistake: queue extra and host worker2 removed
    mbd_stop
    conf_hosts "worker1" 1
    conf_queues "$H worker1" 0
    mbd_start

    T=orphan_marked
    wait_host worker1 ok 30     # the non orphan is UNKNOWN until sbd is back
    [ "$(jstate $run_q)" = ORPHAN ] && [ "$(jstate $pend_q)" = ORPHAN ] &&
    [ "$(jstate $run_h)" = ORPHAN ] && [ "$(jstate $tok)" = RUN ] &&
        pass $T || fail $T "states $(jstate $run_q) $(jstate $pend_q) $(jstate $run_h) $(jstate $tok)"

    T=orphan_frozen
    local r1=$(as_user bkill $run_q 2>&1)
    local r2=$(as_user bmove --to normal $pend_q 2>&1)
    local r3=$(as_user bpriority --priority 1 $pend_q 2>&1)
    [ "$(printf '%s\n%s\n%s\n' "$r1" "$r2" "$r3" | grep -c "not permitted")" = 3 ] &&
        pass $T || fail $T "refusals: $r1 / $r2 / $r3"

    T=orphan_counted
    [ "$(hjobs worker1)" = 2 ] && [ "$(tused)" = 2 ] &&
        pass $T || fail $T "worker1 njobs=$(hjobs worker1) lic used=$(tused)"

    T=orphan_slots_held
    local f1=$(submit --machines worker1 sleep 9411)
    local f2=$(submit --machines worker1 sleep 9412)
    local f3=$(submit --machines worker1 sleep 9413)
    sleep 6
    [ "$(jstate $f1)" = RUN ] && [ "$(jstate $f2)" = RUN ] && [ "$(jstate $f3)" = PEND ] &&
        pass $T || fail $T "fill $(jstate $f1) $(jstate $f2) $(jstate $f3)"

    T=orphan_host_no_register
    wait_log "register from unknown host worker2" 30 &&
        pass $T || fail $T "worker2 sbd not refused"

    T=orphan_finish_frozen
    wait_log "job=$run_q is orphan, finish from sbd=worker1 not acked" 40 &&
        pass $T || fail $T "finish of frozen job not seen"

    # the fix: config restored
    mbd_stop
    conf_full
    mbd_start

    T=orphan_healed
    wait_state $run_q DONE 30 && wait_state $run_h RUN 30 && [ "$(jstate $pend_q)" = PEND ] &&
        pass $T || fail $T "after heal $(jstate $run_q) $(jstate $run_h) $(jstate $pend_q)"

    T=orphan_kill9
    mbd_stop
    conf_hosts "worker1" 1
    conf_queues "$H worker1" 0
    mbd_start
    as_user bkill -s 9 $run_h $pend_q >/dev/null
    [ "$(jstate $run_h)" = EXIT ] && [ "$(jstate $pend_q)" = EXIT ] &&
        pass $T || fail $T "states $(jstate $run_h) $(jstate $pend_q)"

    mbd_stop
    conf_full
    mbd_start
    as_user bkill -s 9 $tok $f1 $f2 $f3 >/dev/null
}

t_token_pool_orphan()
{
    local T=token_pool_orphan
    local j=$(submit --machines worker1 --tokens lic=1 sleep 9501)
    wait_state $j RUN 15 || { fail $T "no RUN"; return; }
    mbd_stop
    conf_hosts "worker1 worker2" 0
    mbd_start
    local s=$(jstate $j)
    local raw=$(as_user bjobs $j 2>&1 | tr '\n' '|')
    mbd_stop
    conf_full
    mbd_start
    [ "$s" = ORPHAN ] && wait_state $j RUN 15 &&
        pass $T || fail $T "orphan=$s healed=$(jstate $j) bjobs=$raw"
    as_user bkill -s 9 $j >/dev/null
}

t_replay_machines_tokens()
{
    local T=replay_machines_tokens
    local j=$(submit --machines $H --tokens lic=1 sleep 9601)
    local d=$(submit --machines worker1 true)
    wait_state $d DONE 20 || { fail $T "no DONE"; return; }
    # threshold 1 period 0: the restart compacts at once
    mbd_stop
    conf_ll 1 0
    mbd_start
    mbd_restart
    local m=$(awk -v j=$j '$1 == "JOB_NEW" && $4 == j' $QA/state/mbd/manifest)
    conf_ll 1000 3600
    mbd_restart
    echo "$m" | grep -q "\"$H\" \"lic=1\"" && [ "$(jstate $j)" = PEND ] &&
        pass $T || fail $T "manifest: $m"
    as_user bkill -s 9 $j >/dev/null
}

t_retention()
{
    local T=retention
    # start from an empty finished list: period 0 drops all at restart
    mbd_stop
    conf_ll 1 0
    mbd_start
    mbd_stop
    conf_ll 2 20
    mbd_start
    local a=$(submit --machines worker1 true)
    local b=$(submit --machines worker1 true)
    wait_state $b DONE 20 || { fail $T "no DONE"; return; }
    sleep 22
    local c=$(submit --machines worker2 true)
    local d=$(submit --machines worker2 true)
    wait_state $d DONE 20 || { fail $T "no DONE"; return; }
    # a restart checks the threshold at once: a b expired, c d young
    mbd_restart
    local done=$(as_user bjobs --done 2>/dev/null | awk 'NR > 1 {print $1}' | sort -n | tr '\n' ' ')
    mlog | grep -q "dropped=2 kept=2" &&
    [ "$done" = "$c $d " ] &&
        pass $T || fail $T "done list: $done"
    mbd_stop
    conf_ll 1000 3600
    mbd_start
}

t_suspend_restart()
{
    local T=suspend_restart
    # threshold 1 period 0: every restart compacts
    mbd_stop
    conf_ll 1 0
    mbd_start
    wait_host worker1 ok 20 && wait_host worker2 ok 20
    submit true >/dev/null   # something finished to compact away
    # j on one host, k on both: stop and cont must move the counters of
    # every host of k, not only its first one
    local j=$(submit -m worker1 sleep 8)
    local k=$(submit --nhosts 2 sleep 9)
    wait_state $j RUN 20 && wait_state $k RUN 20 ||
        { fail $T "no RUN"; return; }
    as_user bkill -s stop $j >/dev/null
    as_user bkill -s stop $k >/dev/null
    wait_state $j SUSP 10 && wait_state $k SUSP 10 ||
        { fail $T "no SUSP: $(jstate $j) $(jstate $k)"; return; }
    mbd_restart
    wait_host worker1 ok 20
    mbd_restart
    wait_host worker1 ok 20
    [ "$(jstate $j) $(jstate $k)" = "SUSP SUSP" ] ||
        { fail $T "after restart: $(jstate $j) $(jstate $k)"; return; }
    # frozen: past their 8 and 9 seconds they are still there
    sleep 11
    alive 8 && alive 9 || { fail $T "ran while suspended"; return; }
    as_user bkill -s cont $j >/dev/null
    as_user bkill -s cont $k >/dev/null
    wait_state $j DONE 30 && wait_state $k DONE 30 ||
        { fail $T "no DONE: $(jstate $j) $(jstate $k)"; return; }
    local ev="$( (as_user bhist $j; as_user bhist $k) | grep -oE 'Suspended|Resumed' | tr '\n' ' ')"
    [ "$ev" = "Suspended Resumed Suspended Resumed " ] &&
        pass $T || fail $T "bhist events: $ev"
    mbd_stop
    conf_ll 1000 3600
    mbd_start
}

t_gpu_multihost()
{
    local T=gpu_multihost
    # a job on both sims with one gpu each must hold a gpu on both, not
    # only on the first one: mbd_assert_counters() checks the gpu ids.
    # The jobs write their environment: CUDA_VISIBLE_DEVICES is set on
    # a job of one host only
    mkdir -p $QA/env; chmod 777 $QA/env
    printf '#!/bin/sh\nenv > %s/env/$1\nsleep $2\n' $QA > $QA/env.sh
    chmod 755 $QA/env.sh
    local a=$(submit -m worker1 --gpus 1 $QA/env.sh a 8)
    local b=$(submit -m worker2 --gpus 1 $QA/env.sh b 8)
    wait_state $a RUN 20 && wait_state $b RUN 20 ||
        { fail $T "no RUN"; return; }
    local k=$(submit --nhosts 2 --gpus 1 $QA/env.sh k 3)
    wait_state $k RUN 20 || { fail $T "k no RUN: $(jstate $k)"; return; }
    wait_state $k DONE 20 || { fail $T "k no DONE: $(jstate $k)"; return; }
    wait_state $a DONE 20 && wait_state $b DONE 20 ||
        { fail $T "no DONE"; return; }
    grep -q '^CUDA_VISIBLE_DEVICES=' $QA/env/a &&
    ! grep -q '^CUDA_VISIBLE_DEVICES=' $QA/env/k ||
        { fail $T "CUDA_VISIBLE_DEVICES, see $QA/env"; return; }
    kill -0 "$(cat $QA/mbd.pid)" 2>/dev/null &&
    ! mlog | grep -q "bad counters" &&
        pass $T || fail $T "mbd died or bad counters, see $QA"
}

t_counters()
{
    local T=counters_and_alive
    kill -0 "$(cat $QA/mbd.pid)" 2>/dev/null &&
    ! mlog | grep -q "bad counters" &&
    ! grep -q "Assertion" $QA/mbd.out &&
        pass $T || fail $T "mbd died or counters off, see $QA"
}

# The system tests of src/tests/system against this sim cluster, as a
# regression run. Not in the default list: SIMQA_ONLY=t_system.
t_system()
{
    local sys=$(cd "$(dirname "$0")/../system" && pwd)
    as_user "LL_TEST_HOST=worker1 bash $sys/run_tests.sh" > $QA/system.out 2>&1
    grep -E "^(PASS|FAIL|SKIP)" $QA/system.out | sed 's/^/  system /'
    grep -q "^FAIL" $QA/system.out &&
        fail system "see $QA/system.out" || pass system
}

# one test's leftovers never leak into the next: kill every job, wait
# for both workers ok, a taint expires within 60 seconds
settle()
{
    as_user bkill -s 9 0 >/dev/null 2>&1
    wait_host worker1 ok 75
    wait_host worker2 ok 75
}

# ---------------------------------------------------------------- main

[ "$(id -u)" = 0 ] || { echo "simqa: run as root"; exit 1; }
id "$U" >/dev/null 2>&1 || { echo "simqa: no user $U"; exit 1; }
chmod 755 $QA

conf_ll 1000 3600
conf_full
mbd_start
sbd_start worker1
sbd_start worker2
wait_host worker1 ok 20 && wait_host worker2 ok 20 ||
    { echo "simqa: cluster did not come up, see $QA"; SIMQA_KEEP=1; exit 1; }

for t in ${SIMQA_ONLY:-t_kill9_running t_kill9_disp t_kill9_all \
         t_pid_timeout t_pid_timeout_mbd_restart t_kill9_disconnected \
         t_taint t_orphans t_token_pool_orphan t_replay_machines_tokens \
         t_retention t_suspend_restart t_gpu_multihost t_counters}; do
    settle
    $t
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ] || SIMQA_KEEP=1
[ $FAIL -eq 0 ]
