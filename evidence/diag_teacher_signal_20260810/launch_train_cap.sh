#!/bin/bash
# Test 3 (cap-arm mini-A/B): idempotent detached launcher.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal
cd "$DIR" || exit 1
if [ -f done_train_cap ]; then
    echo "[skip] train_cap: done marker exists"
    exit 0
fi
if pgrep -f "diag_train_ab_cap.py" > /dev/null; then
    echo "[skip] train_cap: already running"
    exit 0
fi
setsid nohup nice -n 19 env OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 bash -c \
    "python3 diag_train_ab_cap.py && touch done_train_cap" \
    > train_cap.log 2>&1 < /dev/null &
disown
echo "[launched] train_cap pid=$!"
