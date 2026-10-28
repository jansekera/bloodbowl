#!/bin/bash
# Cílené P230 + záloha + P232 (28.10., znovu po pádu 27.10. — chyběly váhy ve zmrazené kopii).
# Zmrazená knihovna zmrazeno_p232/ (commit 407527c6, modul s RUNPATH $ORIGIN — jádro vedle sebe).
# Napřed kouř se STEJNÝM kořenem a skriptem (N=1); spadne-li, ostrý běh se nespustí.
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$D/zmrazeno_p232"; cd "$D"
date > beh_cilene.log
echo "== kouř N=1" >> beh_cilene.log
if ! JADER=6 nice -n 10 /usr/local/bin/python3.8 cilene.py "$KOREN" cilene_kour.pkl 1 >> beh_cilene.log 2>&1; then
  echo "KOUŘ SPADL" >> beh_cilene.log; echo HOTOVO >> beh_cilene.log; exit 1
fi
/usr/local/bin/python3.8 cilene_vyhodnot.py cilene_kour.pkl > cilene_kour_tabulka.txt 2>> beh_cilene.log
echo "== ostrý běh N=15" >> beh_cilene.log
JADER=7 nice -n 10 /usr/local/bin/python3.8 cilene.py "$KOREN" cilene.pkl 15 >> beh_cilene.log 2>&1
/usr/local/bin/python3.8 cilene_vyhodnot.py cilene.pkl > cilene_tabulka.txt 2>> beh_cilene.log
date >> beh_cilene.log; echo HOTOVO >> beh_cilene.log
