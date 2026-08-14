#!/bin/bash
# A3-2 launcher (05.08.2026). PŘEDSUNUTO na odpoledne (pokyn uživatele 05.08.):
# startuje hned, jak doběhne Fable měření (diag_policy_vs_policy) — CPU je pak
# do večera volné a kotvy doběhnou ještě dnes.
# Spouštět: setsid nohup ./run_a3_2_night_20260805.sh > a3_2_launcher_20260805.log 2>&1 &
cd "$(dirname "$0")"

# Nešlapat po dobíhajícím Fable měření (max ~3 h čekání, pak jedeme)
WAITED=0
while pgrep -f "diag_policy_vs_policy" > /dev/null 2>&1 && [ "$WAITED" -lt 10800 ]; do
    echo "$(date -u +%H:%M) čekám na konec Fable měření..."
    sleep 300
    WAITED=$((WAITED + 300))
done

echo "$(date -u +%H:%M) START A3-2 (6 ramen x 300 párů)"
nice -n 19 ./venv/bin/python diag_a3_2_anchor_20260805.py --pairs 300 \
    > a3_2_run_20260805/measure_20260805.log 2>&1
echo "$(date -u +%H:%M) A3-2 launcher končí (exit $?)"
