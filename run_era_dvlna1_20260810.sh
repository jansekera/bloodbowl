#!/usr/bin/env bash
# ============================================================================
# D-vlna 1 (rules parity) — SROVNÁNÍ ÉR, noc 10.→11.08.2026
#
# PROČ SROVNÁNÍ ÉR A NE A/B: dnešní změny mění pravidla pro OBĚ strany
# (dodge +1, mřížka dosahů, zachycení, throw-in po surfu…). Nedá se z toho
# udělat "jedno rameno s opravou, druhé bez" — to by nebyla hra podle žádných
# pravidel. Proto: TYTÉŽ SEEDY protečou dvěma buildy a porovnají se per seed.
#
#   PRE  = 99067fc (stav před 10.08.), worktree + vlastní build
#   POST = HEAD po D-vlně 1
#   Oba načítají TYTÉŽ váhy z hlavního repa (argv[1]) — jistota, že se liší
#   jen pravidla.
#
# PRE-REGISTRACE (POJISTKA, NE EXPERIMENT):
#   PRIMÁRNĚ: trpaslíci NESMÍ regredovat. Párová Δ chess skóre trpaslíka
#             (POST − PRE) na dw-sk i dw-we; PROBLÉM při z <= -1,28
#             (jednostranně, týž práh jako rasová pojistka v bráně).
#   NEUTRÁLNÍ VÝSLEDEK = ÚSPĚCH. Žádná hypotéza o zlepšení se nepredikuje —
#   směry se míchají (leap a přihrávky zlevňují hru rychlým rasám, dodge je
#   obousměrný, Take Root a Stand Firm ve vlně 1 NEJSOU).
#   KONTROLA: orc-sk (bez trpaslíků, a ověřeně bez TTM/Bombardiera) —
#             odliší "pomohlo trpaslíkům" od "zrychlilo hru všem".
#   SEKUNDÁRNĚ jen popisně: TD obou stran, attrition, remízovost.
#
# Výstup: era_measure_20260810/<matchup>_<era>/diag_era_rows.jsonl
# Marker: era_measure_20260810/ERA_DONE
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
SC=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad
OUT=$ROOT/era_measure_20260810
LOG=$OUT/chain.log
PAIRS=${PAIRS:-400}
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
echo "[$(STAMP)] ERA start, PAIRS=$PAIRS" >> "$LOG"

# Nespouštět přes trénink ani jiné měření.
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1
fi

PRE_BIN=$SC/era_pre/diag_era_pre
POST_BIN=$ROOT/diag_f1_cage_advance
for b in "$PRE_BIN" "$POST_BIN"; do
    [ -x "$b" ] || { echo "[$(STAMP)] ABORT: chybí $b" >> "$LOG"; exit 1; }
done

# matchup index -> jméno (0 dw-sk, 1 dw-we, 3 orc-sk)
run_one() {  # $1=idx $2=name $3=era $4=binary
    local d="$OUT/$2_$3"
    mkdir -p "$d"
    rm -f "$d/diag_era_rows.jsonl"
    ( cd "$d" && nice -n 19 "$4" "$ROOT" "$PAIRS" "$1" 2 > run.log 2>&1 )
    echo "[$(STAMP)] done $2 $3" >> "$LOG"
}

for spec in "0 dw-sk" "1 dw-we" "3 orc-sk"; do
    set -- $spec
    run_one "$1" "$2" pre  "$PRE_BIN"  &
    run_one "$1" "$2" post "$POST_BIN" &
done
wait

echo "[$(STAMP)] ERA DONE" >> "$LOG"
touch "$OUT/ERA_DONE"
