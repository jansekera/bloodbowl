#!/bin/bash
# P207 noční párové A/B: báze 1ff3a39e × nové e9edbe35, 800 poločasů na rameno (N=40)
cd /home/jenda/claude/blood-bowl
D=evidence/sloty_20261014
date > $D/noc.log
/usr/local/bin/python3.8 $D/mech.py /home/jenda/claude/bb-p207-baze $D/baze.pkl 40 >> $D/noc.log 2>&1
date >> $D/noc.log
/usr/local/bin/python3.8 $D/mech.py /home/jenda/claude/bb-p207-nove $D/nove.pkl 40 >> $D/noc.log 2>&1
date >> $D/noc.log; echo HOTOVO >> $D/noc.log
