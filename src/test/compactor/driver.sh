#!/bin/sh
set -e

CLEAN=60
./gen_events.py --num-jobs 20 --clean-period $CLEAN --output lsb.events

../build/mbd_compact \
    --events lsb.events \
    --threshold 1 \
    --interval 1 \
    --clean-period $CLEAN \
    --logdir . \
    --log-mask LOG_INFO &
PID=$!

sleep 2
kill $PID || true
wait $PID || true

./verify.py --events lsb.events --clean-period $CLEAN
