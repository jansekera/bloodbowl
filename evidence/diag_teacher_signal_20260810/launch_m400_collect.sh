#!/bin/bash
# Test 2 (MCTS-400 corpus): idempotent detached launcher.
# SEQUENTIAL single process (CPU is busy: era runs now, M1 overnight) —
# 3 shards run one after another inside one nice-19 wrapper, ~2-3 h total.
# Seeds 93M+ (disjoint from all previous runs). Logs read tomorrow.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal
ROOT=/home/jan/claude/bloodbowl
cd "$DIR" || exit 1

if [ -f done_m400_all ]; then
    echo "[skip] m400: done marker exists"
    exit 0
fi
if pgrep -f "m400_sequence_runner" > /dev/null; then
    echo "[skip] m400: already running"
    exit 0
fi

cat > m400_sequence_runner.sh <<'EOF'
#!/bin/bash
# m400_sequence_runner
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal
ROOT=/home/jan/claude/bloodbowl
cd "$DIR" || exit 1
run_shard() {
    local tag=$1 rh=$2 ra=$3 n=$4 seed=$5
    if [ -f "done_m400_${tag}" ]; then return; fi
    rm -f "rows_m400_${tag}.jsonl"
    ./diag_feature_ab_collect_m400 "$ROOT" "$rh" "$ra" "$n" "$seed" "rows_m400_${tag}.jsonl" \
        && touch "done_m400_${tag}"
}
run_shard dwsk_a dwarf skaven 16 93000000
run_shard dwsk_b dwarf skaven 16 93000100
run_shard wesk wood-elf skaven 16 93000200
if [ -f done_m400_dwsk_a ] && [ -f done_m400_dwsk_b ] && [ -f done_m400_wesk ]; then
    touch done_m400_all
fi
EOF
chmod +x m400_sequence_runner.sh
setsid nohup nice -n 19 ./m400_sequence_runner.sh > m400_collect.log 2>&1 < /dev/null &
disown
echo "[launched] m400 sequential runner pid=$!"
