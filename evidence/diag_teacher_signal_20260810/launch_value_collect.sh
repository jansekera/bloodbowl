#!/bin/bash
# Test 1 (teacher value): idempotent detached launcher.
# 3 shards, same protocol/seeds as the A/B corpus, MCTS-100, current engine.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal
ROOT=/home/jan/claude/bloodbowl
cd "$DIR" || exit 1

launch_job() {
    local tag=$1 rh=$2 ra=$3 n=$4 seed=$5
    if [ -f "done_val_${tag}" ]; then
        echo "[skip] val_${tag}: done marker exists"
        return
    fi
    if pgrep -f "diag_teacher_value_collect .* vrows_${tag}.jsonl" > /dev/null; then
        echo "[skip] val_${tag}: already running"
        return
    fi
    rm -f "vrows_${tag}.jsonl"
    setsid nohup nice -n 19 bash -c \
        "./diag_teacher_value_collect $ROOT $rh $ra $n $seed vrows_${tag}.jsonl && touch done_val_${tag}" \
        > "val_${tag}.log" 2>&1 < /dev/null &
    disown
    echo "[launched] val_${tag} ($rh vs $ra, $n games, seeds $seed+) pid=$!"
}

launch_job dwsk_a dwarf skaven 16 92000000
launch_job dwsk_b dwarf skaven 16 92000100
launch_job wesk wood-elf skaven 16 92000200
