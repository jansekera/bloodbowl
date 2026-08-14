#!/usr/bin/env bash
# Autonomous continuation (2026-07-07): smoke-test then launch the N=150
# decisive run for the BLITZ/Tentacles hang fix (commit c6d7b5b), fully
# detached so it survives session/SSH loss. Build+tests already done and
# passing (404/404) before this script was launched.
set -uo pipefail
cd "$(dirname "$0")"
LOG=autonomous_continue_blitz_hang_20260707.log
log() { echo "[$(date -u +%H:%M:%S)] $*" | tee -a "$LOG"; }

log "=== n=20 smoke for BLITZ/Tentacles hang fix (diag_blitz_tentacles_hang_fix.py) ==="
venv/bin/python3 diag_blitz_tentacles_hang_fix.py 20 >> "$LOG" 2>&1
log "=== smoke complete (see $LOG above for result -- no gate on draw-rate for this fix, just check no crash) ==="

log "=== launching N=150 decisive run for BLITZ/Tentacles hang fix (detached) ==="
setsid nohup venv/bin/python3 diag_blitz_tentacles_hang_fix.py 150 > diag_blitz_tentacles_hang_fix_150_20260707.log 2>&1 < /dev/null &
disown
log "=== N=150 launched, PID $! -- output to diag_blitz_tentacles_hang_fix_150_20260707.log ==="
log "=== autonomous continuation script DONE. Next: read diag_blitz_tentacles_hang_fix_150_20260707.log once it completes (~1-2h), paying special attention to the watchdog-skip count (should be lower than ~3-5/150). ==="
