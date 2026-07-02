#!/usr/bin/env python3
"""A/B test (2026-07-02) for Tier 1 item 1 of project_bloodbowl_roadmap_20260702:
does activating the dormant MCTS prior floor/cap heuristics in gating (by
loading ANY policy net file, policy_blend left at 0) change the mirror draw
rate, independent of any trained-weight sensitivity?

Background: macro_mcts.cpp expand() (~lines 212-368) has hand-coded prior
floor/cap rules (PICKUP floor 0.20-0.35, SCORE-family situational floors,
END_TURN cap 0.10, etc.) gated on `config_.policy != nullptr` -- NOT on
policy_blend. Self-play always loads a policy file (use_policy = policy_lr>0,
always true in production) so self-play always gets this regime; gating and
benchmark never load ANY policy file, so they run on flat uniform + Dirichlet
priors only, missing this whole regime. weights_policy.json (from the
2026-06-30 self-play run) already exists on disk. This test loads it for
BOTH sides with policy_blend=0 unset (engine default), so ONLY the floor/cap
block activates -- no actual trained-policy CONTENT influences anything
(that's a separate, bigger Tier 2 step: policy_blend>0 with each side's own
distinct policy).

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
A/B: policy_path='' (current production behavior) vs policy_path=weights_policy.json.
Reuses the PRODUCTION _gate_game/_imap_watchdog helpers from run_iteration.py.

SMOKE-SCALE by default (n=30/arm) -- this is a directional check, not a
decision-grade measurement. Does not touch weights_best.json or production
gating. Pass a larger N as argv[1] for a more decisive run once smoke looks
promising.
"""
import random
import sys
from multiprocessing import Pool
from pathlib import Path

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

for label, policy_path in (("baseline (no policy net)", ""), ("dormant-priors (policy net loaded, blend=0)", POLICY_PATH)):
    print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={policy_path!r} n={N} ---", flush=True)
    tasks = [
        (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, policy_path)
        for i in range(N)
    ]
    wins = draws = losses = 0
    done = 0
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, f'gate-prior-{bool(policy_path)}',
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
                print(f"  [{label}] {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                      f"= {100*draws/total:.1f}% draws", flush=True)

    total = wins + draws + losses
    print(f"\n=== MIRROR gate champion vs champion  {label}  n={total} (of {N} requested) ===", flush=True)
    print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
          f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
    print(f"  Compare to self-mirror baseline (diag_mirror_budget.py, N=150, MCTS=100): "
          f"51.0% draws, 26.8% home win, 22.1% home loss.", flush=True)
