#!/bin/bash
# P220 diagnostika: agilní hráči v kontaktu na konci tahu — jak se tam dostali (s P219 / bez P219).
D=/home/jenda/claude/blood-bowl/evidence/p220_20261020; cd $D; date > beh.log
JADER=7 nice -n 5 /usr/local/bin/python3.8 diag.py /home/jenda/claude/blood-bowl diag.pkl 6 >> beh.log 2>&1
date >> beh.log; echo HOTOVO >> beh.log
