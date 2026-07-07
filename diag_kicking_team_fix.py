#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-07) for audit finding 3 (Fable 5 fresh-eyes
pass, 2026-07-03, project_bloodbowl_audit_findings_20260703): after a
TOUCHDOWN, game_simulator.cpp's TOUCHDOWN-phase branch (both simulateGame
and simulateGameLogged loops) unconditionally did
`state.kickingTeam = opponent(state.kickingTeam)` -- just flipping the
previous kicker/receiver. Real rule: the SCORING team kicks off next. This
happens to be correct when the receiving team scores (the common case) but
is WRONG when the DEFENDING team scores (an interception/loose-ball TD) --
the scorer should receive the next kickoff too, but the flip gave it to the
original receiver instead.

Fixed: both TOUCHDOWN branches now do
`state.kickingTeam = state.getPlayer(state.ball.carrierId).teamSide`
(carrier is still valid at this point -- checkTouchdown() requires
ball.isHeld, and nothing resets the ball before this branch runs). The
HALF_TIME branches (separate, correct "receiver of half 1 kicks half 2"
rule) are untouched.

This changes real gameplay dynamics specifically around defensive-steal
states (who kicks off after an interception TD), narrower blast radius than
finding 2 (turn-clock reset) but still a genuine behavior change, not pure
diagnostics like finding 5. Single-arm smoke, self-mirror, MCTS=100,
policy_path=weights_policy.json (production default).

Compares against the current reference point: diag_cage_prior_fix.py's
N=150 result, 42.6% draws (n=148) -- includes every fix landed through
2026-07-03 (cage prior floor was the last one before this).
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
      f"(+ kicking-team-after-TD fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'kicking-team-fix',
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
print(f"\n=== MIRROR gate champion vs champion  kicking-team-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to pre-fix reference (diag_cage_prior_fix.py, N=150): "
      f"42.6% draws, 33.1% home win, 24.3% home loss (n=148).", flush=True)
