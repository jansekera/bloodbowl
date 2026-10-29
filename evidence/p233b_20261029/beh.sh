#!/bin/bash
# P233 v běžných hrách 29.10.: 9 dvojic (bez Block × rychlejší), 2 ramena, jen náš tah + odpověď.
# Zmrazená knihovna ./zmrazeno (commit 43e08d71, jádro vedle modulu, RUNPATH $ORIGIN, váhy, hlavičky).
# Napřed kouř se STEJNÝM kořenem a skriptem (N=1); spadne-li, ostrý běh se nespustí.
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$D/zmrazeno"; cd "$D"
date > beh.log
echo "== kouř N=1" >> beh.log
if ! JADER=7 nice -n 10 /usr/local/bin/python3.8 mereni.py "$KOREN" kour.pkl 1 >> beh.log 2>&1; then
  echo "KOUŘ SPADL" >> beh.log; echo HOTOVO >> beh.log; exit 1
fi
/usr/local/bin/python3.8 vyhodnot.py kour.pkl > kour_tabulka.txt 2>> beh.log
echo "== ostrý běh N=6" >> beh.log
JADER=7 nice -n 10 /usr/local/bin/python3.8 mereni.py "$KOREN" mereni.pkl 6 >> beh.log 2>&1
/usr/local/bin/python3.8 vyhodnot.py mereni.pkl > tabulka.txt 2>> beh.log
date >> beh.log; echo HOTOVO >> beh.log
