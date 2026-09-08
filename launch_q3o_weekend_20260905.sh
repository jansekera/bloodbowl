#!/bin/bash
# ============================================================================
# VIKENDOVY BEH 05.->07.09.2026  —  Q3-O / mode 17, VETSI VZOREK (7200 paru)
# Predregistrace: evidence/night_prereg_20260905_q3o_big.preds
#
# Duvod: Q3-N (2400) i Q3-O (2400) skoncily NEROZHODNUTO; ~6199 paru bylo
# potreba na rozhodnuti pri nezmenenem odhadu. 7200 = 3,0x, s rezervou
# (feedback_size_nights_with_margin_not_just_enough).
#
# ⭐ TRI BRANY, KAZDA MUSI PROJIT, JINAK SE BEH NESPOUSTI (viz launch_q3o_night.sh):
#   (1) PREFLIGHT s --expect
#   (2) KONTROLA SEEDOVANI (mode 2, n_nonzero musi byt presne 0)
#   (3) az pak beh
# ============================================================================
set -u
cd /home/jenda/claude/blood-bowl
EXPECT="Q3/UTEK,Q3/ODPOVED,Q3/VSTAVANI,Q3/CENA,Q3/CENA-PRICINA,Q3/ZED,Q3/PRILIS-RIZIKOVE,Q3/ODEBRANO-ZUSTAT"

echo "=== (1) PREFLIGHT — tisknou se vsechna registrovana cteni? ==="
python3 colab_night_preflight.py --mode 17 --matchups "2:dw-dw:1" \
        --expect "$EXPECT" --pairs 7200 --workers 4 --control-mode2 \
        --prereg evidence/night_prereg_20260905_q3o_big.preds \
        --session-hours 60 || {
  echo "⛔ PREFLIGHT NEPROSEL — BEH SE NESPOUSTI"; exit 1; }

echo "=== (2) kontrola seedovani: mode 2, 8 paru, dw-dw ==="
./diag_f1_cage_advance . 8 2 2 > /tmp/claude-1000/q3o_weekend_control.log 2>&1
NZ=$(grep -o 'n_nonzero [0-9]*' /tmp/claude-1000/q3o_weekend_control.log | head -1 | awk '{print $2}')
grep -E 'SUMMARY' /tmp/claude-1000/q3o_weekend_control.log
if [ "${NZ:-x}" != "0" ]; then
  echo "⛔ KONTROLA SEEDOVANI NEPROSLA (n_nonzero=${NZ:-?}) — BEH SE NESPOUSTI"; exit 1
fi
echo "✅ kontrola cista (n_nonzero=0)"

echo "=== (3) spoustim beh: 7200 paru, 144 kusu po 50, 4 workery ==="
OUT=$PWD/ab_q3o_weekend_20260905 MODE=17 MATCHUPS="2:dw-dw:1" PAIRS=7200 \
CHUNKS=144 NULL_PAIRS=0 WORKERS=4 ./run_laptop_night.sh
