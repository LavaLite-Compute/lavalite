#!/bin/bash
# tests/system/bsub_tokens.sh
NAME="bsub_tokens"
fail() {
    echo "FAIL $NAME: $1"
    exit 1
}

TOKENS=$(btokens | awk 'NR>1 {print $1}')
[ -z "$TOKENS" ] && fail "no tokens returned by btokens"

for TOK in $TOKENS; do
    JID=$(bsub --tokens "${TOK}=1" -o /dev/null -e /dev/null true 2>&1 | grep -oP 'Job <\K[0-9]+')
    [ -z "$JID" ] && fail "no jobid returned for token $TOK"
    echo "RUN: $NAME token=$TOK jobid=$JID"

    STATE=""
    for i in $(seq 1 10); do
        STATE=$(bjobs "$JID" 2>/dev/null | awk 'NR==2 {print $3}')
        [ "$STATE" = "DONE" ] && break
        sleep 1
    done
    [ "$STATE" != "DONE" ] && fail "timeout waiting for DONE on token $TOK, last state=$STATE"

    bhist "$JID" 2>/dev/null | grep -q "$TOK" || fail "token $TOK not found in bhist"
done

echo "PASS: $NAME"
exit 0
