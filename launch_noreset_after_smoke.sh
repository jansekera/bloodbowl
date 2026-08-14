#!/bin/bash
# Auto-launcher: počká na doběhnutí smoke, pak spustí no-reset sérii.
cd /home/jan/claude/bloodbowl
while pgrep -f "run_iteration.py --no-push --loop 1" >/dev/null; do sleep 30; done
sleep 10
# čistý start iterace 1 ze šampiona (smoke nechal své az_train na disku)
rm -f weights_az_train.json weights_az_train_meta.json
BB_NO_RESET=1 BB_GATE=600 setsid nohup python3 run_iteration.py --loop 4 --no-push \
  > training_noreset_20260730.log 2>&1 &
disown
echo "$(date '+%H:%M') no-reset serie spustena, PID $!" >> launch_noreset_after_smoke.done
