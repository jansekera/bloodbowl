#!/usr/bin/env python3
"""Smoke test (2026-07-03) for the PASS targetId fix in rules_engine.cpp:
PASS actions were generated with targetId=-1 (rules_engine.cpp:79), but
expandPass/expandPassScore/expandChainScore (macro_actions.cpp) all match
candidate actions by targetId == receiver player id -- a match that could
never succeed. PASS_SCORE and CHAIN_SCORE macros were therefore guaranteed
no-ops (expand to zero actions -> MacroMCTSPolicy falls back to END_TURN),
and the long-range PASS macro only ever worked by accident when the
receiver happened to be adjacent (degenerating into a hand-off). Found by
an independent Fable 5 fresh-eyes audit (2026-07-03), verified directly
against the code before fixing. Fix: set targetId = teammate.id at
generation time (rules_engine.cpp:79) -- verified safe since
resolvePass() (action_resolver.cpp) dispatches on action.target (position),
never action.targetId.

This is a bigger behavioral change than the other fixes today (an entire
macro family goes from no-op to functional), so draws/outcomes may shift
more than the other levers. Single-arm smoke only, given time constraints
before a scheduled break -- a full N=150 decisive run should follow next
session.

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
policy_path=weights_policy.json. Reuses production _gate_game/_imap_watchdog.
"""
import random
import sys
from pathlib import Path
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 20
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 6  # reduced -- sharing the machine with a concurrent N=150 negamax run
init_args = ("engine/build", "python")

if not Path(POLICY_PATH).exists():
    print(f"ERROR: {POLICY_PATH} not found in cwd -- run from repo root.", file=sys.stderr)
    sys.exit(1)

print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={POLICY_PATH!r} "
      f"(+ PASS targetId fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'pass-targetid-fix',
                                  mcts_iterations=MCTS_ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 5 == 0 or done == N:
            total = wins + draws + losses
            print(f"  {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== MIRROR gate champion vs champion  pass-targetid-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_negamax_fix.py, n=30): "
      f"43.3% draws, 36.7% home win, 20.0% home loss (n=30).", flush=True)
