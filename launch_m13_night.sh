#!/bin/bash
# ============================================================================
# NOC 01.->02.09.2026  —  M13 / mode 13
# ⚠️ OUT je ab_m13b_ (s "b"): prvni pokus 13:12 byl po 10 min zastaven, aby se
#   uvolnil stroj na odpoledni praci. Zustaly po nem CTYRI adresare rozjetych
#   kusu BEZ znacky hotovo a na JINEM enginu -- spoustet do nej znovu by michalo
#   castecna data ze dvou buildu. Stary adresar se NEMAZE, je to zaznam.
# Predregistrace: evidence/night_prereg_20260901_m13.preds (+ dodatek)
#
# Poradi je zamerne: kontrola seedovani BEZI PRVNI a noc se pusti, JEN kdyz
# vrati presnou nulu. Pod CRN musi obe orientace hrat touz hru; cokoli jineho
# znamena hrubou chybu v seedovani a hlavni beh by se necetl.
# ============================================================================
set -u
cd /home/jenda/claude/blood-bowl

echo "=== (1) kontrola seedovani: mode 2, 8 paru, dw-dw ==="
./diag_f1_cage_advance . 8 2 2 > /tmp/claude-1000/m13_control.log 2>&1
NZ=$(grep -o 'n_nonzero [0-9]*' /tmp/claude-1000/m13_control.log | head -1 | awk '{print $2}')
grep -E 'SUMMARY' /tmp/claude-1000/m13_control.log
if [ "${NZ:-x}" != "0" ]; then
  echo "⛔ KONTROLA NEPROSLA (n_nonzero=${NZ:-?}) — NOC SE NESPOUSTI"
  exit 1
fi
echo "✅ kontrola cista (n_nonzero=0)"

echo "=== (2) spoustim noc: 2400 paru, 48 kusu po 50, 4 workery, BEZ limitu seance ==="
OUT=$PWD/ab_m13b_20260901 MODE=13 MATCHUPS="2:dw-dw:1" PAIRS=2400 \
CHUNKS=48 NULL_PAIRS=0 WORKERS=4 ./run_laptop_night.sh
