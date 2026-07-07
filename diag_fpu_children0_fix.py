#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-07) for audit finding 4 (Fable 5 fresh-eyes
pass, 2026-07-03, project_bloodbowl_audit_findings_20260703): after
`expand(node, sim)`, macro_mcts.cpp's search() always did
`node = &node->children[0]` before the first rollout of a newly-expanded
node. `getAvailableMacros` always pushes END_TURN first
(macro_actions.cpp:145), so a newly-expanded branch's FIRST-ever visit --
which seeds FPU for all its siblings -- was always the passive END_TURN
continuation, systematically biasing FPU toward passivity tree-wide,
independent of any child's actual prior.

Fixed: descend into the highest-prior child instead of always children[0]:
    MacroMCTSNode* bestChild = &node->children[0];
    for (auto& child : node->children) {
        if (child.prior > bestChild->prior) bestChild = &child;
    }
    node = bestChild;

Narrower/more localized than finding 2 (turn-clock) -- touches search
dynamics (which child seeds FPU), not game rules -- but still a genuine
behavioral change to MCTS, so measured in isolation, not bundled with
finding 6 (PICKUP step-cap fix) which was sitting in the same working tree.
Single-arm smoke, self-mirror, MCTS=100, policy_path=weights_policy.json
(production default).

Compares against the current reference point: diag_halfclock_fix_150's
N=150 result, 45.5% draws (n=145) -- this session's fresh baseline
(post-halfclock-fix, the most recent landed change before this one).
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
      f"(+ FPU children[0] fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'fpu-children0-fix',
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
print(f"\n=== MIRROR gate champion vs champion  fpu-children0-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_halfclock_fix_150, N=150): "
      f"45.5% draws, 29.0% home win, 25.5% home loss (n=145).", flush=True)
