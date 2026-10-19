#!/bin/bash
# P219 (blitz) a pak P219b (odskok), každé zvlášť, jen tah + odpověď soupeře.
D=/home/jenda/claude/blood-bowl/evidence/p219_20261019; cd $D; date > beh.log
MASKA=32 JADER=7 nice -n 5 /usr/local/bin/python3.8 situace.py /home/jenda/claude/blood-bowl blitz.pkl 6 8 >> beh.log 2>&1
MASKA=64 JADER=7 nice -n 5 /usr/local/bin/python3.8 situace.py /home/jenda/claude/blood-bowl odskok.pkl 6 8 >> beh.log 2>&1
date >> beh.log; echo HOTOVO >> beh.log
