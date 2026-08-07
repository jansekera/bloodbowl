#!/bin/bash
# Launcher nočního grind potvrzení #2 (novy roster): čeká do 17:00 UTC
# + na konec item13 validačního harnessu, pak spustí session-nezávisle
# run_grind_confirm_20260807.sh. (Vzor launch_staged_night; PPID 1.)
LOG=/home/jan/claude/bloodbowl/tempo_measure_20260807_launch.log
echo "launcher armed $(date -u '+%F %H:%M') pid $$" >> "$LOG"
while [ "$(date -u +%H)" -lt 17 ]; do sleep 300; done
while ps -eo args | grep -v grep | grep -q "diag_item13_staged_planner "; do
    sleep 300
done
echo "launching $(date -u '+%F %H:%M')" >> "$LOG"
exec bash /home/jan/claude/bloodbowl/run_grind_confirm_20260807.sh >> "$LOG" 2>&1
