#!/bin/bash
# P159 o víkendu „pořádně“ (uživatel 16.10.: „spusť ho až po resetu v sobotu v 10 ráno“): tytéž zápasy a semínka jako ab.py, ale 60 semínek (720 párů na rameno).
until [ "$(date +%s)" -ge "$(date -d '2026-10-17 10:00' +%s)" ]; do sleep 60; done
until grep -q HOTOVO /home/jenda/claude/blood-bowl/evidence/p180_p187_20261016/ab.log 2>/dev/null; do sleep 60; done
D=/home/jenda/claude/blood-bowl/evidence/p159_20261016; W=/home/jenda/claude/bb-p159
cd $D; date > vikend.log
RISK=0 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W $D/vikend_baze.pkl 60 >> vikend.log 2>&1 &
RISK=1 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W $D/vikend_nove.pkl 60 >> vikend.log 2>&1 &
wait; date >> vikend.log; echo HOTOVO >> vikend.log
