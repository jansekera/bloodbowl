#!/bin/bash
# Víkendová validace #2 (GO uživatele 07.08.: „spusť přes víkend i druhou
# validaci"): grind A/B dwarf vs WOOD-ELF na novém rosteru — sobotní noc
# 08.→09.08., výsledky se čtou v pondělí 10.08.
# 400 párů, matchup 1 (dw-we), mode 1 (kandidát cageAdvance+cageGrind vs
# baseline cageAdvance bez grindu). Seedy deterministické dle indexu páru.
#
# KONTEXT A PRE-REGISTRACE:
#   Referenční first read 06.08. (starý roster, 40 párů): Δchess −0,8 SE —
#   grind SÁM na elfy nestačí (predikce reportu: potřebuje koridor+risk
#   budget). Tento běh měří POPRVÉ synergii doktríny rohů (Guard+Tackle
#   slayeři/blitzeři, f7aa61c) s grindem proti elfím markerům/wardancerům.
#   PRIMÁRNÍ:   párová Δchess >= +5,6 pp (2 SE; SE@400 ~±2,8 pp) => grind
#               s novými rohy na elfy FUNGUJE (vstup do GO diskuze).
#   SEKUNDÁRNÍ: dwarf TD/hru nárůst >= +0,10; soupeřovo TD pokles;
#               posun distribuce skóre od 0:2/0:1 k 1:1/1:2.
#   Δ ~ 0 (šum) => potvrzení predikce „bez koridoru/risk budgetu to nejde"
#               — taky cenný výsledek (prioritizuje koridor práce).
# Vyhodnocení (pondělí): python3 tempo_measure_20260808/analyze_grind_ab.py
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT"
OUT=$ROOT/tempo_measure_20260808/ab_m400we
mkdir -p "$OUT"
LOG=$ROOT/tempo_measure_20260808/chain.log
STAMP() { date -u '+%H:%M'; }

if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] GRINDWE400 ABORT: běží run_iteration" >> "$LOG"; exit 1
fi
# Binárka diag_f1_cage_advance je čerstvá z pátečního řetězu (rebuild tam);
# pro jistotu rebuild no-op zopakovat.
cmake --build engine/build -j"$(nproc)" >> "$LOG" 2>&1 \
    || { echo "[$(STAMP)] BUILD FAIL engine" >> "$LOG"; exit 1; }
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_cage_advance_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,$ROOT/engine/build \
    -o diag_f1_cage_advance \
    >> "$LOG" 2>&1 || { echo "[$(STAMP)] BUILD FAIL harness" >> "$LOG"; exit 1; }
cp tempo_measure_20260806/analyze_grind_ab.py tempo_measure_20260808/ 2>>"$LOG"

echo "[$(STAMP)] GRINDWE400 start (400 párů dw-we, mode 1, novy roster)" >> "$LOG"
( cd "$OUT" && nice -n 19 "$ROOT/diag_f1_cage_advance" "$ROOT" 400 1 1 \
      > ab_grind_m400we.log 2>&1 )
echo "[$(STAMP)] GRINDWE400 DONE" >> "$LOG"
touch "$ROOT/tempo_measure_20260808/GRINDWE400_DONE"
