#!/usr/bin/env bash
# ============================================================================
# BALÍK G (persistence attrition) — SROVNÁNÍ ÉR, 11.08.2026
#
# PRE  = 336038c  (Thick Skull UVNITŘ, G VENKU)  <- izoluje G
# POST = 5b7d19f  (HEAD, celé G)
# Kdyby se vzalo 07836d1, změřilo by se "Thick Skull + G" dohromady.
#
# mode 2 => seedBase 51'000'000 napevno => TYTÉŽ SEEDY jako běh 10.08.
# ⇒ TROJBODOVÉ SROVNÁNÍ na týchž hrách: pre-D(99067fc) / post-D(07836d1) / post-G.
# Oba army načítají TYTÉŽ váhy z hlavního repa (argv[1]).
#
# ⚑ PRE-REGISTRACE: G NENÍ BRÁNA.
#   Není co promovat — G je korektnostní oprava a zůstává tak jako tak
#   (po každém TD se vraceli mrtví; nic v pravidlech tomu neodpovídá).
#   Měří se VELIKOST dopadu, ne jestli ji přijmout.
#   ⇒ ŽÁDNÝ PRÁH SE NESTANOVUJE, aby se pak nevydával za potvrzení.
#
#   Popisné metriky: přeživších z 11 (obě strany) · DEAD/hru (dosud 0,00 ve
#   3200 hrách, teď musí být nenulové) · casualty/hru a KO/hru per rasa ·
#   párová Δ chess trpaslíka (JEN POPISNĚ) · užití apothecary.
#
#   Jediné hlídané riziko: kdyby G trpaslíkům výrazně UŠKODILO, je to signál
#   ŠPATNÉ IMPLEMENTACE (mlátící tým na persistenci attrition prodělávat
#   nemá) ⇒ důvod k revizi kódu, NE k vypnutí G.
#
# ⚑ OPRAVY PROTI VČEREJŠKU (nálezy z noci 10.→11.08.):
#   1. Obě ramena jsou SAMOSTATNÉ WORKTREE s VLASTNÍ libbb_engine.so.
#      Včera diag_m1 slinkovaný proti měnitelné .so umřel na undefined
#      symbol, když se .so mezitím přestavěla. Tady na sebe rebuild v hlavním
#      repu nemůže dosáhnout.
#   2. Marker G_DONE vzniká JEN když všech 6 běhů skončí exit 0.
#      Včera run skript touchnul M1_DONE bez ohledu na návratový kód =>
#      selhaný běh se označil za hotový a idempotenční zámek pak bránil
#      opravnému běhu. Při selhání vzniká G_PARTIAL a rozepsaný FAIL řádek.
#   3. Idempotence: marker + lockfile + kontrola vlastní běžící instance.
#
# Výstup: g_measure_20260811/<matchup>_<era>/diag_era_rows.jsonl
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
SC=/tmp/claude-1000/-home-jan-claude/e1df046f-ad55-465c-b0c8-2ab829c4b956/scratchpad
OUT=$ROOT/g_measure_20260811
LOG=$OUT/chain.log
PAIRS=${PAIRS:-400}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"

# --- IDEMPOTENCE ---------------------------------------------------------
[ -f "$OUT/G_DONE" ] && { echo "[$(STAMP)] G už hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then
    echo "[$(STAMP)] ABORT: drží lock $LOCK" >> "$LOG"; exit 1
fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "diag_g_g_(pre|post) $ROOT" > /dev/null; then
    echo "[$(STAMP)] ABORT: G už běží" >> "$LOG"; exit 1
fi
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1
fi

PRE_BIN=$SC/g_pre/diag_g_g_pre
POST_BIN=$SC/g_post/diag_g_g_post
for b in "$PRE_BIN" "$POST_BIN"; do
    [ -x "$b" ] || { echo "[$(STAMP)] ABORT: chybí $b" >> "$LOG"; exit 1; }
done

echo "[$(STAMP)] G start, PAIRS=$PAIRS, pre=336038c post=5b7d19f" >> "$LOG"

# matchup index -> jméno (0 dw-sk, 1 dw-we, 3 orc-sk)
run_one() {  # $1=idx $2=name $3=era $4=binary
    local d="$OUT/$2_$3"
    mkdir -p "$d"
    rm -f "$d/diag_era_rows.jsonl" "$d/OK" "$d/FAIL"
    if ( cd "$d" && nice -n 19 "$4" "$ROOT" "$PAIRS" "$1" 2 > run.log 2>&1 ); then
        touch "$d/OK"
        echo "[$(STAMP)] done $2 $3" >> "$LOG"
    else
        touch "$d/FAIL"
        echo "[$(STAMP)] ⛔ FAIL $2 $3 (exit $?) — viz $d/run.log" >> "$LOG"
    fi
}

for spec in "0 dw-sk" "1 dw-we" "3 orc-sk"; do
    set -- $spec
    run_one "$1" "$2" pre  "$PRE_BIN"  &
    run_one "$1" "$2" post "$POST_BIN" &
done
wait

# --- marker JEN při plném úspěchu ---------------------------------------
FAILED=$(find "$OUT" -name FAIL | wc -l)
OKS=$(find "$OUT" -name OK | wc -l)
if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq 6 ]; then
    echo "[$(STAMP)] G DONE (6/6)" >> "$LOG"
    touch "$OUT/G_DONE"
else
    echo "[$(STAMP)] ⛔ G PARTIAL: ok=$OKS fail=$FAILED — marker NEVZNIKÁ, běh lze opakovat" >> "$LOG"
    touch "$OUT/G_PARTIAL"
fi
