#!/bin/bash
# Prázdné blitzy: události každého našeho blitzu (aktuální main), 36 poločasů, 2 jádra.
D=/home/jenda/claude/blood-bowl/evidence/prazdne_blitzy_20261021; cd $D; date > beh.log
JADER=2 nice -n 5 /usr/local/bin/python3.8 sonda.py /home/jenda/claude/blood-bowl blitzy.pkl 3 >> beh.log 2>&1
date >> beh.log; echo HOTOVO >> beh.log
