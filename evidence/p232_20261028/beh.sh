#!/bin/bash
# P232 měření 28.10.: 3 ramena, jen náš tah + odpověď soupeře, 8 dvojic × 6 poločasů útoku.
# Zmrazená knihovna ../p230_20261027/zmrazeno_p232 (commit 407527c6, jádro vedle modulu, RUNPATH $ORIGIN).
# Navazuje na cílené měření P230 (čeká na HOTOVO). Napřed kouř se STEJNÝM kořenem (1 dvojice × 1 hra přes N=1 nejde
# oddělit — kouř = celé N=1, 8 her); spadne-li, ostrý běh se nespustí.
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$D/../p230_20261027/zmrazeno_p232"; cd "$D"
until grep -q HOTOVO ../p230_20261027/beh_cilene.log 2>/dev/null; do sleep 60; done
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
