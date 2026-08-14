#!/usr/bin/env bash
# ============================================================================
# DOHÁNĚNÍ DLUHU — noc 12.→13.08.2026
#
# Uživatel 12.08.: „raději bych byl po kontrolu každé zvlášť — obecné pravidlo".
# Proto NE balík, ale ŘETĚZ: každé rameno přidá právě JEDEN commit, takže
# rozdíl sousedních ramen je čistý příspěvek jedné změny.
#
#   A = 1f3f168  základ (= včerejší post; běží znovu jako KONTROLA REPRODUKCE)
#   B = fea042c  + chain push (rekurze, Side Step/Grab prázdné pole)
#   C = 0ec69f3  + Stand Firm v řetězu + follow-up na neopuštěné pole
#   D = 9f98070  + Dauntless před asistencemi
#
#   B−A = chain push · C−B = Stand Firm · D−C = Dauntless
#
# mode 2 => seedBase 51'000'000 napevno => TYTÉŽ SEEDY jako 10., 11. i 12.08.
#
# ⚑ PRE-REGISTRACE — OBRÁCENÁ PROTI VČEREJŠKU
#   Všechny tři opravy braly VÝHODU, kterou jsme neměli mít.
#   ⇒ PŘEDPOVÍDÁM POKLES trpaslíka. Neutrál by byl podezřelý.
#   * dw-sk  — hne hlavně Dauntless (Troll Slayeři; spouštělo se to při každé
#              soupeřově asistenci a nemohlo to selhat)
#   * dw-we  — všechny tři: Dauntless, Side Step (Wardancer), Stand Firm (Treeman)
#   * orc-sk — ⭐ MĚLA BY SE HNOUT MINIMÁLNĚ: ani ork, ani skaven nemá v TV1200
#              Dauntless, Side Step ani Stand Firm. Výrazný pohyb = něco jiného
#              je špatně, a to je informace navíc, kterou včerejší A/B neměl.
#   * A vs. včerejší post — musí sedět NA ČÍSLO. Když nesedí, není deterministický
#              harness a všechna párová srovnání jsou podezřelá.
#
# Pojistky: samostatné worktree s vlastní .so · marker jen při 12/12 exit 0 ·
#           marker + lockfile + kontrola běžící instance (idempotence)
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
SC=/tmp/claude-1000/-home-jan-claude/411f9e30-63ed-4aa0-9f3b-c6e8565a72f1/scratchpad
OUT=$ROOT/debt_measure_20260812
LOG=$OUT/chain.log
PAIRS=${PAIRS:-400}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/DEBT_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "bin_[ABCD] $ROOT" > /dev/null; then
    echo "[$(STAMP)] ABORT: měření už běží" >> "$LOG"; exit 1; fi
if ps -eo args | grep -v grep | grep -qE "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží run_iteration" >> "$LOG"; exit 1; fi

for a in A B C D; do
    [ -x "$SC/bin_$a" ] || { echo "[$(STAMP)] ABORT: chybí $SC/bin_$a" >> "$LOG"; exit 1; }
done

echo "[$(STAMP)] START, PAIRS=$PAIRS, ramena A=1f3f168 B=fea042c C=0ec69f3 D=9f98070" >> "$LOG"

run_one() {  # $1=idx $2=jméno $3=rameno
    local d="$OUT/$2_$3"
    mkdir -p "$d"; rm -f "$d/diag_era_rows.jsonl" "$d/OK" "$d/FAIL"
    if ( cd "$d" && nice -n 19 "$SC/bin_$3" "$ROOT" "$PAIRS" "$1" 2 > run.log 2>&1 ); then
        touch "$d/OK";  echo "[$(STAMP)] done $2 $3" >> "$LOG"
    else
        touch "$d/FAIL"; echo "[$(STAMP)] FAIL $2 $3 — viz $d/run.log" >> "$LOG"
    fi
}

for spec in "0 dw-sk" "1 dw-we" "3 orc-sk"; do
    set -- $spec
    for arm in A B C D; do run_one "$1" "$2" "$arm" & done
done
wait

OKS=$(find "$OUT" -name OK | wc -l); FAILED=$(find "$OUT" -name FAIL | wc -l)
if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq 12 ]; then
    echo "[$(STAMP)] DONE (12/12)" >> "$LOG"; touch "$OUT/DEBT_DONE"
else
    echo "[$(STAMP)] PARTIAL: ok=$OKS fail=$FAILED — marker NEVZNIKÁ" >> "$LOG"
    touch "$OUT/DEBT_PARTIAL"
fi
