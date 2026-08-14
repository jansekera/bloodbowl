#!/bin/bash
# Potvrzovací A/B grindu (GO uživatele 06.08. „grind" — noc patří ověření
# místo tréninku #3, dle pravidla ověřovací řetěz > trénink pod zámkem).
# 400 párů dw-sk (matchup 0), mode 1 (grind A/B: kandidát cageAdvance+cageGrind
# vs baseline cageAdvance bez grindu). Seedy deterministické dle indexu páru
# (37M+idx) → prvních 40 párů = replikace first-readu, zbytek nový.
#
# PRE-REGISTROVANÁ KRITÉRIA (z reportu fable_tempo_doctrine_report_20260806.md,
# first read +13,75 pp / 1,6 SE; SE@400 ~±2,8 pp):
#   PRIMÁRNÍ:   párová Δchess >= +5,6 pp (2 SE) => POTVRZENO (GO default ON
#               dw-sk větve grindu — finální slovo uživatel).
#   SEKUNDÁRNÍ (kritéria 2b): dwarf TD/hru nárůst >= +0,10; soupeřovo TD
#               pokles; výskyt výher 2:0/2:1 v grind rameni.
#   Δchess < +5,6 pp, ale sekundární splněna => INCONCLUSIVE, diskuze.
#   Δchess <= 0 => NEPOTVRZENO.
# Vyhodnocení: python3 tempo_measure_20260806/analyze_grind_ab.py nad
# ab400_m0/diag_f1_grind_rows.jsonl (ráno 07.08.).
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT"
OUT=$ROOT/tempo_measure_20260806/ab400_m0
mkdir -p "$OUT"
LOG=$ROOT/tempo_measure_20260806/chain.log
STAMP() { date -u '+%H:%M'; }

if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] GRIND400 ABORT: běží run_iteration" >> "$LOG"; exit 1
fi
echo "[$(STAMP)] GRIND400 start (400 párů dw-sk, mode 1)" >> "$LOG"
( cd "$OUT" && nice -n 19 "$ROOT/diag_f1_cage_advance" "$ROOT" 400 0 1 \
      > ab_grind400_m0.log 2>&1 )
echo "[$(STAMP)] GRIND400 DONE" >> "$LOG"
touch "$ROOT/tempo_measure_20260806/GRIND400_DONE"
