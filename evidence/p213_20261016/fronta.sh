#!/bin/bash
until grep -q HOTOVO /home/jenda/claude/blood-bowl/evidence/p180b_20261016/ab.log 2>/dev/null; do sleep 60; done
D=/home/jenda/claude/blood-bowl/evidence/p213_20261016; W=/home/jenda/claude/bb-p213
cd $D; date > ab.log
JADER=4 /usr/local/bin/python3.8 ab.py $W 0 vse.pkl 10 >> ab.log 2>&1 &
JADER=4 /usr/local/bin/python3.8 ab.py $W 1073741824 bez.pkl 10 >> ab.log 2>&1 &
wait; date >> ab.log; echo HOTOVO >> ab.log
