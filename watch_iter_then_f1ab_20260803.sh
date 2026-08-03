#!/bin/bash
# Watcher 03.08.: ceka na konec iterace (PID 49374) cistym kill -0 (pouceni
# 31.07. - zadny pgrep pattern matching), pak spusti F1 A/B mereni ze
# worktree agent-ac2fc491e08aef722 (4 matchupy paralelne, nice -19).
# Merge/vyhodnoceni NEDELA - to je prace Clauda rano.
ITER_PID=49374
WT=/home/jan/claude/bloodbowl/.claude/worktrees/agent-ac2fc491e08aef722
ROOT=/home/jan/claude/bloodbowl
LOG=$ROOT/watch_iter_then_f1ab_20260803.log
STAMP() { date -u +%Y-%m-%dT%H:%M:%S+00:00; }

echo "[$(STAMP)] watcher start (pid $$, ppid $PPID), cekam na iteraci PID $ITER_PID" >> "$LOG"
while kill -0 "$ITER_PID" 2>/dev/null; do sleep 120; done
echo "[$(STAMP)] iterace PID $ITER_PID skoncila" >> "$LOG"
sleep 60

# guard: zadna dalsi iterace nesmi bezet (match na python3 binarku, ne na text)
if ps -eo args | grep -v grep | grep -q "python3 run_iteration.py"; then
  echo "[$(STAMP)] ABORT: bezi jina run_iteration" >> "$LOG"
  touch "$ROOT/f1ab_ABORTED_$(date +%Y%m%d_%H%M).alert"
  exit 1
fi

md5sum "$ROOT/weights_best.json" >> "$LOG"
ls -la "$ROOT/weights_best_policy.json" >> "$LOG" 2>&1  # existence = gate PROMOTED s blendem

cd "$WT" || { touch "$ROOT/f1ab_ABORTED_cd_$(date +%Y%m%d_%H%M).alert"; exit 1; }
for m in 0 1 2 3; do
  setsid nohup nice -n 19 ./diag_f1_cage_advance . 400 "$m" \
    > "diag_f1_m${m}_20260803.log" 2>&1 &
  echo "[$(STAMP)] matchup $m spusten (pid $!)" >> "$LOG"
done
touch "$ROOT/f1ab_launched_$(date +%Y%m%d_%H%M).done"
echo "[$(STAMP)] hotovo - 4 matchupy bezi" >> "$LOG"
