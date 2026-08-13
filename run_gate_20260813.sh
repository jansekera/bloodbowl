#!/usr/bin/env bash
# ============================================================================
# T3.1 — BRÁNA KLECE (cageAdvance) ON vs OFF, 13.08.2026
#
# Za bránou leží hotová nezměřená práce: cage advance (c085331 = uživatelovo
# R1), cage-fill fallback (cd2d98c, 1445bd3) a „jeď co můžeš" (dda0cdd).
# Brána je v produkci vypnutá, takže se nic z toho nikdy neprojevilo — v
# korpusu je `plán: NOT_CONSULTED` ve 100 % kol.
#
# ⚑ PROČ 1500 PÁRŮ A NE 400
#   Null-testy z 12.08. (mode 2, obě ramena stejná konfigurace) daly šumové
#   dno ±5,3 pp na 400 párech. Efekt řádu 3 pp by na 400 párech NEŠLO odlišit
#   od nuly. Při 1500 párech je SE ≈ 0,0134, takže +3 pp vyjde na ~2,2 SE.
#   Spouštět tohle na 400 párech by byl předem zbytečný běh.
#
# ⚑ PRE-REGISTRACE
#   Plán dělá 5,00 pole na kolo proti 1,73 u search(). Advance sám odmítá
#   v 85 % kol (TEMPO_INSUFFICIENT 48 %, DICEY 37 %), ale od 11.08. má
#   fallback FILL_ONLY, takže „odmítne" už neznamená „nic se nestane".
#   * dw-sk, dw-we — PŘEDPOVÍDÁM ZLEPŠENÍ trpaslíka. Tempo je měřený
#     schodek (K9a −3,19 pole/kolo) a tohle je jediný nástroj, který na něj
#     míří přímo.
#   * orc-sk — KONTROLA. Ork bránu má taky, takže nula se tu čekat nedá;
#     čeká se MENŠÍ efekt než u trpaslíka. Kdyby ork získal víc, není to
#     trpasličí nástroj a doktrína to má vysvětlit.
#   * `cage plans adopted` musí být > 0. Ve smoke běhu 2,65/hru.
#
# Rozdělení: 3 matchupy × 4 shardy × 375 párů = 1500 párů/matchup, 12 procesů.
# Shardy jsou disjunktní úseky seedů (argv[5]), ověřeno bit-identicky.
#
# Pojistky: marker + lockfile + kontrola běžící instance (idempotence).
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/gate_measure_20260813
LOG=$OUT/chain.log
BIN=$ROOT/diag_f1_cage_advance
PAIRS=${PAIRS:-375}
SHARDS=${SHARDS:-4}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/GATE_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
# -x na jméno procesu, ne -f na cmdline: `pgrep -f "diag_f1_cage_advance $ROOT"`
# matchne sám sebe (vlastní cmdline ten řetězec obsahuje) a launcher se odmítne
# spustit na prázdném stroji. Táž past jako u pkill.
if pgrep -x diag_f1_cage_advance > /dev/null; then
    echo "[$(STAMP)] ABORT: měření už běží" >> "$LOG"; exit 1; fi
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1; fi
[ -x "$BIN" ] || { echo "[$(STAMP)] ABORT: chybí $BIN" >> "$LOG"; exit 1; }

echo "[$(STAMP)] START mode 0 (cage ON vs OFF), $SHARDS×$PAIRS párů na matchup," \
     "HEAD=$(cd $ROOT && git rev-parse --short HEAD)" >> "$LOG"

run_one() {  # $1=matchup idx  $2=jméno  $3=shard
    local off=$(( $3 * PAIRS ))
    local d="$OUT/$2_s$3"
    mkdir -p "$d"; rm -f "$d/diag_f1_cage_advance_rows.jsonl" "$d/OK" "$d/FAIL"
    if ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$PAIRS" "$1" 0 "$off" > run.log 2>&1 ); then
        touch "$d/OK";  echo "[$(STAMP)] done $2 shard $3" >> "$LOG"
    else
        touch "$d/FAIL"; echo "[$(STAMP)] FAIL $2 shard $3 — viz $d/run.log" >> "$LOG"
    fi
}

for spec in "0 dw-sk" "1 dw-we" "3 orc-sk"; do
    set -- $spec
    for s in $(seq 0 $((SHARDS - 1))); do run_one "$1" "$2" "$s" & done
done
wait

EXPECT=$(( 3 * SHARDS ))
OKS=$(find "$OUT" -name OK | wc -l); FAILED=$(find "$OUT" -name FAIL | wc -l)
if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq "$EXPECT" ]; then
    echo "[$(STAMP)] DONE ($OKS/$EXPECT)" >> "$LOG"; touch "$OUT/GATE_DONE"
else
    echo "[$(STAMP)] PARTIAL: ok=$OKS fail=$FAILED — marker NEVZNIKÁ" >> "$LOG"
    touch "$OUT/GATE_PARTIAL"
fi
