#!/bin/bash
# Launcher validace dw-we (novy roster): naváže ROVNOU za doběhnutý páteční
# dw-sk běh (pokyn uživatele 07.08.: „nemusí čekat na sobotu") — čeká jen na
# (1) marker GRIND400_DONE, (2) volno od run_iteration i harnessu.
# Session-nezávislý (setsid, PPID 1) — uživatel i Claude budou odpojeni.
LOG=/home/jan/claude/bloodbowl/tempo_measure_20260808_launch.log
echo "launcher armed $(date -u '+%F %H:%M') pid $$" >> "$LOG"
until [ -f /home/jan/claude/bloodbowl/tempo_measure_20260807/GRIND400_DONE ]; do
    sleep 300
done
while ps -eo args | grep -v grep | grep -qE "diag_f1_cage_advance |python3 run_iteration"; do
    sleep 300
done
echo "launching $(date -u '+%F %H:%M')" >> "$LOG"
exec bash /home/jan/claude/bloodbowl/run_grind_dwwe_20260808.sh >> "$LOG" 2>&1
