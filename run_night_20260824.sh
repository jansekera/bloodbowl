#!/bin/bash
# ============================================================================
# NOC 24.→25.08.2026 — REPLIKACE P38 (volba cílového pole nosiče)
#
# Pořadí kroků NENÍ kosmetika, je to oprava toho, na čem umřela fáze B víkendu:
#   (1) build enginu   -> (2) TESTY -> (3) PŘEKLAD HARNESSU -> (4) run_night_ab
# Harness se linkuje proti libbb_engine.so až za běhu (rpath), takže musí být
# přeložený AŽ PO finálním buildu enginu. Když se to obrátí, binárka sahá na
# staré offsety struct Player a spadne na SEGFAULT hned na první hře -- přesně
# to se stalo 22.08. (viz commit 32f93c8f).
# ⭐ Kdyby na krok (3) někdo zapomněl, `night_preflight` to od 24.08. CHYTNE
#   a odmítne spustit; tenhle skript to jen dělá tak, aby k tomu nedošlo.
#
# LIGHT=1  -> krátká zkouška celého řetězu (jiný OUT, malé N). NEMĚŘÍ, jen
#             dokazuje, že řetěz dojede od sondy k zápisu dat a k souhrnu.
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT" || exit 1

LIGHT=${LIGHT:-0}
if [ "$LIGHT" = "1" ]; then
    OUT_DIR=carrier_square_lighttest_20260824
    PAIRS=20; SHARDS=2; CHUNKS=4; CONTROL_PAIRS=10
    LOG=$ROOT/lighttest_20260824.log
else
    OUT_DIR=carrier_square_replic_20260824
    PAIRS=850; SHARDS=8; CHUNKS=40; CONTROL_PAIRS=50
    LOG=$ROOT/night_20260824.log
fi

say() { echo "[$(date '+%m-%d %H:%M')] $*" | tee -a "$LOG"; }

say "=== NOC 24.08. START (LIGHT=$LIGHT, OUT=$OUT_DIR) ==="
say "engine HEAD $(git rev-parse --short HEAD)"

# --- (1) build enginu -------------------------------------------------------
say "(1) build enginu"
cmake --build engine/build -j"$(nproc)" >> "$LOG" 2>&1 || { say "⛔ BUILD ENGINE FAIL"; exit 1; }

# --- (2) testy: nespouštět noc na červené sadě ------------------------------
say "(2) testy"
if ! ( cd engine/build && ./bb_tests ) >> "$LOG" 2>&1; then
    say "⛔ TESTY NEPROŠLY — nespouštím noc. Noc na červené sadě měří neznámo."
    exit 1
fi
say "    testy OK"

# --- (3) PŘEKLAD HARNESSU -- AŽ TEĎ, proti hotovému .so ---------------------
say "(3) překlad harnessu proti aktuálnímu .so"
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_cage_advance_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,"$ROOT/engine/build" \
    -o diag_f1_cage_advance >> "$LOG" 2>&1 \
    || { say "⛔ BUILD HARNESS FAIL"; exit 1; }
say "    harness OK ($(date -r diag_f1_cage_advance '+%H:%M'))"

# --- (4) samotný běh; preflight a sonda jsou uvnitř run_night_ab.sh ---------
say "(4) A/B: mode 6, ${SHARDS} × ${PAIRS} párů, dw-we, práh ±0,015"
MODE=6 PAIRS="$PAIRS" SHARDS="$SHARDS" THRESHOLD=0.015 \
CHUNKS="$CHUNKS" WORKERS=8 CONTROL_MODE2=1 CONTROL_PAIRS="$CONTROL_PAIRS" CORPUS=0 \
MATCHUPS="1:dw-we:1" \
PREREG="$ROOT/evidence/night_prereg_20260824.preds" \
OUT="$OUT_DIR" \
    ./run_night_ab.sh >> "$LOG" 2>&1
rc=$?
say "A/B skončilo (rc=$rc)"
say "=== KONEC ==="
exit $rc
