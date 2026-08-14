#!/bin/bash
# Jednorázový auto-archiv replay bufferu iterace #2 (06.08.).
# Důvod: iterace #2 v2 (PID 115687) startovala PŘED commitem 07d6b09
# (automatický replay archiv), a noční iterace #3 by buffer přepsala.
# Čeká na konec run_iteration, pak kopíruje. Session-nezávislý (setsid).
cd /home/jan/claude/bloodbowl
LOG=archive_replay_iter2_20260806.log
while ps -p 115687 >/dev/null 2>&1; do sleep 120; done
sleep 60  # nechat doběhnout finální zápisy
if [ -f replay_buffer.pkl ]; then
    cp replay_buffer.pkl "replay_archive/replay_20260806_manual_iter2.pkl"
    echo "[$(date -u '+%H:%M')] archivováno: $(md5sum replay_archive/replay_20260806_manual_iter2.pkl)" >> "$LOG"
else
    echo "[$(date -u '+%H:%M')] CHYBA: replay_buffer.pkl neexistuje" >> "$LOG"
fi
