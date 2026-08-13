#!/usr/bin/env bash
# ============================================================================
# NOČNÍ ŘETĚZ 13.→14.08.2026 — jedna věc za noc
#
# 1) korpus se ZAPNUTOU bránou klece (120 her, tytéž seedy jako 11.08.)
# 2) kontroly K29–K36 na obou korpusech, výstup vedle sebe
#
# Krok 2 běží jen když krok 1 doběhl celý; jinak by porovnával neúplný vzorek.
#
# ⚑ PROČ TOHLE A NE DALŠÍ A/B
#   A/B brány (`run_gate_20260813.sh`) se ptá „hraje se s bránou lépe?".
#   Tenhle běh se ptá „co brána dělá?" a je potřeba **bez ohledu na to, jak
#   A/B dopadlo** — zvlášť když dopadlo neprůkazně. Odemyká K9b, `paceAch`
#   a hranici S2/S3/S4, tedy tři měřicí díry s jednou příčinou.
#
# ⚑ PÁROVOST: korpusy sdílejí seedy, soupeře i orientace a liší se jedinou
#   věcí — zapnutou bránou. Rozdíl v kontrolách je tedy dopad brány na
#   chování, ne rozdíl mezi dvěma nezávislými vzorky.
#
# Pojistky: marker + lockfile + kontrola běžící instance.
#   `pgrep -f` se hledaným řetězcem v UVOZOVKÁCH matchne sám sebe (vlastní
#   cmdline ten řetězec obsahuje) a launcher se odmítne spustit na prázdném
#   stroji -- proto `[d]iag`, což je táž past jako u pkill.
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/night_20260813
LOG=$OUT/chain.log
NEW=$ROOT/diag_replay_mine_20260813_gate_data
OLD=$ROOT/diag_replay_mine_20260811b_data
GAMES=${GAMES:-120}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/NIGHT_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "[d]iag_f1_cage_advance" > /dev/null; then
    echo "[$(STAMP)] ABORT: běží ještě měření brány" >> "$LOG"; exit 1; fi
if pgrep -f "[d]iag_replay_mine_20260813_gate" > /dev/null; then
    echo "[$(STAMP)] ABORT: sběr korpusu už běží" >> "$LOG"; exit 1; fi

cd "$ROOT" || exit 1

# ---- 1) korpus se zapnutou bránou -----------------------------------------
if [ -f "$NEW/COLLECT_DONE" ]; then
    echo "[$(STAMP)] korpus už existuje, přeskakuji sběr" >> "$LOG"
else
    echo "[$(STAMP)] START sběr korpusu s bránou ON, $GAMES her," \
         "HEAD=$(git rev-parse --short HEAD)" >> "$LOG"
    if nice -n 19 python3 diag_replay_mine_20260813_gate.py collect "$GAMES" \
            > "$OUT/collect.log" 2>&1; then
        echo "[$(STAMP)] korpus HOTOV" >> "$LOG"
    else
        echo "[$(STAMP)] FAIL sběr korpusu — viz $OUT/collect.log" >> "$LOG"
        touch "$OUT/NIGHT_PARTIAL"; exit 1
    fi
fi

[ -f "$NEW/COLLECT_DONE" ] || {
    echo "[$(STAMP)] ABORT: korpus nemá COLLECT_DONE, kontroly neběží" >> "$LOG"
    touch "$OUT/NIGHT_PARTIAL"; exit 1; }

# ---- 2) kontroly na obou korpusech ----------------------------------------
echo "[$(STAMP)] START kontroly K29–K36 (brána OFF)" >> "$LOG"
nice -n 19 python3 diag_rules_checks_20260812.py "$OLD/*.json.gz" \
    > "$OUT/checks_gate_off.txt" 2>&1
echo "[$(STAMP)] START kontroly K29–K36 (brána ON)" >> "$LOG"
nice -n 19 python3 diag_rules_checks_20260812.py "$NEW/*.json.gz" \
    > "$OUT/checks_gate_on.txt" 2>&1

if grep -q "K33" "$OUT/checks_gate_off.txt" && grep -q "K33" "$OUT/checks_gate_on.txt"; then
    echo "[$(STAMP)] DONE — porovnej $OUT/checks_gate_{off,on}.txt" >> "$LOG"
    touch "$OUT/NIGHT_DONE"
else
    echo "[$(STAMP)] PARTIAL: kontroly nedoběhly celé" >> "$LOG"
    touch "$OUT/NIGHT_PARTIAL"
fi
