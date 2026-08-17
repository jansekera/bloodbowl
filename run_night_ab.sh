#!/usr/bin/env bash
# ============================================================================
# NOČNÍ A/B — OBECNÝ SPOUŠTĚČ                              (17.08.2026)
#
# Nahrazuje zvyk psát na každou noc nový skript s vlastní kopií zámku. Devět
# takových kopií = devět různých způsobů, jak přijít o noc. Tenhle sourcuje
# `run_night_lib.sh` a přidá dvě věci, které A/B tohoto projektu musí mít:
#
#   ⛔ MATCHUP S NULOVOU EXPOZICÍ JE POVINNÝ.
#      15.08. splnil pre-registrovaný práh „PROŠLO" i matchup, kde se rameno
#      ANI JEDNOU nespustilo (+2,28 pp, +2,3 SE při `cand_daunt = 0` v 6 000
#      hrách). Pár není tatáž hra s jedním přehozeným bitem — jsou to dvě hry
#      na spřízněných seedech. ⇒ Efekt se čte PROTI NULOVÉMU RAMENI, ne proti
#      nule, a bez nuly se běh NESPOUŠTÍ. Viz P20.
#
#   ⛔ PREFLIGHT PŘED 14 HODINAMI, NE PO NICH.
#      Nejhorší selhání není pád, ale noc odběhnutá na starém `libbb_engine.so`.
#
# POUŽITÍ
#   MODE=4 PAIRS=750 SHARDS=4 \
#   MATCHUPS="4:dw-orc:1 0:dw-sk:0 3:orc-sk:0" \
#   OUT=ab_wrestle_20260818 ./run_night_ab.sh
#
#   Formát matchupu je `index:jméno:expozice`, kde expozice 1 = tam se rameno
#   spustí (ta otázka), 0 = tam se spustit NEMŮŽE (null). Aspoň jedna nula.
#
#   Volitelně: CORPUS=1 sebere po A/B korpus 3000 her se zapnutým ramenem,
#   BASELINE=<adresář> ho porovná — a odmítne to, když baseline běžela na jiném
#   commitu enginu (P22: korpus 14.08. byl s baseline 6 commitů rozejitý).
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
. "$ROOT/run_night_lib.sh"

MODE=${MODE:?nastav MODE (režim harnessu, např. 4 = Dauntless v nabídce)}
MATCHUPS=${MATCHUPS:?nastav MATCHUPS "idx:jméno:expozice ..."}
PAIRS=${PAIRS:-750}
SHARDS=${SHARDS:-4}
OUT="$ROOT/${OUT:?nastav OUT (jméno výstupního adresáře)}"
BIN="$ROOT/diag_f1_cage_advance"
SRC="$ROOT/diag_f1_cage_advance_harness.cpp"
CORPUS=${CORPUS:-0}
BASELINE=${BASELINE:-}

night_init "$OUT" "ab-mode$MODE"
night_stamp_head "$OUT"

# --- povinná nula -----------------------------------------------------------
nulls=0; total=0
for spec in $MATCHUPS; do
    total=$((total+1))
    [ "${spec##*:}" = "0" ] && nulls=$((nulls+1))
done
if [ "$nulls" -eq 0 ]; then
    night_log "⛔ ODMÍTÁM SPUSTIT: žádný matchup s nulovou expozicí."
    night_log "   Bez nuly se efekt nedá odlišit od podlahy aparátu (P20)."
    night_log "   Přidej matchup, kde se rameno spustit NEMŮŽE, a pusť znovu."
    exit 2
fi
night_log "matchupů $total, z toho nulových $nulls — OK"

night_preflight "$BIN" "$OUT" "$SRC" || {
    night_log "⛔ preflight neprošel — nespouštím. Oprav a pusť znovu."
    exit 3
}

# --- A/B --------------------------------------------------------------------
if [ -f "$OUT/AB_DONE" ]; then
    night_log "A/B už hotové, přeskakuji"
else
    night_log "START A/B mode $MODE, ${SHARDS}×${PAIRS} párů na matchup"
    run_one() {  # $1=index  $2=jméno  $3=shard
        local off=$(( $3 * PAIRS )) d="$OUT/$2_s$3"
        mkdir -p "$d"; rm -f "$d/OK" "$d/FAIL"
        if ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$PAIRS" "$1" "$MODE" "$off" > run.log 2>&1 ); then
            touch "$d/OK";  night_log "done $2 shard $3"
        else
            touch "$d/FAIL"; night_log "FAIL $2 shard $3 — viz $d/run.log"
        fi
    }
    for spec in $MATCHUPS; do
        idx=${spec%%:*}; rest=${spec#*:}; name=${rest%%:*}
        for s in $(seq 0 $((SHARDS - 1))); do night_run_bg run_one "$idx" "$name" "$s"; done
    done
    night_wait
    EXPECT=$(( total * SHARDS ))
    OKS=$(find "$OUT" -name OK | wc -l); FAILED=$(find "$OUT" -name FAIL | wc -l)
    if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq "$EXPECT" ]; then
        night_log "A/B DONE ($OKS/$EXPECT)"; touch "$OUT/AB_DONE"
    else
        night_log "A/B PARTIAL: ok=$OKS fail=$FAILED"; touch "$OUT/AB_PARTIAL"; exit 1
    fi
fi

# --- „spustilo se to rameno vůbec?" -----------------------------------------
# Nejcennější řádek pondělního čtení. Harness ho od 17.08. tiskne sám; tady se
# jen vytáhne nahoru, aby se na něj nemuselo hledat.
night_log "--- ARM (nula = ta ramena běžela na stejném kódu) ---"
for spec in $MATCHUPS; do
    rest=${spec#*:}; name=${rest%%:*}
    # Pozor na vzor: "ARM " sedí i na "both arms" v řádku o kleci. Kotvit.
    line=$(grep -h "^  ARM " "$OUT/${name}_s"*/run.log 2>/dev/null | head -1 | sed 's/^  //')
    night_log "  $name: ${line:-(harness nic netiskl — stará binárka?)}"
done

# --- korpus (volitelně) -----------------------------------------------------
if [ "$CORPUS" != "0" ]; then
    CORPUS_DATA="$ROOT/${OUT##*/}_corpus_data"
    if [ -f "$CORPUS_DATA/COLLECT_DONE" ]; then
        night_log "korpus už existuje, přeskakuji"
    else
        night_log "START korpus 3000 her se zapnutým ramenem"
        if CAGE_GATE=0 DAUNTLESS=1 DATA_ROOT="$CORPUS_DATA" SEED_BASE=20260900 \
                nice -n 19 python3 diag_replay_mine_20260813_gate.py collect 3000 \
                > "$OUT/collect.log" 2>&1; then
            night_log "korpus HOTOV"
        else
            night_log "FAIL korpus — viz $OUT/collect.log"; exit 1
        fi
    fi
    nice -n 19 python3 diag_drive_failure_20260811.py "$CORPUS_DATA" > "$OUT/drives.txt" 2>&1
    nice -n 19 python3 diag_rules_checks_20260812.py "$CORPUS_DATA/*.json.gz" > "$OUT/checks.txt" 2>&1
    if [ -n "$BASELINE" ]; then
        if night_check_baseline "$OUT" "$ROOT/$BASELINE"; then
            night_log "korpus SE SMÍ srovnat s $BASELINE"
        else
            night_log "⛔ korpus se s $BASELINE srovnat NESMÍ — rozdíl NENÍ efekt ramene"
        fi
    fi
fi

night_log "NIGHT DONE"
touch "$OUT/NIGHT_DONE"
