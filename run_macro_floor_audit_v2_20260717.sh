#!/bin/bash
cd /home/jan/claude/bloodbowl
OUT=scratch_macro_floor_audit
echo "[$(date '+%F %T')] v2: run screen (K=25, reduced for time budget)"
$OUT/diag_macro_floor_audit screen 25 < $OUT/snaps.txt > $OUT/screen_k25.out
echo "[$(date '+%F %T')] v2: screen done"
echo "[$(date '+%F %T')] v2: aggregate counts (reuse existing counts.out)"
./venv/bin/python3 diag_macro_floor_audit_20260716.py agg_counts $OUT/counts.out
echo "[$(date '+%F %T')] v2: aggregate screen"
./venv/bin/python3 diag_macro_floor_audit_20260716.py agg_screen $OUT/screen_k25.out
echo "[$(date '+%F %T')] v2 ALL DONE"
