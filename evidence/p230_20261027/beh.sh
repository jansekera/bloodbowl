#!/bin/bash
# P230 měření 27.10.: 2 ramena (vše / bez P230), jen náš tah + odpověď soupeře, 48 poločasů útoku.
# Běží na knihovně z worktree p228-spinavy-roh (main se nepřestavuje). Spouštět: setsid nohup bash beh.sh > /dev/null 2>&1 &
D="$(cd "$(dirname "$0")" && pwd)"; KOREN=/home/jenda/claude/blood-bowl/.claude/worktrees/p228-spinavy-roh; cd "$D"; date > beh.log
JADER=6 nice -n 10 /usr/local/bin/python3.8 mereni.py "$KOREN" mereni.pkl 6 >> beh.log 2>&1
/usr/local/bin/python3.8 vyhodnot.py mereni.pkl > souhrn_tabulka.txt 2>> beh.log
date >> beh.log; echo HOTOVO >> beh.log
