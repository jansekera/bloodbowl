#!/bin/bash
# P233 cílené měření 28.10.: týmy bez Block (nemrtví, Nurgle, Chaos) × rychlejší, 3 ramena, jen náš tah + odpověď.
# Zmrazená knihovna ./zmrazeno_p233 (modul + jádro vedle sebe, RUNPATH $ORIGIN, váhy). Napřed kouř se STEJNÝM
# kořenem a skriptem (N=1); spadne-li, ostrý běh se nespustí.
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$D/zmrazeno_p233"; cd "$D"
date > beh.log
echo "== kouř N=1" >> beh.log
if ! JADER=7 nice -n 10 /usr/local/bin/python3.8 cilene_p233.py "$KOREN" kour.pkl 1 >> beh.log 2>&1; then
  echo "KOUŘ SPADL" >> beh.log; echo HOTOVO >> beh.log; exit 1
fi
/usr/local/bin/python3.8 vyhodnot.py kour.pkl > kour_tabulka.txt 2>> beh.log
echo "== ostrý běh N=10" >> beh.log
JADER=7 nice -n 10 /usr/local/bin/python3.8 cilene_p233.py "$KOREN" cilene.pkl 10 >> beh.log 2>&1
/usr/local/bin/python3.8 vyhodnot.py cilene.pkl > tabulka.txt 2>> beh.log
date >> beh.log; echo HOTOVO >> beh.log
