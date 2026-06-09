#!/bin/bash
# tests/system/run_tests.sh

TESTS_DIR=$(dirname $0)
PASS=0
FAIL=0

run_test() {
    local script=$1
    local out
    out=$(bash "$script" 2>&1)
    echo "$out"
    if echo "$out" | grep -q "^PASS"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
}

run_test $TESTS_DIR/bsub_basic.sh
run_test $TESTS_DIR/bsub_hold.sh
run_test $TESTS_DIR/bsub_nhosts.sh
run_test $TESTS_DIR/bsub_gpu.sh
run_test $TESTS_DIR/bsub_exclusive.sh

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ $FAIL -eq 0 ]
