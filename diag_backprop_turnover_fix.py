#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-03) for the turnover/terminal-discard fix in
macro_mcts.cpp: MacroMCTSSearch::search() used to silently discard the
ENTIRE MCTS iteration (no backpropagate call at all) whenever an open-loop
replayToNode() hit a turnover or a terminal phase (TOUCHDOWN/GAME_OVER/
HALF_TIME) partway through the path -- meaning only "lucky" replays that
didn't turn over ever contributed to a node's statistics, systematically
under-penalizing risky-but-scoring branches (roadmap Tier 2 item 4,
flagged independently by Fable 5 and cross-checked against Opus 4.8's
negamax finding). Fixed: replayToNode() now returns the deepest node
actually reached plus whether it completed; on an incomplete replay,
search() backpropagates simulate() evaluated on the REAL resulting state
(reusing the existing, already-tuned heuristic+VF blend) instead of
discarding. This touches core MCTS backprop semantics -- tested in
ISOLATION, not bundled with any other change, per the roadmap's explicit
caution.

Single-arm test (no clean "before" binary without a wasteful rebuild/revert
cycle). Compares against the last established reference point BEFORE this
fix: diag_pickup_cage_throttle.py's N=150 result, 48W 72D 27L = 49.0% draws
(n=147) -- that number already includes both PICKUP fixes and policy
priors, but predates this backprop fix.

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
policy_path=weights_policy.json (matches shipped production default,
GATE_USE_POLICY_PRIORS=1). Reuses production _gate_game/_imap_watchdog.
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
      f"(+ backprop-turnover-discard fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'backprop-turnover-fix',
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
print(f"\n=== MIRROR gate champion vs champion  backprop-turnover-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_pickup_cage_throttle.py, N=150): "
      f"49.0% draws, 32.7% home win, 18.4% home loss (n=147).", flush=True)
