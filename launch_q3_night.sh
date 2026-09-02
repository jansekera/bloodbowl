#!/bin/bash
# ============================================================================
# NOC 02.->03.09.2026  —  Q3 / mode 14  (oceneni tri vetvi vstavani)
# Predregistrace: evidence/night_prereg_20260902_q3.preds
#
# ⭐ TRI BRANY, KAZDA MUSI PROJIT, JINAK SE NOC NESPOUSTI:
#   (1) PREFLIGHT s --expect: sonda 1 paru musi VYTISKNOUT vsechna jmena
#       radku, ktera predregistrace slibuje jako rucni cteni.
#       ⛔ Bez teto brany se 01.09. i 02.09. stalo, ze noc dobehla a
#         registrovane cteni v ni NEBYLO (P35, M13).
#   ⚠️ --control-mode2: nulovy MATCHUP se u Q3 koupit NEDA -- rameno se
#       spousti pri kazdem vstavani vedle soupere, neni rasa, kde by neslo.
#       Rozpulit 2400 paru na expozici+nulu by stalo silu (prah +-0,015 je
#       v jedne noci jen 2,5 sigma). ⇒ Nulou je brana (2) nize, a ta je pod
#       CRN TAUTOLOGIE: chyti hrubou chybu v seedovani, ale VERDIKT NA NI
#       NESTOJI. Skutecnou ochranou je leak test v samotne noci
#       ("MOVED WITHOUT THE ARM ACTING: 0").
#   (2) KONTROLA SEEDOVANI (mode 2): pod CRN musi obe orientace hrat touz
#       hru => n_nonzero PRESNE 0. Je to tautologie a verdikt na ni nestoji,
#       ale hrubou chybu v seedovani chyti.
#   (3) az pak noc.
# ============================================================================
set -u
cd /home/jenda/claude/blood-bowl
EXPECT="Q3/UTEK,Q3/ODPOVED,Q3/VSTAVANI"

echo "=== (1) PREFLIGHT — tisknou se vsechna registrovana cteni? ==="
python3 colab_night_preflight.py --mode 14 --matchups "2:dw-dw:1" \
        --expect "$EXPECT" --pairs 2400 --workers 4 --control-mode2 \
        --prereg evidence/night_prereg_20260902_q3.preds || {
  echo "⛔ PREFLIGHT NEPROSEL — NOC SE NESPOUSTI"; exit 1; }

echo "=== (2) kontrola seedovani: mode 2, 8 paru, dw-dw ==="
./diag_f1_cage_advance . 8 2 2 > /tmp/claude-1000/q3_control.log 2>&1
NZ=$(grep -o 'n_nonzero [0-9]*' /tmp/claude-1000/q3_control.log | head -1 | awk '{print $2}')
grep -E 'SUMMARY' /tmp/claude-1000/q3_control.log
if [ "${NZ:-x}" != "0" ]; then
  echo "⛔ KONTROLA SEEDOVANI NEPROSLA (n_nonzero=${NZ:-?}) — NOC SE NESPOUSTI"; exit 1
fi
echo "✅ kontrola cista (n_nonzero=0)"

echo "=== (3) spoustim noc: 2400 paru, 48 kusu po 50, 4 workery ==="
OUT=$PWD/ab_q3_20260902 MODE=14 MATCHUPS="2:dw-dw:1" PAIRS=2400 \
CHUNKS=48 NULL_PAIRS=0 WORKERS=4 ./run_laptop_night.sh
