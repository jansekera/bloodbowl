#!/bin/bash
# v3: v2 + tie-robust REPOS hit metric (hit_maxset/chance_maxset).
# Waits for the v2 chain to end so runs never overlap. Idempotent.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/feature_ab
PY=/home/jan/claude/bloodbowl/venv/bin/python
cd "$DIR" || exit 1

if [ "$1" != "--inner" ]; then
    if [ -f done_train_v3_all ]; then echo "[skip] v3 done"; exit 0; fi
    if pgrep -f "launch_train_v3[.]sh --inner" > /dev/null 2>&1; then
        echo "[skip] v3 chain already running"; exit 0
    fi
    setsid nohup bash "$0" --inner > train_v3_chain.log 2>&1 < /dev/null &
    disown
    echo "[launched] v3 chain pid=$!"
    exit 0
fi

echo "v3 chain start $(date -u)"
for i in $(seq 1 240); do
    pgrep -f "launch_train_v2[.]sh --inner" > /dev/null || break
    sleep 15
done
if [ ! -f done_train_v3 ]; then
    AB_SHUFFLE=1 AB_SUFFIX=_v3 nice -n 19 "$PY" diag_train_ab.py \
        > train_ab_v3.log 2>&1 && touch done_train_v3
fi
if [ ! -f done_train_v3x ]; then
    AB_SHUFFLE=1 AB_SUFFIX=_v3_explor64 AB_PASSES=64 AB_SEEDS=1 \
        nice -n 19 "$PY" diag_train_ab.py \
        > train_ab_v3_explor64.log 2>&1 && touch done_train_v3x
fi
[ -f done_train_v3 ] && [ -f done_train_v3x ] && touch done_train_v3_all
echo "v3 chain end $(date -u)"
