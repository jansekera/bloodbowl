#!/bin/bash
# Pipeline: paired-seed C++ A/B for the throw-in direction-template fix.
set -euo pipefail
cd /home/jan/claude/bloodbowl

FIX_FILES="engine/include/bb/ball_handler.h engine/src/ball_handler.cpp engine/src/pass_handler.cpp engine/tests/test_ball_handler.cpp"
N=300

echo "=== $(date -u '+%F %T') stash fix, build PRE-FIX binary ==="
git stash push -m throwindir-fix-ab -- $FIX_FILES
trap 'echo "!!! ERROR: attempting stash pop to restore fix"; git stash pop || true' ERR
cmake --build engine/build -j"$(nproc)" > /dev/null
echo "=== $(date -u '+%F %T') baseline arm (pre-fix) N=$N ==="
./venv/bin/python diag_throwin_direction_fix_ab_20260716.py baseline "$N"
echo "=== $(date -u '+%F %T') pop fix, build POST-FIX binary ==="
trap - ERR
git stash pop
cmake --build engine/build -j"$(nproc)" > /dev/null
echo "=== $(date -u '+%F %T') candidate arm (post-fix) N=$N ==="
./venv/bin/python diag_throwin_direction_fix_ab_20260716.py candidate "$N"
echo "=== $(date -u '+%F %T') report ==="
./venv/bin/python diag_throwin_direction_fix_ab_20260716.py report
echo "=== $(date -u '+%F %T') PIPELINE DONE ==="
