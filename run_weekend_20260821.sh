#!/usr/bin/env bash
# ============================================================================
# VÍKENDOVÝ ŘETĚZ                                          (21.08.2026, pátek)
#
#   FÁZE A: křížový korpus, 15 dvojic × 1 200 her            (~28 h)
#   FÁZE B: párové A/B, replikace volby pole nosiče           (~26 h)
#   ------------------------------------------------------------------
#                                                     dohromady ~54 h z 60
#
# ⚑ PROČ V TOMHLE POŘADÍ
#   Korpus je PODMÍNKA, ne doplněk: po dnešním řezu (P45, vstávání) nemáme
#   platnou základní čáru, takže se proti ničemu nedá srovnávat. A/B ji
#   nepotřebuje (obě ramena běží na témž enginu), takže může jít až za ní.
#
# ⚑ FÁZE B JE REPLIKACE, NE NOVÁ OTÁZKA
#   Totéž rameno (mode 6), týž matchup, TÝŽ počet párů (8 × 850 = 6 800), týž
#   práh ±0,015 jako 19.08. Mění se JEN engine. 19.08.: +0,0827 ± 0,0065.
#   ⇒ Přežil ten nález opravu vstávání? Na tom visí rozhodnutí o nasazení.
#
# ⚑ POUŽITÍ
#   nohup setsid ./run_weekend_20260821.sh > /dev/null 2>&1 &
#   ⛔ NEEDITOVAT ZA BĚHU.
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

GAMES=${GAMES:-1200}
PAIRS=${PAIRS:-850}
SHARDS=${SHARDS:-8}
LOG="$ROOT/weekend_20260821.log"
say() { echo "[$(date -u '+%m-%d %H:%M')] $*" >> "$LOG"; }

say "=== VÍKENDOVÝ ŘETĚZ START (HEAD $(git rev-parse --short HEAD)) ==="

# --- FÁZE A ---------------------------------------------------------------
say "FÁZE A: křížový korpus, 15 dvojic × $GAMES her"
if GAMES="$GAMES" ./run_crosses_20260821.sh; then
    say "FÁZE A HOTOVA"
else
    say "⛔ FÁZE A SELHALA (rc=$?) — B se PŘESTO pustí, nezávisí na korpusu"
fi

# --- FÁZE B ---------------------------------------------------------------
# Řetězení je záměrně bezpodmínečné: A/B na korpusu nestojí, a kdyby fáze A
# spadla na disku nebo na jedné dvojici, bylo by zahození 26 hodin A/B čistá
# ztráta. Případný FAIL fáze A je v logu.
say "FÁZE B: A/B mode 6, $SHARDS × $PAIRS párů, dw-we, práh ±0,015"
MODE=6 PAIRS="$PAIRS" SHARDS="$SHARDS" THRESHOLD=0.015 \
CHUNKS=40 WORKERS=8 CONTROL_MODE2=1 CORPUS=0 \
MATCHUPS="1:dw-we:1" \
PREREG="$ROOT/evidence/night_prereg_20260821.preds" \
OUT=carrier_square_replic_20260821 \
    ./run_night_ab.sh
say "FÁZE B skončila (rc=$?)"
say "=== VÍKENDOVÝ ŘETĚZ KONEC ==="
