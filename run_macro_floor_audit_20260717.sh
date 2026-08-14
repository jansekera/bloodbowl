#!/bin/bash
set -e
cd /home/jan/claude/bloodbowl
echo "[$(date '+%F %T')] build start"
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_macro_floor_audit_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,"$PWD/engine/build" \
    -o scratch_macro_floor_audit/diag_macro_floor_audit
echo "[$(date '+%F %T')] build done"

echo "[$(date '+%F %T')] dump corpus"
./venv/bin/python3 diag_macro_floor_audit_20260716.py dump main_postfix \
    > scratch_macro_floor_audit/snaps.txt
wc -l scratch_macro_floor_audit/snaps.txt

echo "[$(date '+%F %T')] run counts"
scratch_macro_floor_audit/diag_macro_floor_audit counts \
    < scratch_macro_floor_audit/snaps.txt > scratch_macro_floor_audit/counts.out
echo "[$(date '+%F %T')] run screen (K=150)"
scratch_macro_floor_audit/diag_macro_floor_audit screen 150 \
    < scratch_macro_floor_audit/snaps.txt > scratch_macro_floor_audit/screen.out

echo "[$(date '+%F %T')] aggregate counts"
./venv/bin/python3 diag_macro_floor_audit_20260716.py agg_counts \
    scratch_macro_floor_audit/counts.out
echo "[$(date '+%F %T')] aggregate screen"
./venv/bin/python3 diag_macro_floor_audit_20260716.py agg_screen \
    scratch_macro_floor_audit/screen.out
echo "[$(date '+%F %T')] ALL DONE"
