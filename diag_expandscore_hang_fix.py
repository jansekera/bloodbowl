#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-09) for the SCORE-macro off-pitch-carrier hang
fix (macro_actions.cpp's expandScore, commit TBD). Root-caused via a live
gdb-attach repro (fuzz_gate_gdb.py, this session) after hang_analysis.md
(2026-06-25) had actually RULED OUT this exact loop as a hang candidate --
its static analysis assumed a valid on-pitch carrier and showed cx/cy
converge to (targetX, testY) in <=PITCH_WIDTH+PITCH_HEIGHT steps, which is
correct *given that assumption*. What it missed: replayToNode replays a
cached macro path open-loop (fresh dice each MCTS iteration), so by the time
a SCORE macro for player X is replayed, X can have been KO'd/crowd-surfed off
pitch by an *earlier* macro in that same replay. expandScore read
carrier.position without checking carrier.isOnPitch() first, so carrier.position
was the {-1,-1} off-pitch sentinel. For an AWAY carrier (dx=-1, targetX=0)
starting at x=-1, the raw TZ-probe walk's cx decrements *away* from 0 and only
terminates after a signed-integer-overflow wraparound -- confirmed live via
gdb: cx observed at 224583956 mid-spin on a real hung process (pid 143103,
carrier id=18, teamSide=AWAY, state=KO, position={-1,-1}), i.e. ~4 billion
iterations in, technically undefined behavior, and ~100-200s of wall time
per occurrence -- the dominant contributor to the 2026-07-08 gate abort
(6/200, 7/200, 15/600 watchdog skips, all > the 2% MAX_SKIP_FRAC tolerance).

Fix: bail out of expandScore immediately if `!carrier.isOnPitch()` (mirrors
the guard expandBlitzAndScore already had before its own movePlayerToward
call), plus a hard iteration cap on the TZ-probe while loop as
defense-in-depth (PITCH_WIDTH+PITCH_HEIGHT, matching the loop's own
established convergence bound, so it can never spin even if a future bug
reintroduces an invalid carrier here).

Single-arm smoke, self-mirror, MCTS=100, policy_path=weights_policy.json
(production default). Compares against the current reference point:
diag_pickup_stepcap_fix_150's N=150 result, 47.3% draws (n=146) -- this
session's fresh baseline (post-PICKUP-stepcap-fix, the most recent landed
change). The metric that matters most here is the WATCHDOG-SKIP COUNT,
which should drop back toward the ~1.3% (2/150) post-BLITZ-fix floor (or
lower) -- not the draw-rate, which this fix is not expected to move much
(it only prevents a rare stale-replay state, doesn't change strategy).
"""
import random
import sys
from pathlib import Path
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 30
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 12
init_args = ("engine/build", "python")

if not Path(POLICY_PATH).exists():
    print(f"ERROR: {POLICY_PATH} not found in cwd -- run from repo root.", file=sys.stderr)
    sys.exit(1)

print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={POLICY_PATH!r} "
      f"(+ SCORE-macro off-pitch-carrier hang fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'expandscore-hang-fix',
                                  mcts_iterations=MCTS_ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 10 == 0 or done == N:
            total = wins + draws + losses
            print(f"  {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== MIRROR gate champion vs champion  expandscore-hang-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_pickup_stepcap_fix_150, N=150): "
      f"47.3% draws, 26.7% home win, 26.0% home loss (n=146).", flush=True)
print(f"  KEY METRIC for this fix specifically: watchdog-skip count above "
      f"should be lower than the ~2.5-3.5%/150 rate seen in the last three "
      f"post-BLITZ-fix runs (blitz-fix 2/150, halfclock 5/150, "
      f"pickup-stepcap 4/150), ideally back toward the ~1.3% (2/150) "
      f"post-BLITZ-fix floor.", flush=True)
