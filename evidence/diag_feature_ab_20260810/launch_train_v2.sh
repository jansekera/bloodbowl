#!/bin/bash
# v2 rerun (candidate-order shuffle fix) + exploratory annex (64 passes).
# Idempotent: done markers + running-instance check.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/feature_ab
PY=/home/jan/claude/bloodbowl/venv/bin/python
cd "$DIR" || exit 1

if [ "$1" != "--inner" ]; then
    if [ -f done_train_v2_all ]; then echo "[skip] v2 done"; exit 0; fi
    if pgrep -f "launch_train_v2[.]sh --inner" > /dev/null 2>&1; then
        echo "[skip] v2 chain already running"; exit 0
    fi
    setsid nohup bash "$0" --inner > train_v2_chain.log 2>&1 < /dev/null &
    disown
    echo "[launched] v2 chain pid=$!"
    exit 0
fi

echo "v2 chain start $(date -u)"
if [ ! -f done_train_v2 ]; then
    AB_SHUFFLE=1 AB_SUFFIX=_v2 nice -n 19 "$PY" diag_train_ab.py \
        > train_ab_v2.log 2>&1 && touch done_train_v2
fi
echo "v2 main done $(date -u)"
if [ ! -f done_train_v2x ]; then
    AB_SHUFFLE=1 AB_SUFFIX=_v2_explor64 AB_PASSES=64 AB_SEEDS=1 \
        nice -n 19 "$PY" diag_train_ab.py \
        > train_ab_v2_explor64.log 2>&1 && touch done_train_v2x
fi
echo "v2 exploratory done $(date -u)"
[ -f done_train_v2 ] && [ -f done_train_v2x ] && touch done_train_v2_all
