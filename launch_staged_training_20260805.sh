#!/bin/bash
# Auto-launcher (05.08. večer, předsunuto z 06.→07.08. — kotvy doběhly dřív):
# počká na doběhnutí policy-vs-policy H2H, provede kontroly a spustí PRVNÍ
# ostrou iteraci s celotahovým plánovačem sebrání (BB_STAGED_PICKUP=1).
cd /home/jan/claude/bloodbowl
STAMP() { date -u '+%H:%M'; }
LOG=launch_staged_training_20260805.done

# 1) počkat na souboj intuicí (re-run 600 her)
while pgrep -f "run_h2h.py" >/dev/null; do sleep 60; done
sleep 30

# 2) nikdy nespouštět přes běžící iteraci
if ps -eo args | grep -v grep | grep -q "python3 run_iteration.py"; then
    echo "[$(STAMP)] ABORT: běží jiná run_iteration" >> "$LOG"; exit 1
fi

# 3) guard stashe (incident 04.08.): bez stashe nespouštět, jen hlásit
#    (carry-over má self-healing, ale radši hlasitý stop než tichá oprava)
if [ ! -f weights_policy.json ]; then
    echo "[$(STAMP)] ABORT: weights_policy.json CHYBÍ — prošetřit před startem" >> "$LOG"
    exit 1
fi
md5sum weights_policy.json weights_best.json >> "$LOG"

# 4) start: policy blend 0.2 (produkční) + item13 staged planner POPRVÉ ostře
BB_GATE_POLICY_BLEND=0.2 BB_STAGED_PICKUP=1 setsid nohup nice -n 19 \
    python3 run_iteration.py > training_staged_20260805.log 2>&1 &
disown
echo "[$(STAMP)] staged trénink spuštěn, PID $!" >> "$LOG"
