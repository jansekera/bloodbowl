#!/usr/bin/env python3
"""Cheapest decisive falsification test proposed by Fable 5 (fresh-eyes
cross-model review, 2026-07-02): does gating actually measure anything about
the candidate's trained weights at all?

Established facts (verified 2026-07-02 against engine/python/bb_module.cpp
and run_iteration.py):
- Production gating (_gate_game) always runs at VF_BLEND=0.0 (run_iteration.py:38).
- macro_mcts.cpp:537 only reads valueFn_ when `vfBlend > 0.0f` -- at 0.0 the
  loaded value function, whatever its weights, is NEVER evaluated.
- _gate_game never passes policy_weights_path either, so both sides use
  uniform + Dirichlet priors, not the learned/shaped policy prior.
- => at vf_blend=0, "candidate" (gate_path weights) and "champion" (frozen_path
  weights) are mechanistically IDENTICAL agents: same heuristic-only leaf eval,
  same prior scheme. The only difference between them should be statistical
  noise (RNG/seed), not anything about which weights file was loaded.

This test: home side loads NO weights at all (weights_path="", so
bb_module.cpp:416-418 skips loading a value function entirely -- guarantees
zero possible influence from any weights file), away side loads the current
champion (weights_best.json, loaded but per the above, never evaluated
either). If gating draws are governed only by the shared heuristic/mirror
dynamic (not by which weights are loaded), this should reproduce the same
~48-51% draw rate as the champion-vs-champion self-mirror test
(diag_mirror_budget.py, N=150, MCTS=100: 51.0% draws).

PASS (gating measures nothing about weights): draw/win/loss split
  statistically indistinguishable from the self-mirror baseline (~51% draws,
  ~coin-flip win/loss split) -- confirms gating has been comparing two
  functionally identical heuristic agents this whole time.
FAIL (weights DO matter somehow): meaningfully different split from the
  self-mirror baseline -- would mean something else (not yet identified) lets
  the loaded weights affect behavior even at vf_blend=0, and the mechanism
  needs to be found before trusting this conclusion.
"""
import random
import sys
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 150
ITERS = 100
CHAMPION = "weights_best.json"
NO_WEIGHTS = ""
TV = 1000
VF_BLEND = 0.0
WORKERS = 12
init_args = ("engine/build", "python")

print(f"\n--- starting NULL-WEIGHTS gate: home=NO WEIGHTS vs away=champion  "
      f"MCTS={ITERS} n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, NO_WEIGHTS, CHAMPION, ITERS, VF_BLEND, TV)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'null-weights',
                                  mcts_iterations=ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 10 == 0 or done == N:
            total = wins + draws + losses
            print(f"  [null-weights] {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== NULL-WEIGHTS gate: home=NO WEIGHTS vs away=champion  "
      f"MCTS={ITERS}  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print(f"  Compare to self-mirror baseline (diag_mirror_budget.py, N=150, MCTS=100): "
      f"51.0% draws, 26.8% home win, 22.1% home loss.", flush=True)
