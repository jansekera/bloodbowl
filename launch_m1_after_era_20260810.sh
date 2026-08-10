#!/usr/bin/env bash
# Launcher M1: naváže na doběhnutí ERA běhu (D-vlna 1). Session-nezávislý
# (setsid, PPID 1) — přežije odpojení uživatele i Clauda.
#
# Pojistky (nález 10.08., project_bloodbowl_launcher_idempotence_20260810):
#   1. čeká na marker ERA_DONE
#   2. čeká, až doopravdy nic z ERA běhu neběží (marker sám nestačí)
#   3. sám je idempotentní — run skript se ukončí, pokud M1_DONE existuje
#      nebo pokud diag_m1 už běží
LOG=/home/jan/claude/bloodbowl/m1_measure_20260810_launch.log
ROOT=/home/jan/claude/bloodbowl
mkdir -p "$ROOT/m1_measure_20260810"
echo "launcher armed $(date -u '+%F %H:%M') pid $$" >> "$LOG"

until [ -f "$ROOT/era_measure_20260810/ERA_DONE" ]; do sleep 120; done
echo "ERA_DONE spatren $(date -u '+%F %H:%M')" >> "$LOG"

# Marker nestačí: poslední procesy mohou dobíhat.
while pgrep -f "diag_era_pre|diag_f1_cage_advance " > /dev/null; do sleep 60; done
while ps -eo args | grep -v grep | grep -q "python3 run_iteration"; do sleep 300; done

echo "launching M1 $(date -u '+%F %H:%M')" >> "$LOG"
exec bash "$ROOT/run_m1_policy_20260810.sh" >> "$LOG" 2>&1
