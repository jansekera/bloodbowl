#!/bin/bash
# P196 měření v noci 14.→15.10.: počká, až doběhne P207 (noc.log končí HOTOVO), pak 200 poločasů na main.
cd /home/jenda/claude/blood-bowl
L=evidence/sloty_20261014/noc.log
while ! grep -q HOTOVO $L; do sleep 60; done
D=evidence/p196_20261015
date > $D/noc2.log
/usr/local/bin/python3.8 $D/mereni.py /home/jenda/claude/blood-bowl $D/main.pkl 10 >> $D/noc2.log 2>&1
date >> $D/noc2.log; echo HOTOVO >> $D/noc2.log
