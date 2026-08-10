#!/bin/bash
# Idempotent detached launcher: waits for the three collection done-markers,
# then runs the offline A/B training (diag_train_ab.py). Survives session end.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/feature_ab
PY=/home/jan/claude/bloodbowl/venv/bin/python
cd "$DIR" || exit 1

if [ -f done_train ]; then
    echo "[skip] training already done"
    exit 0
fi
if pgrep -f "diag_train_ab.py" > /dev/null; then
    echo "[skip] training already running"
    exit 0
fi
if [ "$1" != "--inner" ]; then
    # guard matches the ACTUAL inner cmdline "launch_train.sh --inner"
    if pgrep -f "launch_train[.]sh --inner" > /dev/null 2>&1; then
        echo "[skip] waiter already running"
        exit 0
    fi
    setsid nohup bash "$0" --inner > train_chain.log 2>&1 < /dev/null &
    disown
    echo "[launched] waiter+train chain pid=$!"
    exit 0
fi

# --- inner: waiter + training ---
echo "waiter start $(date -u)"
# belt & braces: refuse to run twice even if two waiters somehow start
if [ -f train_started ]; then
    echo "[abort] train_started marker exists (another instance owns the run)"
    exit 0
fi
touch train_started
for i in $(seq 1 480); do  # max 4 h
    if [ -f done_dwsk_a ] && [ -f done_dwsk_b ] && [ -f done_wesk ]; then
        break
    fi
    sleep 30
done
if ! { [ -f done_dwsk_a ] && [ -f done_dwsk_b ] && [ -f done_wesk ]; }; then
    echo "TIMEOUT: collection not finished within 4 h" >&2
    exit 1
fi
echo "collection complete $(date -u); starting training"
nice -n 19 "$PY" diag_train_ab.py > train_ab.log 2>&1 && touch done_train
echo "training finished $(date -u), marker=$([ -f done_train ] && echo yes || echo NO)"
