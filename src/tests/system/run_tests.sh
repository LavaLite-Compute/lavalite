#!/bin/bash
# tests/system/run_tests.sh

TESTS_DIR=$(dirname $0)
PASS=0
FAIL=0
SKIP=0

run_test() {
    local script=$1
    local out
    out=$(bash "$script" 2>&1)
    echo "$out"
    if echo "$out" | grep -q "^PASS"; then
        PASS=$((PASS + 1))
    elif echo "$out" | grep -q "^SKIP"; then
        SKIP=$((SKIP + 1))
    else
        FAIL=$((FAIL + 1))
    fi
}

run_test $TESTS_DIR/bsub_basic.sh
run_test $TESTS_DIR/bsub_hold.sh
run_test $TESTS_DIR/bsub_nhosts.sh
run_test $TESTS_DIR/bsub_gpu.sh
run_test $TESTS_DIR/bsub_queue.sh
run_test $TESTS_DIR/bsub_mem.sh
run_test $TESTS_DIR/bsub_begin.sh
run_test $TESTS_DIR/bsub_terminate.sh
run_test $TESTS_DIR/bsub_tokens.sh
run_test $TESTS_DIR/bsub_machines.sh
run_test $TESTS_DIR/bsub_io.sh
run_test $TESTS_DIR/bsub_name.sh
run_test $TESTS_DIR/bsub_project.sh
run_test $TESTS_DIR/bsub_comment.sh
run_test $TESTS_DIR/bsub_array.sh
run_test $TESTS_DIR/bsub_dependency.sh
run_test $TESTS_DIR/bsub_time.sh
run_test $TESTS_DIR/bsub_expire.sh
run_test $TESTS_DIR/bservices_basic.sh
run_test $TESTS_DIR/bservices_time.sh

echo ""
echo "Results: $PASS passed, $FAIL failed, $SKIP skipped"
[ $FAIL -eq 0 ]
