#!/bin/bash
# Potvrzovací A/B grindu #2 — NOVÝ ROSTER (noc 07.→08.08., plán schválen
# uživatelem 07.08. ráno; výsledky se čtou v pondělí 10.08.).
# Repliku 400 párů dw-sk (matchup 0, mode 1: kandidát cageAdvance+cageGrind
# vs baseline cageAdvance bez grindu) ze 06.08. spouští nad novou érou:
#   - dwarf roster f7aa61c (rohy Guard+Tackle, ball-hunter pryč, Guard 2×LB)
#   - orc roster 2e5b7b7 (Black Orc +Block) — dw-sk běh nezasahuje
#   - fix nálezu 1 6531ba0 (ADVANCE TZ pull-back) — v OBOU ramenech
#   - item13 krok 2 575d4f1 — gated OFF, běhu se netýká
# Stejné seedy 37M+idx => párové srovnání staré vs nové éry per seed možné.
#
# PRE-REGISTROVANÁ KRITÉRIA (shodná s 06.08., fable_tempo_doctrine_report):
#   PRIMÁRNÍ:   párová Δchess >= +5,6 pp (2 SE) => POTVRZENO (GO default ON
#               dw-sk větve grindu — finální slovo uživatel; před defaultem
#               fix nálezu 2 + stat-agnostické rohy, pořadí z reportu).
#   SEKUNDÁRNÍ (2b): dwarf TD/hru nárůst >= +0,10; soupeřovo TD pokles;
#               výskyt výher 2:0/2:1 v grind rameni.
#   Δchess < +5,6 pp, ale sekundární splněna => INCONCLUSIVE, diskuze.
#   Δchess <= 0 => NEPOTVRZENO.
# Referenční baseline staré éry: +4,37 pp ± 2,77 (INCONCLUSIVE, addendum
# v evidence/fable_tempo_doctrine_report_20260806.md).
# Vyhodnocení (pondělí): python3 tempo_measure_20260807/analyze_grind_ab.py
# (kopie skriptu z 06.08.; čte ab_m*/diag_f1_grind_rows.jsonl).
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT"
OUT=$ROOT/tempo_measure_20260807/ab_m400
mkdir -p "$OUT"
LOG=$ROOT/tempo_measure_20260807/chain.log
STAMP() { date -u '+%H:%M'; }

if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] GRIND400v2 ABORT: běží run_iteration" >> "$LOG"; exit 1
fi

# Rebuild harness proti aktuální engine knihovně (nová éra f7aa61c..575d4f1).
echo "[$(STAMP)] GRIND400v2 rebuild" >> "$LOG"
cmake --build engine/build -j"$(nproc)" >> "$LOG" 2>&1 \
    || { echo "[$(STAMP)] BUILD FAIL engine" >> "$LOG"; exit 1; }
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_cage_advance_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,$ROOT/engine/build \
    -o diag_f1_cage_advance \
    >> "$LOG" 2>&1 || { echo "[$(STAMP)] BUILD FAIL harness" >> "$LOG"; exit 1; }
cp tempo_measure_20260806/analyze_grind_ab.py tempo_measure_20260807/ 2>>"$LOG"

echo "[$(STAMP)] GRIND400v2 start (400 párů dw-sk, mode 1, novy roster)" >> "$LOG"
( cd "$OUT" && nice -n 19 "$ROOT/diag_f1_cage_advance" "$ROOT" 400 0 1 \
      > ab_grind_m400.log 2>&1 )
echo "[$(STAMP)] GRIND400v2 DONE" >> "$LOG"
touch "$ROOT/tempo_measure_20260807/GRIND400_DONE"
