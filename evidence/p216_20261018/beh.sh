#!/bin/bash
# P216 18.10.: verze `eba21a2b`; ramena vše zapnuté × P216 vypnuté (kFeatReadySurround = 2147483648); 20 semínek = 300 párů.
# Spustí se až po doběhnutí P213b. Čekám: TD předávkou / makra HAND_OFF_SCORE u silnějších beze ztrát, méně ztracených míčů; TD až nakonec.
D=/home/jenda/claude/blood-bowl/evidence/p216_20261018; W=/home/jenda/claude/bb-p216
until grep -q HOTOVO /home/jenda/claude/blood-bowl/evidence/p213b_20261018/beh.log 2>/dev/null; do sleep 60; done
cd $D; date > beh.log
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 noc_vse.pkl 20 >> beh.log 2>&1 &
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 2147483648 noc_bez.pkl 20 >> beh.log 2>&1 &
wait; date >> beh.log; echo HOTOVO >> beh.log
