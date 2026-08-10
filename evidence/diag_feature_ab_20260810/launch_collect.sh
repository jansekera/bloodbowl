#!/bin/bash
# Idempotent detached launcher for the offline feature A/B collection
# (evidence/fable_offline_feature_ab_20260810.md). Pre-registered corpus:
# 32x dwarf-skaven + 16x wood-elf-skaven, fairtest config, seeds 92M+.
# Idempotency: per-job done-marker + running-instance check (pgrep).
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/feature_ab
ROOT=/home/jan/claude/bloodbowl
cd "$DIR" || exit 1

launch_job() {
    local tag=$1 rh=$2 ra=$3 n=$4 seed=$5
    if [ -f "done_${tag}" ]; then
        echo "[skip] ${tag}: done marker exists"
        return
    fi
    if pgrep -f "diag_feature_ab_collect .* rows_${tag}.jsonl" > /dev/null; then
        echo "[skip] ${tag}: already running"
        return
    fi
    # fresh start: truncate partial rows from a previous crashed run so the
    # jsonl never contains duplicate seeds
    rm -f "rows_${tag}.jsonl"
    setsid nohup nice -n 19 bash -c \
        "./diag_feature_ab_collect $ROOT $rh $ra $n $seed rows_${tag}.jsonl && touch done_${tag}" \
        > "collect_${tag}.log" 2>&1 < /dev/null &
    disown
    echo "[launched] ${tag} ($rh vs $ra, $n games, seeds $seed+) pid=$!"
}

launch_job dwsk_a dwarf skaven 16 92000000
launch_job dwsk_b dwarf skaven 16 92000100
launch_job wesk wood-elf skaven 16 92000200
