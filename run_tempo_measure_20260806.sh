#!/bin/bash
# Session-nezávislý měřicí řetěz pro tempo doktrínu (Fable #3, 06.08.).
# Důvod: Fable agent nepřežije break session — data se sbírají tímto řetězem
# (setsid, PPID 1) a agent/Claude je pak jen analyzuje ze souborů.
# Kroky: počkat na konec tréninku → rebuild diag binárek → sonda TEMPO
# (baseline + grind shadow) → párové A/B grind (first-read N=40 párů/matchup,
# paralelně z oddělených CWD kvůli sdílenému rows souboru) → markery.
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT"
OUT=$ROOT/tempo_measure_20260806
mkdir -p "$OUT"
LOG=$OUT/chain.log
STAMP() { date -u '+%H:%M'; }

echo "[$(STAMP)] řetěz startuje, čekám na konec run_iteration" >> "$LOG"
while ps -eo args | grep -v grep | grep -q "python3 run_iteration"; do sleep 120; done
sleep 60
echo "[$(STAMP)] trénink doběhl → stavím binárky" >> "$LOG"

g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_adoption_probe.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,$ROOT/engine/build -o probe \
    >> "$LOG" 2>&1 || { echo "[$(STAMP)] BUILD FAIL probe" >> "$LOG"; exit 1; }
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_cage_advance_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,$ROOT/engine/build \
    -o diag_f1_cage_advance \
    >> "$LOG" 2>&1 || { echo "[$(STAMP)] BUILD FAIL harness" >> "$LOG"; exit 1; }

# --- 1) Sonda: baseline (grind=0) na 3 matchupech + grind SHADOW (grind=1,
#        observation-only) na dwarf matchupech; max 2 souběžně (pokyn v hlavičce).
echo "[$(STAMP)] PROBE fáze" >> "$LOG"
nice -n 19 ./probe . 8 0 0 > "$OUT/probe_m0_g0.log" 2>&1 &
nice -n 19 ./probe . 8 1 0 > "$OUT/probe_m1_g0.log" 2>&1 &
wait
nice -n 19 ./probe . 8 3 0 > "$OUT/probe_m3_g0.log" 2>&1 &
nice -n 19 ./probe . 8 0 1 > "$OUT/probe_m0_g1.log" 2>&1 &
wait
nice -n 19 ./probe . 8 1 1 > "$OUT/probe_m1_g1.log" 2>&1 &
wait
echo "[$(STAMP)] PROBE DONE" >> "$LOG"
touch "$OUT/PROBE_DONE"

# --- 2) Grind A/B first-read: 40 párů × matchupy 0 (dw-sk) a 1 (dw-we),
#        paralelně z oddělených CWD (rows soubor je append-shared).
echo "[$(STAMP)] A/B fáze (40 párů/matchup)" >> "$LOG"
mkdir -p "$OUT/ab_m0" "$OUT/ab_m1"
( cd "$OUT/ab_m0" && nice -n 19 "$ROOT/diag_f1_cage_advance" "$ROOT" 40 0 1 \
      > ab_grind_m0.log 2>&1 ) &
( cd "$OUT/ab_m1" && nice -n 19 "$ROOT/diag_f1_cage_advance" "$ROOT" 40 1 1 \
      > ab_grind_m1.log 2>&1 ) &
wait
echo "[$(STAMP)] ALL MEASUREMENTS DONE" >> "$LOG"
touch "$OUT/ALL_DONE"
