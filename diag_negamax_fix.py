#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-03) for the missing-negamax fix in
macro_mcts.cpp (roadmap Tier 2 item 5, flagged independently by Opus 4.8
2026-07-01 and Fable 5 2026-07-02): MacroMCTSNode::bestChildPUCT() always
maximized Q (stored in the search's fixed searchingSide perspective)
regardless of whose decision a node's children actually represented --
once the open-loop tree crosses an END_TURN boundary into the opponent's
turn, selection kept picking the child best FOR searchingSide, i.e. an
implicitly cooperative opponent.

Fixed: MacroMCTSNode now tracks `actingTeam` (set in expand() from the
state's activeTeam), and select()/bestChildPUCT() flip to minimizing Q
(rank by -Q) whenever a node's actingTeam differs from searchingSide.
backpropagate()/simulate() are untouched -- Q stays in a single fixed
perspective throughout, only the SELECTION formula's treatment of it
changes. Riskiest lever in this batch (touches core tree-search adversarial
assumption) -- tested here in ISOLATION per the roadmap's explicit caution,
after item 4 (turnover-discard fix, commit 716e64e) already landed and was
confirmed non-regressive at N=150.

Single-arm test (no clean "before" binary without a wasteful rebuild/revert
cycle). Compares against the last established reference point BEFORE this
fix: diag_backprop_turnover_fix.py's N=150 result, 43W 70D 34L = 47.6%
draws (n=147) -- includes both PICKUP fixes, policy priors, AND the
turnover-discard backprop fix, but predates this negamax fix.

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
      f"(+ negamax fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'negamax-fix',
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
print(f"\n=== MIRROR gate champion vs champion  negamax-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_backprop_turnover_fix.py, N=150): "
      f"47.6% draws, 29.3% home win, 23.1% home loss (n=147).", flush=True)
