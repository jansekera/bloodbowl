#!/bin/bash
# Pipeline: paired-seed C++ A/B for the hasActed double-activation fix.
# Stash fix -> build pre-fix -> baseline arm -> pop -> build post-fix ->
# candidate arm -> report. Detach with setsid+nohup; log is the artifact.
set -euo pipefail
cd /home/jan/claude/bloodbowl

FIX_FILES="engine/include/bb/game_state.h engine/src/action_resolver.cpp engine/src/game_state.cpp engine/tests/test_action_resolver.cpp"
N=300

echo "=== $(date -u '+%F %T') stash fix, build PRE-FIX binary ==="
git stash push -m hasacted-fix-ab -- $FIX_FILES
trap 'echo "!!! ERROR: attempting stash pop to restore fix"; git stash pop || true' ERR
cmake --build engine/build -j"$(nproc)" > /dev/null
echo "=== $(date -u '+%F %T') baseline arm (pre-fix) N=$N ==="
./venv/bin/python diag_hasacted_fix_ab_20260715.py baseline "$N"
echo "=== $(date -u '+%F %T') pop fix, build POST-FIX binary ==="
trap - ERR
git stash pop
cmake --build engine/build -j"$(nproc)" > /dev/null
echo "=== $(date -u '+%F %T') candidate arm (post-fix) N=$N ==="
./venv/bin/python diag_hasacted_fix_ab_20260715.py candidate "$N"
echo "=== $(date -u '+%F %T') report ==="
./venv/bin/python diag_hasacted_fix_ab_20260715.py report
echo "=== $(date -u '+%F %T') PIPELINE DONE ==="
