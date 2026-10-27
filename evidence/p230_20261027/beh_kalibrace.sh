#!/bin/bash
# Kalibrace hrozby 27.10.: navazuje na beh.sh (čeká na HOTOVO), knihovna z worktree p228-spinavy-roh (nepřestavovat!).
D="$(cd "$(dirname "$0")" && pwd)"; KOREN=/home/jenda/claude/blood-bowl/.claude/worktrees/p228-spinavy-roh; cd "$D"
until grep -q HOTOVO beh.log; do sleep 60; done
date > beh_kalibrace.log
JADER=6 nice -n 10 /usr/local/bin/python3.8 kalibrace.py "$KOREN" kalibrace.pkl 8 8 >> beh_kalibrace.log 2>&1
/usr/local/bin/python3.8 kalibrace_vyhodnot.py kalibrace.pkl > kalibrace_tabulka.txt 2>> beh_kalibrace.log
date >> beh_kalibrace.log; echo HOTOVO >> beh_kalibrace.log
