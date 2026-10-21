#!/bin/bash
# P220 měření: maska 128 zapnuto × vypnuto, jen náš tah + odpověď soupeře (72 poločasů jako diagnostika P220).
# Spouštět: setsid nohup bash beh.sh > /dev/null 2>&1 &
D="$(cd "$(dirname "$0")" && pwd)"; KOREN="$(cd "$D/../.." && pwd)"; cd "$D"; date > beh.log
JADER=4 nice -n 10 /usr/local/bin/python3.8 mereni.py "$KOREN" mereni.pkl 6 >> beh.log 2>&1
/usr/local/bin/python3.8 vyhodnot.py mereni.pkl > souhrn_tabulka.txt 2>> beh.log
date >> beh.log; echo HOTOVO >> beh.log
