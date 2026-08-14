#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-02) for Tier 1 item 3 of
project_bloodbowl_roadmap_20260702: the PICKUP fallback fix in
macro_actions.cpp (findNearestFreePlayer() fallback now checks reach +
NoHands before being accepted, matching the main picker loop -- Opus 4.8
finding).

Single-arm test (not A/B against a stale binary -- the engine is already
rebuilt with the fix, so there's no clean "before" to compare against without
a wasteful rebuild/revert cycle). Instead compares directly against the
already-established reference: diag_gate_policy_prior.py's "dormant-priors"
arm (policy net loaded, blend=0, PRE this fix): N=150, 36W 74D 38L = 50.0%
draws (n=148).

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
policy_path=weights_policy.json (matches the new shipped production default,
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
      f"(+ PICKUP fallback fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'pickup-fix',
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
print(f"\n=== MIRROR gate champion vs champion  policy+pickup-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to policy-priors-on WITHOUT this fix (diag_gate_policy_prior.py, N=150): "
      f"50.0% draws, 24.3% home win, 25.7% home loss (n=148).", flush=True)
