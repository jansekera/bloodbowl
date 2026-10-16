#!/bin/bash
# P213 noc 16.→17.10.: tytéž ramena, 30 semínek (450 párů) — potvrdit / vyvrátit úbytek TD předávkou (23→17, 1,9σ).
D=/home/jenda/claude/blood-bowl/evidence/p213_20261016; W=/home/jenda/claude/bb-p213
cd $D; date > noc.log
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 noc_vse.pkl 30 >> noc.log 2>&1 &
JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 1073741824 noc_bez.pkl 30 >> noc.log 2>&1 &
wait; date >> noc.log; echo HOTOVO >> noc.log
