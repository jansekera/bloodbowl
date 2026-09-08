#!/bin/bash
# ============================================================================
# NOC 04.->05.09.2026  —  Q3-O / mode 17  (N+O dohromady proti rameni vypnutemu)
# Predregistrace: evidence/night_prereg_20260904_q3o.preds
#
# ⭐ TRI BRANY, KAZDA MUSI PROJIT, JINAK SE NOC NESPOUSTI (viz launch_q3n_night.sh):
#   (1) PREFLIGHT s --expect
#   (2) KONTROLA SEEDOVANI (mode 2, n_nonzero musi byt presne 0)
#   (3) az pak noc
# ============================================================================
set -u
cd /home/jenda/claude/blood-bowl
# ⭐ Q3/ODEBRANO-ZUSTAT dnes NESMI byt 0 -- jinak rameno O nikdy nezabralo.
EXPECT="Q3/UTEK,Q3/ODPOVED,Q3/VSTAVANI,Q3/CENA,Q3/CENA-PRICINA,Q3/ZED,Q3/PRILIS-RIZIKOVE,Q3/ODEBRANO-ZUSTAT"

echo "=== (1) PREFLIGHT — tisknou se vsechna registrovana cteni? ==="
python3 colab_night_preflight.py --mode 17 --matchups "2:dw-dw:1" \
        --expect "$EXPECT" --pairs 2400 --workers 4 --control-mode2 \
        --prereg evidence/night_prereg_20260904_q3o.preds \
        --session-hours 16 || {
  echo "⛔ PREFLIGHT NEPROSEL — NOC SE NESPOUSTI"; exit 1; }

echo "=== (2) kontrola seedovani: mode 2, 8 paru, dw-dw ==="
./diag_f1_cage_advance . 8 2 2 > /tmp/claude-1000/q3o_control.log 2>&1
NZ=$(grep -o 'n_nonzero [0-9]*' /tmp/claude-1000/q3o_control.log | head -1 | awk '{print $2}')
grep -E 'SUMMARY' /tmp/claude-1000/q3o_control.log
if [ "${NZ:-x}" != "0" ]; then
  echo "⛔ KONTROLA SEEDOVANI NEPROSLA (n_nonzero=${NZ:-?}) — NOC SE NESPOUSTI"; exit 1
fi
echo "✅ kontrola cista (n_nonzero=0)"

echo "=== (3) spoustim noc: 2400 paru, 48 kusu po 50, 4 workery ==="
OUT=$PWD/ab_q3o_20260904 MODE=17 MATCHUPS="2:dw-dw:1" PAIRS=2400 \
CHUNKS=48 NULL_PAIRS=0 WORKERS=4 ./run_laptop_night.sh
