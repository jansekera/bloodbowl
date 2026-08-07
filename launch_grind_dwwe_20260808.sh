#!/bin/bash
# Launcher sobotní validace dw-we (novy roster): čeká na (1) marker
# GRIND400_DONE pátečního běhu, (2) sobotu >= 17:00 UTC, (3) volno od
# run_iteration i harnessu. Session-nezávislý (setsid, PPID 1) — uživatel
# i Claude budou o víkendu odpojeni.
LOG=/home/jan/claude/bloodbowl/tempo_measure_20260808_launch.log
echo "launcher armed $(date -u '+%F %H:%M') pid $$" >> "$LOG"
until [ -f /home/jan/claude/bloodbowl/tempo_measure_20260807/GRIND400_DONE ]; do
    sleep 600
done
# Sobota (den v týdnu 6) od 17:00 UTC; neděle+ (7) spouští hned jako dohánění.
while true; do
    DOW=$(date -u +%u); HH=$(date -u +%H)
    if [ "$DOW" -ge 7 ]; then break; fi
    if [ "$DOW" -eq 6 ] && [ "$HH" -ge 17 ]; then break; fi
    sleep 600
done
while ps -eo args | grep -v grep | grep -qE "diag_f1_cage_advance |python3 run_iteration"; do
    sleep 600
done
echo "launching $(date -u '+%F %H:%M')" >> "$LOG"
exec bash /home/jan/claude/bloodbowl/run_grind_dwwe_20260808.sh >> "$LOG" 2>&1
