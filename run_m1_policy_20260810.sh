#!/usr/bin/env bash
# ============================================================================
# M1 — škodí NAUČENÁ policy trpaslíkovi, nebo jen víc pomáhá soupeři?
# Noc 10.→11.08.2026. Zadání: Fable report §M1
# (evidence/fable_learning_mechanism_report_20260811.md).
#
# NÁVRH: A/B uvnitř jednoho binárky. Obě ramena mají policy NAČTENOU, takže
# ručně psané prior floory jsou aktivní v OBOU — liší se jen NAUČENÝ obsah
# (policyBlend 0,2 vs 0,0), a to POUZE na trpasličí straně. Tím se odděluje
# "pomáhá naučená policy" od "pomáhají floory".
#
# PRE-REGISTRACE (diagnostika, ne brána):
#   Primární popisná metrika: párová Δ chess trpaslíka (blend 0,2 − blend 0)
#   na dw-sk i dw-we, + TD trpaslíka/hru.
#   Otázka, kterou to rozsoudí: záporná Δ u OBOU matchupů => policy trpaslíka
#   kazí; záporná jen u dw-we => spíš víc pomáhá rychlému soupeři.
#   ⚠️ Fable sám v malém A/B (16+16 her) viděl OPAČNÉ znaménko a označil to
#   za pod šumovým dnem — proto tohle měření vzniká. Nepredikuje se výsledek.
#
# Výstup: m1_measure_20260810/<matchup>/diag_m1_rows.jsonl
# Marker: m1_measure_20260810/M1_DONE
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/m1_measure_20260810
LOG=$OUT/chain.log
PAIRS=${PAIRS:-400}
BIN=$ROOT/diag_m1
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"

# --- IDEMPOTENCE (nález 10.08.: páteční launcher tohle neměl a pustil běh
# --- dvakrát; viz project_bloodbowl_launcher_idempotence_20260810) ---
[ -f "$OUT/M1_DONE" ] && { echo "[$(STAMP)] M1 už hotovo, končím" >> "$LOG"; exit 0; }
if pgrep -f "diag_m1 $ROOT" > /dev/null; then
    echo "[$(STAMP)] ABORT: diag_m1 už běží" >> "$LOG"; exit 1
fi
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1
fi
[ -x "$BIN" ] || { echo "[$(STAMP)] ABORT: chybí $BIN" >> "$LOG"; exit 1; }

echo "[$(STAMP)] M1 start, PAIRS=$PAIRS" >> "$LOG"

run_one() {  # $1=idx $2=name
    local d="$OUT/$2"
    mkdir -p "$d"
    rm -f "$d/diag_m1_rows.jsonl"
    ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$PAIRS" "$1" 3 > run.log 2>&1 )
    echo "[$(STAMP)] done $2" >> "$LOG"
}

run_one 0 dw-sk &
run_one 1 dw-we &
wait

echo "[$(STAMP)] M1 DONE" >> "$LOG"
touch "$OUT/M1_DONE"
