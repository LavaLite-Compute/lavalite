#!/bin/sh
#
# perf20.sh - bperf numbers for 2.0.0, the 1.1.0 method of
# docs/admin/04-performance.md.
#
# Run as the user that submits, with sudo for systemctl and the state
# directory. LL_STATE_DIR comes from ll.conf, bperf is next to this
# script. Before: LL_LOG_MASK=LOG_INFO, LL_ARRAY_MAX_SIZE=16000, no
# LL_ASSERT_COUNTERS in ll.conf.
#
# Every size starts from an empty mbd: our jobs are killed and must
# be finished on every sbd, then the content of the mbd state
# directory is removed.
#
#   perf20.sh 2>&1 | tee perf20.out

[ -n "$LL_CONF_DIR" ] || { echo "perf20: LL_CONF_DIR must be defined"; exit 1; }

conf()
{
    sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "$LL_CONF_DIR/ll.conf" | tail -1
}

STATE_DIR=$(conf LL_STATE_DIR)
BPERF=$(dirname "$0")/bperf

[ -n "$STATE_DIR" ] || { echo "perf20: no LL_STATE_DIR in ll.conf"; exit 1; }
[ -d "$STATE_DIR/mbd" ] || { echo "perf20: no $STATE_DIR/mbd"; exit 1; }
[ -f "$BPERF" ] || { echo "perf20: no $BPERF"; exit 1; }

# kill our jobs, wait until no job of ours is left anywhere
drain()
{
    bkill 0 >/dev/null 2>&1
    for i in $(seq 1 60); do
        bjobs 2>&1 | grep -q 'No jobs found' && return 0
        sleep 2
    done
    echo "perf20: jobs still active after 120s"
    exit 1
}

fresh_mbd()
{
    sudo systemctl stop lavalite-mbd
    sudo rm -rf "$STATE_DIR/mbd/"*
    sudo systemctl start lavalite-mbd
    sleep 5
}

for n in 1000 6000 16000; do
    echo "=== N=$n $(date '+%F %T')"
    drain
    fresh_mbd

    t0=$(date +%s.%N)
    bsub -o /dev/null -e /dev/null --array 1-$n -H sleep 4
    t1=$(date +%s.%N)
    echo "array_submit N=$n secs=$(echo "$t1 - $t0" | bc)"

    for run in 1 2 3; do
        sleep 5
        echo "--- run $run"
        python3 "$BPERF" --bjobs 100 --bhist 100
    done

    sleep 5
    echo "--- submit"
    python3 "$BPERF" --submit 100
done

drain
