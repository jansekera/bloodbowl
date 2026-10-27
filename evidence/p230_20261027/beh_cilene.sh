#!/bin/bash
# Cílené P230 + záloha 27.10.: navazuje na beh_kalibrace.sh (čeká na HOTOVO), zmrazená knihovna p231 (zmrazeno_p231/).
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$D/zmrazeno_p231"; cd "$D"
until grep -q HOTOVO beh_kalibrace.log 2>/dev/null; do sleep 60; done
date > beh_cilene.log
JADER=7 nice -n 10 /usr/local/bin/python3.8 cilene.py "$KOREN" cilene.pkl 15 >> beh_cilene.log 2>&1
/usr/local/bin/python3.8 cilene_vyhodnot.py cilene.pkl > cilene_tabulka.txt 2>> beh_cilene.log
date >> beh_cilene.log; echo HOTOVO >> beh_cilene.log
