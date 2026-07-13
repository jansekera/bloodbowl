#!/usr/bin/env bash
# Cross-rebuild paired-seed A/B for the stall-guard blitz fix (a88f5e2).
# Baseline arm needs the PRE-fix binary, so this drives the rebuilds itself.
# Always leaves the working tree back on the committed (fixed) source.
set -euo pipefail
cd /home/jan/claude/bloodbowl

N="${1:-400}"
FIX_COMMIT=a88f5e2
SRC=engine/src/macro_actions.cpp

restore_fix() {
    echo "--- restoring fixed source ---"
    git checkout "$FIX_COMMIT" -- "$SRC"
    cmake --build engine/build -j"$(nproc)" >/dev/null
}
trap restore_fix EXIT

source venv/bin/activate

echo "=== ARM 1/2: baseline (pre-fix engine, ${FIX_COMMIT}^) ==="
git checkout "${FIX_COMMIT}^" -- "$SRC"
cmake --build engine/build -j"$(nproc)" >/dev/null
grep -q "carrierIsBlitzable(state, carrier)" "$SRC" && {
    echo "ABORT: source still has the fix -- checkout failed"; exit 1; }
python3 diag_stall_guard_blitz.py baseline "$N"

echo
echo "=== ARM 2/2: candidate (fixed engine, ${FIX_COMMIT}) ==="
git checkout "$FIX_COMMIT" -- "$SRC"
cmake --build engine/build -j"$(nproc)" >/dev/null
grep -q "carrierIsBlitzable(state, carrier)" "$SRC" || {
    echo "ABORT: fix missing from source -- checkout failed"; exit 1; }
python3 diag_stall_guard_blitz.py candidate "$N"

echo
echo "=== COMPARE ==="
python3 diag_stall_guard_blitz.py compare
