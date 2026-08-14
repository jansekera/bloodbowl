#!/bin/bash
# Noční staged iterace #3 (06.→07.08.) — auto-launcher, session-nezávislý.
# Čeká na: (1) čas >= 18:30 UTC, (2) žádná běžící iterace, (3) žádné těžké
# diag měření Fable agenta (tempo doktrína běží odpoledne). Pak kontroly
# (stash) a start pod PROMOTION FREEZE (default ON v kódu od 5658f83).
cd /home/jan/claude/bloodbowl
STAMP() { date -u '+%H:%M'; }
LOG=launch_staged_night_20260806.done

while true; do
    H=$(date -u '+%H%M')
    if [ "$H" -ge 1830 ]; then
        BUSY=0
        ps -eo args | grep -v grep | grep -qE "python3 run_iteration|diag_f1_adoption_probe|diag_f1_cage_advance|run_tempo_measure|diag_.*ab_|run_h2h" && BUSY=1
        # měřicí řetěz tempo doktríny musí být hotový (marker ALL_DONE)
        [ -f tempo_measure_20260806/ALL_DONE ] || BUSY=1
        # load průměr < počet jader/2 = Fable měření nejspíš skončila
        LOAD=$(awk '{printf "%d", $1}' /proc/loadavg)
        [ "$BUSY" -eq 0 ] && [ "$LOAD" -lt 6 ] && break
    fi
    sleep 300
done

if ps -eo args | grep -v grep | grep -q "python3 run_iteration"; then
    echo "[$(STAMP)] ABORT: běží jiná run_iteration" >> "$LOG"; exit 1
fi
if [ ! -f weights_policy.json ]; then
    echo "[$(STAMP)] ABORT: weights_policy.json CHYBÍ" >> "$LOG"; exit 1
fi
md5sum weights_policy.json weights_best.json >> "$LOG"

BB_GATE_POLICY_BLEND=0.2 BB_STAGED_PICKUP=1 setsid nohup nice -n 19 \
    python3 run_iteration.py > training_staged_20260806_night.log 2>&1 &
disown
echo "[$(STAMP)] noční staged iterace #3 spuštěna, PID $!" >> "$LOG"
