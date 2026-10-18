#!/bin/bash
# P213b 18.10.: verze `b77b8c55` (útěk před TD předávkou + útěk s GFI/úhybem do 3+, jen když bez hodu nejde).
# Tatáž ramena a semínka jako noc 16.→17.10. (vše zapnuté × P213 vypnuté, 30 semínek = 450 párů).
# Čekám: nosič sražen / míč ztracen v tahu soupeře méně než v noci (−0,038 ± 0,022), víc maker HAND_OFF_SCORE. TD až nakonec.
D=/home/jenda/claude/blood-bowl/evidence/p213b_20261018; W=/home/jenda/claude/bb-p213b
cd $D; date > beh.log
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 noc_vse.pkl 30 >> beh.log 2>&1 &
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 1073741824 noc_bez.pkl 30 >> beh.log 2>&1 &
wait; date >> beh.log; echo HOTOVO >> beh.log
