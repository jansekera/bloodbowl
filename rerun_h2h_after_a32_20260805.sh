#!/bin/bash
# Čeká na doběhnutí A3-2, pak čistě přespustí policy-vs-policy H2H (úkol 3
# Fable #1 zemřel 05.08. na 240/600 při ukončení agenta; pre-reg = 600 her).
cd "$(dirname "$0")"
while ! grep -q "ALL ARMS DONE" a3_2_run_20260805/measure_20260805.log 2>/dev/null; do
    sleep 600
done
nice -n 19 ./venv/bin/python diag_policy_vs_policy_20260805/run_h2h.py \
    > diag_policy_vs_policy_20260805/run_h2h_rerun.log 2>&1
echo "H2H RERUN DONE $(date -u +%H:%M)"
