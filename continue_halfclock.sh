#!/bin/bash
# Autonomous continuation (2026-07-07): waits for the currently-running
# finding-3 (kicking-team) N=150 decisive run to finish (it has the current
# .so mmap'd, so rebuilding concurrently risks corrupting it), then carries
# the finding-2 (half-clock) fix through build -> test -> smoke -> commit ->
# decisive N=150. All output goes to this log so it survives session loss.
set -uo pipefail
cd /home/jan/claude/bloodbowl || exit 1

log() { echo "[$(date -u +%H:%M:%S)] $*"; }

log "=== waiting for finding-3 (kicking-team-fix) N=150 run to finish ==="
while pgrep -f "diag_kicking_team_fix.py 150" > /dev/null 2>&1; do
    sleep 30
done
log "=== finding-3 N=150 run finished ==="
log "--- final tail of diag_kicking_team_fix_150_20260707.log ---"
tail -20 diag_kicking_team_fix_150_20260707.log

log "=== rebuilding engine for finding-2 (half-clock) fix ==="
cd engine/build || exit 1
if ! make -j"$(nproc)" 2>&1; then
    log "=== BUILD FAILED -- aborting, finding-2 changes remain uncommitted ==="
    exit 1
fi
log "=== build OK, running full test suite ==="
if ! ./bb_tests 2>&1; then
    log "=== TESTS FAILED -- aborting, finding-2 changes remain uncommitted for manual inspection ==="
    exit 1
fi
log "=== all tests pass ==="
cd /home/jan/claude/bloodbowl || exit 1

log "=== running n=20 smoke for half-clock fix (diag_halfclock_fix.py) ==="
venv/bin/python3 diag_halfclock_fix.py 20 2>&1
log "=== smoke complete ==="

log "=== committing finding-2 (half-clock) fix ==="
git add engine/include/bb/game_simulator.h engine/src/game_simulator.cpp engine/tests/test_game_simulator.cpp diag_halfclock_fix.py
git commit -m "$(cat <<'EOF'
fix(rules): touchdown no longer resets the half's turn clock/rerolls

game_simulator.cpp's TOUCHDOWN branches called setupHalf() to restart play
after a score, but setupHalf's internals (buildTeam) unconditionally reset
turnNumber=0 and rerolls=3 for BOTH teams -- checkHalfOver is just
turnNumber > 8, so every touchdown granted both teams a fresh 8-turn half
instead of continuing the existing one. Net effect: a "half" was 8 turns
since the last score, not 8 turns total -- unbounded game length (capped
only by MAX_ACTIONS=5000), the conceding team always got a full fresh 8
turns to equalize, and every stall-aware/urgency mechanic keyed off
turnsRemaining was optimizing against a clock that scoring itself reset.

Split buildTeam's team-state reset behind a resetHalfState flag and added
setupDrive() (post-TD restart: re-places players/ball, preserves turn
clock and rerolls) alongside the unchanged setupHalf() (true half
boundaries only: game start, half-time). Both TOUCHDOWN branches now call
setupDrive instead of setupHalf; HALF_TIME branches are untouched.

Audit finding 2 from project_bloodbowl_audit_findings_20260703 (Fable 5
fresh-eyes pass, 2026-07-03) -- flagged there as plausibly a major
contributor to the entire draw-collapse investigation arc, and as needing
"a full session's attention" given it touches the core game loop and
invalidates every historical gating/benchmark baseline once landed (game
length distribution changes fundamentally -- treat post-fix numbers as a
fresh baseline, not a delta against prior references).

404/404 tests pass (403 existing + new regression test
GameSimulator.SetupDrivePreservesTurnClockAndRerolls, which manually sets
mid-half turnNumber/rerolls, calls setupDrive and asserts they survive,
then calls setupHalf and asserts they DO reset). n=20 smoke
(diag_halfclock_fix.py) results logged separately -- not compared against
pre-fix draw-rate references since this fix structurally changes
game-length dynamics.

Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>
EOF
)" 2>&1
log "=== commit done ==="

log "=== launching N=150 decisive run for half-clock fix (detached) ==="
setsid nohup venv/bin/python3 diag_halfclock_fix.py 150 > diag_halfclock_fix_150_20260707.log 2>&1 < /dev/null &
disown
log "=== N=150 launched, PID $! -- output to diag_halfclock_fix_150_20260707.log ==="
log "=== autonomous continuation script DONE. Next: read diag_kicking_team_fix_150 result above, and diag_halfclock_fix_150_20260707.log once it completes (~1-2h). ==="
