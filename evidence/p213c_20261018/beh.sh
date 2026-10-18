#!/bin/bash
# P213c 18.10. (verze `2f3dd213`): dvě změny P213 zvlášť, tatáž semínka (30 = 450 párů), vše ostatní zapnuté.
#   vse.pkl  = vše zapnuté · bez_poradi.pkl = staré pořadí (TD předávkou před útěkem) · bez_hodu.pkl = útěk jen bez hodu
# Čekám (mechanismus): každá část zvlášť — nosič sražen / míč ztracen v tahu soupeře, turnovery (u útěku s GFI podezření na +).
# TD až nakonec (pořadí: méně TD v témž poločase je podle doktríny uživatele přijatelné).
D=/home/jenda/claude/blood-bowl/evidence/p213c_20261018; W=/home/jenda/claude/bb-p213c
cd $D; date > beh.log
P213OFF=0 JADER=3 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 vse.pkl 30 >> beh.log 2>&1 &
P213OFF=1 JADER=3 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 bez_poradi.pkl 30 >> beh.log 2>&1 &
P213OFF=2 JADER=2 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 bez_hodu.pkl 30 >> beh.log 2>&1 &
wait; date >> beh.log; echo HOTOVO >> beh.log
