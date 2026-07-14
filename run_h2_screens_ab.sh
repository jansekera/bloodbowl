#!/usr/bin/env bash
# Cross-rebuild paired-seed chain A/B for the 2026-07-14 fixes:
#   pre(251d21b) -> h2(a79d164) -> repo2a(bd21d4a) -> lane2b(9baeb04)
# Each arm rebuilds the engine from that commit's sources; a marker grep
# verifies every checkout before games run. Always restores HEAD sources.
set -euo pipefail
cd /home/jan/claude/bloodbowl

N="${1:-400}"
SRCS="engine/src/game_simulator.cpp engine/src/macro_actions.cpp"

declare -A SHA=( [pre]=251d21b [h2]=a79d164 [repo2a]=bd21d4a [lane2b]=9baeb04 )
# marker: present-string|absent-string (empty side skipped)
declare -A CHECK=(
  [pre]="|openingKickingTeam"
  [h2]="openingKickingTeam|real movement budget"
  [repo2a]="real movement budget|Strategy 0.5"
  [lane2b]="Strategy 0.5|"
)

restore_head() {
    echo "--- restoring HEAD sources ---"
    git checkout HEAD -- $SRCS
    cmake --build engine/build -j"$(nproc)" >/dev/null
}
trap restore_head EXIT

source venv/bin/activate

i=0
for arm in pre h2 repo2a lane2b; do
    i=$((i+1))
    echo "=== ARM $i/4: $arm (${SHA[$arm]}) ==="
    git checkout "${SHA[$arm]}" -- $SRCS
    IFS='|' read -r want absent <<< "${CHECK[$arm]}"
    if [ -n "$want" ]; then
        grep -qr "$want" $SRCS || { echo "ABORT: marker '$want' missing in $arm"; exit 1; }
    fi
    if [ -n "$absent" ]; then
        grep -qr "$absent" $SRCS && { echo "ABORT: marker '$absent' present in $arm"; exit 1; }
    fi
    cmake --build engine/build -j"$(nproc)" >/dev/null
    python3 diag_h2_screens_chain.py "$arm" "$N"
    echo
done

echo "=== COMPARE ==="
python3 diag_h2_screens_chain.py compare
