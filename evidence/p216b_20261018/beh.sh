#!/bin/bash
# P216b 18.10. (`340f50dd`): záloha přípravy (bez bezpečného pole se chystají všichni volní, ≥ 2 pole od sebe).
# Ramena: vše zapnuté × záloha vypnutá (EXTRAOFF=4), 20 semínek = 300 párů. Naváže po P213c.
# Čekám (mechanismus): víc maker HAND_OFF_SCORE / TD předávkou, míč déle v držení; cena: víc sražených (připravení v dosahu). TD až nakonec.
D=/home/jenda/claude/blood-bowl/evidence/p216b_20261018; W=/home/jenda/claude/bb-p216b
until grep -q HOTOVO /home/jenda/claude/blood-bowl/evidence/p213c_20261018/beh.log 2>/dev/null; do sleep 60; done
cd $D; date > beh.log
EXTRAOFF=0 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 noc_vse.pkl 20 >> beh.log 2>&1 &
EXTRAOFF=4 JADER=4 nice -n 5 /usr/local/bin/python3.8 ab.py $W 0 noc_bez.pkl 20 >> beh.log 2>&1 &
wait; date >> beh.log; echo HOTOVO >> beh.log
