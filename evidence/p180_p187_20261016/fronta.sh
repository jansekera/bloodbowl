#!/bin/bash
until grep -q HOTOVO /home/jenda/claude/blood-bowl/evidence/p159_20261016/ab.log; do sleep 30; done
cd /home/jenda/claude/blood-bowl/evidence/p180_p187_20261016; date > ab.log
JADER=3 /usr/local/bin/python3.8 ab.py /home/jenda/claude/bb-p159 0 vse.pkl 10 >> ab.log 2>&1 &
JADER=3 /usr/local/bin/python3.8 ab.py /home/jenda/claude/bb-p159 786432 bez180.pkl 10 >> ab.log 2>&1 &
JADER=2 /usr/local/bin/python3.8 ab.py /home/jenda/claude/bb-p159 3145728 bez187.pkl 10 >> ab.log 2>&1 &
wait; date >> ab.log; echo HOTOVO >> ab.log
