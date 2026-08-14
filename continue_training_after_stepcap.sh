#!/bin/bash
# Autonomous continuation (2026-07-08): waits for the currently-running
# finding-6 (PICKUP step-cap) N=150 decisive run to finish (shares the same
# 12 CPU cores as a fresh training loop would -- launching both at once
# would 2x-oversubscribe the machine and risk corrupting the diagnostic
# run's 180s watchdog-skip timing measurement), then pushes committed fixes
# to origin (mandatory before training -- see feedback_commit_before_training)
# and launches a fresh full training loop (default production config) to
# get a post-fix baseline now that this batch of engine correctness fixes
# (halfclock, kicking-team, FPU children[0], BLITZ/Tentacles hang, PICKUP
# step-cap) is complete.
set -uo pipefail
cd /home/jan/claude/bloodbowl || exit 1

log() { echo "[$(date -u +%H:%M:%S)] $*"; }

log "=== waiting for finding-6 (pickup-stepcap-fix) N=150 run to finish ==="
while pgrep -f "diag_pickup_stepcap_fix.py 150" > /dev/null 2>&1; do
    sleep 30
done
log "=== finding-6 N=150 run finished ==="
log "--- final tail of diag_pickup_stepcap_fix_150_20260708.log ---"
tail -20 diag_pickup_stepcap_fix_150_20260708.log

log "=== pushing committed fixes to origin (mandatory before training) ==="
git push origin main 2>&1

log "=== launching fresh N=1 full training loop (default production config, detached) ==="
setsid nohup venv/bin/python3 run_iteration.py --loop 1 --no-push \
    > training_post_stepcap_fix_20260708.log 2>&1 < /dev/null &
disown
log "=== training launched, PID $! -- output to training_post_stepcap_fix_20260708.log ==="
log "=== 16 epochs x 40 games, MCTS=100, BM=400, GATE=600, default config -- ETA ~3-5h ==="
log "=== autonomous continuation script DONE. ==="
