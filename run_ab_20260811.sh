#!/usr/bin/env bash
# ============================================================================
# A/B VŠECH DNEŠNÍCH ZMĚN — noc 11.→12.08.2026
#
# PRE  = 5b7d19f  (stav dnes ráno, tedy po balíku G)
# POST = 1f3f168  (dnešní HEAD)
#
# Měří BALÍK, ne jednotlivosti. Co je uvnitř:
#   1. rozestavení podle rolí (nejhlubší slot bere handlera, lajna ne)
#   2. náhradníci berou role, ne jen prázdný čtverec
#   3. jednokostkové bloky pro hráče s Block
#   4. brána přihrávek (jen když >= 50 % nebo nouze)
#   5. apothecary podle nahraditelnosti, ne podle jména pozice
#   6. pravidlové: kolo 9, míč po výkopu, faul = turnover, Secret Weapon
# NEPROJEVÍ SE (brána cageAdvance je vypnutá):
#   exposure klece · "jeď co můžeš" · cage-fill
#
# mode 2 => seedBase 51'000'000 napevno => TYTÉŽ SEEDY jako 10.08. i jako
# měření balíku G ⇒ čtvrtý bod na týchž hrách.
# Oba army načítají TYTÉŽ váhy z hlavního repa (argv[1]).
#
# ⚑ PRE-REGISTRACE
#   PRIMÁRNĚ: trpaslíci NESMÍ regredovat. Párová Δ chess trpaslíka na dw-sk
#   i dw-we; PROBLÉM při z <= -1,28 (jednostranně, týž práh jako rasová
#   pojistka). NEUTRÁLNÍ = ÚSPĚCH.
#   ⚠️ ŽÁDNÁ hypotéza o zlepšení se nepredikuje. Je to balík šesti změn,
#   z nichž většina je korektnostní; kladná Δ NENÍ důkaz, že některá pomohla.
#   KONTROLA: orc-sk odliší "pomohlo trpaslíkům" od "hnulo to všemi".
#   SEKUNDÁRNĚ popisně: TD obou stran, attrition, remízovost, bloky/tah.
#
# Pojistky (nálezy 10.-11.08.):
#   * obě ramena SAMOSTATNÉ WORKTREE s vlastní libbb_engine.so
#   * marker AB_DONE jen při 6/6 exit 0; jinak AB_PARTIAL
#   * idempotence: marker + lockfile + kontrola běžící instance
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
SC=/tmp/claude-1000/-home-jan-claude/e1df046f-ad55-465c-b0c8-2ab829c4b956/scratchpad
OUT=$ROOT/ab_measure_20260811
LOG=$OUT/chain.log
PAIRS=${PAIRS:-400}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"

[ -f "$OUT/AB_DONE" ] && { echo "[$(STAMP)] A/B už hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "diag_ab_d_(pre|post) $ROOT" > /dev/null; then
    echo "[$(STAMP)] ABORT: A/B už běží" >> "$LOG"; exit 1
fi
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1
fi

PRE_BIN=$SC/d_pre/diag_ab_d_pre
POST_BIN=$SC/d_post/diag_ab_d_post
for b in "$PRE_BIN" "$POST_BIN"; do
    [ -x "$b" ] || { echo "[$(STAMP)] ABORT: chybí $b" >> "$LOG"; exit 1; }
done

echo "[$(STAMP)] A/B start, PAIRS=$PAIRS, pre=5b7d19f post=1f3f168" >> "$LOG"

run_one() {  # $1=idx $2=jmeno $3=era $4=binarka
    local d="$OUT/$2_$3"
    mkdir -p "$d"
    rm -f "$d/diag_era_rows.jsonl" "$d/OK" "$d/FAIL"
    if ( cd "$d" && nice -n 19 "$4" "$ROOT" "$PAIRS" "$1" 2 > run.log 2>&1 ); then
        touch "$d/OK";  echo "[$(STAMP)] done $2 $3" >> "$LOG"
    else
        touch "$d/FAIL"; echo "[$(STAMP)] FAIL $2 $3 — viz $d/run.log" >> "$LOG"
    fi
}

for spec in "0 dw-sk" "1 dw-we" "3 orc-sk"; do
    set -- $spec
    run_one "$1" "$2" pre  "$PRE_BIN"  &
    run_one "$1" "$2" post "$POST_BIN" &
done
wait

FAILED=$(find "$OUT" -name FAIL | wc -l)
OKS=$(find "$OUT" -name OK | wc -l)
if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq 6 ]; then
    echo "[$(STAMP)] A/B DONE (6/6)" >> "$LOG"; touch "$OUT/AB_DONE"
else
    echo "[$(STAMP)] A/B PARTIAL: ok=$OKS fail=$FAILED — marker NEVZNIKÁ" >> "$LOG"
    touch "$OUT/AB_PARTIAL"
fi
