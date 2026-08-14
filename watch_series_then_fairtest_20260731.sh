#!/bin/bash
# Watcher 31.07.: ceka na dobehnuti no-reset serie, pak post-serie checklist
# (evidence/fable_policy_activation_design_20260731.md par. 7) a start kroku 1
# (potvrzovaci policy fairtest). GO uzivatele 31.07. (dotaz c. 1 fronty).
#
# Pouceni z 30.07. pgrep-self-match deadlocku: cekani ciste pres PID (kill -0),
# zadny pgrep na pattern obsazeny v tomto skriptu; skript se spousti z REALNEHO
# souboru, takze jeho cmdline je jen cesta k nemu.
cd /home/jan/claude/bloodbowl || exit 1
exec >> watch_series_then_fairtest_20260731.log 2>&1

SERIES_PID=1984
EXPECTED_MD5=b426c64d55c172fe16e273928716b1ce   # weights_best.json == zaloha 30.07.

echo "[$(date -Is)] watcher start (pid $$, ppid $PPID)"

if grep -q run_iteration "/proc/$SERIES_PID/cmdline" 2>/dev/null; then
    echo "[$(date -Is)] serie bezi (PID $SERIES_PID), cekam (check po 5 min)"
    while kill -0 "$SERIES_PID" 2>/dev/null; do sleep 300; done
    echo "[$(date -Is)] PID $SERIES_PID skoncil"
else
    echo "[$(date -Is)] PID $SERIES_PID neni run_iteration -> serie uz nebezi"
fi
echo "--- tail training_noreset_20260731.log ---"
tail -5 training_noreset_20260731.log
echo "-------------------------------------------"

sleep 60   # usazeni FS zapisu serie

# guard 1: nesmi bezet zadny jiny run_iteration (napr. rucne restartovana serie)
if pgrep -f 'python3 run_iteration' > /dev/null; then
    echo "[$(date -Is)] ABORT: bezi jiny run_iteration, test NESTARTUJI"
    touch fairtest_ABORTED_other_run.alert
    exit 1
fi

# guard 2: sampion nedotcen (NO_RESET best nikdy nepise -- overit, ne verit)
ACTUAL_MD5=$(md5sum weights_best.json | cut -d' ' -f1)
if [ "$ACTUAL_MD5" != "$EXPECTED_MD5" ]; then
    echo "[$(date -Is)] ABORT: weights_best.json md5 $ACTUAL_MD5 != $EXPECTED_MD5"
    touch fairtest_ABORTED_md5.alert
    exit 1
fi
echo "[$(date -Is)] guardy OK (zadny run_iteration, sampion md5 $ACTUAL_MD5)"

# snapshot post-serie policy (serie weights_policy.json prubezne prepisovala)
SNAP="evidence/policy_snapshot_postnoreset_$(date +%Y%m%d).json"
cp weights_policy.json "$SNAP"
echo "[$(date -Is)] policy snapshot: $(md5sum "$SNAP")"

setsid nohup python3 diag_policy_confirm_20260731.py "$SNAP" \
    > diag_policy_confirm_20260731.log 2>&1 < /dev/null &
FAIRTEST_PID=$!
disown
sleep 10
if kill -0 "$FAIRTEST_PID" 2>/dev/null; then
    echo "[$(date -Is)] fairtest SPUSTEN (PID $FAIRTEST_PID), ~3 h @ Pool(6)"
    touch "fairtest_launched_$(date +%Y%m%d_%H%M).done"
else
    echo "[$(date -Is)] ABORT: fairtest spadl hned po startu, viz diag_policy_confirm_20260731.log"
    touch fairtest_ABORTED_launch_fail.alert
    exit 1
fi
