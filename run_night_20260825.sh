#!/bin/bash
# ============================================================================
# NOC 25.→26.08.2026 — B1/P35 (VÝBĚR BLITZUJÍCÍHO PODLE POLE, KAM DOJDE)
#
# Pořadí kroků je TOTÉŽ jako 24.08. a NENÍ kosmetika:
#   (1) build enginu -> (2) TESTY -> (3) PŘEKLAD HARNESSU -> (4) run_night_ab
# Harness se linkuje proti libbb_engine.so za běhu (rpath) ⇒ musí se překládat
# AŽ PO finálním buildu enginu, jinak sahá na staré offsety struct Player.
# To zabilo fázi B víkendu (SEGFAULT, commit 32f93c8f).
#
# ⛔ ENGINE JE OD SPUŠTĚNÍ ZMRAZENÝ. Dnešní práce na okruhu POHYB (M1/N10, Leap)
#   musí být zacommitovaná a přeložená PŘED startem; po startu se `.so` nesmí
#   přestavět, dokud běh neskončí.
#
# ⭐ MODE 8 JE NOVÝ (25.08.). Rameno `setBlitzLandingArm` je hotové od 19.08.,
#   ale harness pro něj neměl mode, takže se P35 šest dní neměřila.
#
# LIGHT=1 -> krátká zkouška celého řetězu (jiný OUT, malé N, bez korpusu).
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
cd "$ROOT" || exit 1

LIGHT=${LIGHT:-0}
if [ "$LIGHT" = "1" ]; then
    OUT_DIR=blitzlanding_lighttest_20260825
    # korpus i v light testu, jen 12 her: 25.08. do něj přibyla KONTROLA
    # INVARIANTŮ a nezkoušená větev v noci je totéž co žádná větev.
    # ⚠️ 25.08.: první pokus mel PAIRS=3/SHARDS=2/CHUNKS=4 => 6 parů na 4 kusy,
    # což hlídač správně odmítl (rc=7): zbytek by tiše ZMIZEL a změnil N.
    # Platí i pro light test: CHUNKS musí DĚLIT celkový počet párů a být >= WORKERS.
    PAIRS=2; SHARDS=2; CHUNKS=2; NWORKERS=2; CONTROL_PAIRS=5; CORPUS_ON=1; CORPUS_N=12
    LOG=$ROOT/lighttest_20260825.log
else
    OUT_DIR=blitzlanding_replic_20260825
    # 4 800 párů = 40 chunků × 120. Kalibrace: mode 8 dal v light testu
    # 46,8 s/pár na JEDNOM procesu (8 párů / 6 m 14 s, prázdný stroj); při 8
    # naráz čekej 6-9 s/pár wall, tedy 8-12 h. Loňská noc (mode 6) jela 9,13 s/pár.
    # 4 800 párů / 40 kusů = 120 na kus, 40 >= WORKERS 8 ⇒ hlídač dělitelnosti projde.
    PAIRS=600; SHARDS=8; CHUNKS=40; NWORKERS=8; CONTROL_PAIRS=50; CORPUS_ON=1; CORPUS_N=3000
    LOG=$ROOT/night_20260825.log
fi

say() { echo "[$(date '+%m-%d %H:%M')] $*" | tee -a "$LOG"; }

say "=== NOC 25.08. START (LIGHT=$LIGHT, OUT=$OUT_DIR) ==="
say "engine HEAD $(git rev-parse --short HEAD)"

say "(1) build enginu"
cmake --build engine/build -j"$(nproc)" >> "$LOG" 2>&1 || { say "⛔ BUILD ENGINE FAIL"; exit 1; }

say "(2) testy"
if ! ( cd engine/build && ./bb_tests ) >> "$LOG" 2>&1; then
    say "⛔ TESTY NEPROŠLY — nespouštím noc. Noc na červené sadě měří neznámo."
    exit 1
fi
say "    testy OK"

say "(3) překlad harnessu proti aktuálnímu .so"
g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
    diag_f1_cage_advance_harness.cpp \
    -Lengine/build -lbb_engine -Wl,-rpath,"$ROOT/engine/build" \
    -o diag_f1_cage_advance >> "$LOG" 2>&1 \
    || { say "⛔ BUILD HARNESS FAIL"; exit 1; }
say "    harness OK ($(date -r diag_f1_cage_advance '+%H:%M'))"

say "(4) A/B: mode 8, ${SHARDS} × ${PAIRS} párů, dw-we, práh ±0,015, CORPUS=$CORPUS_ON"
MODE=8 PAIRS="$PAIRS" SHARDS="$SHARDS" THRESHOLD=0.015 \
CHUNKS="$CHUNKS" WORKERS="$NWORKERS" CONTROL_MODE2=1 CONTROL_PAIRS="$CONTROL_PAIRS" \
CORPUS="$CORPUS_ON" CORPUS_GAMES="$CORPUS_N" \
MATCHUPS="1:dw-we:1" \
PREREG="$ROOT/evidence/night_prereg_20260825.preds" \
OUT="$OUT_DIR" \
    ./run_night_ab.sh >> "$LOG" 2>&1
rc=$?
say "A/B skončilo (rc=$rc)"
say "=== KONEC ==="
exit $rc
