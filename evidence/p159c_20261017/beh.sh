#!/bin/bash
# P159 nová verze `220095ce` (riziko počítané přímo, bez 48 zkušebních her) — krátké měření 17.10.:
# 15 semínek = 180 párů. Rozhoduje čas na tah (má být ~jako bez odkladu) a propadlé aktivace; TD jen s výhradou.
D=/home/jenda/claude/blood-bowl/evidence/p159c_20261017; W=/home/jenda/claude/bb-p159
cd $D; date > beh.log
RISK=0 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W $D/baze.pkl 15 >> beh.log 2>&1 &
RISK=1 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W $D/nove.pkl 15 >> beh.log 2>&1 &
wait; date >> beh.log; echo HOTOVO >> beh.log
