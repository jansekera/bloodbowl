#!/bin/bash
# Queued launcher for diag_policy_blend_bringup_20260716.py (2026-07-16).
# Waits for the concurrent vf_blend Phase 0 retest (8 workers, CPU saturated)
# to exit before claiming its cores -- starting alongside it risks false
# per-game watchdog skips in BOTH experiments. Falls back to a reduced worker
# count if the retest is still alive unusually late (>23:00), so a wedged
# retest cannot starve this run entirely.
cd /home/jan/claude/bloodbowl || exit 1
LOG=diag_policy_blend_bringup_20260716.log
PAT="diag_vf_blend_phase0_retest_20260716"
WORKERS=8
echo "[queue] $(date '+%F %T') waiting for vf retest to finish" >> "$LOG"
while pgrep -u "$(whoami)" -f "$PAT" > /dev/null; do
    if [ "$(date +%H%M)" -ge 2300 ]; then
        WORKERS=4
        echo "[queue] $(date '+%F %T') vf retest still alive at 23:00 --" \
             "starting anyway with PB_WORKERS=4" >> "$LOG"
        break
    fi
    sleep 120
done
echo "[queue] $(date '+%F %T') launching with PB_WORKERS=$WORKERS" >> "$LOG"
PB_WORKERS=$WORKERS exec ./venv/bin/python3 \
    diag_policy_blend_bringup_20260716.py all >> "$LOG" 2>&1
