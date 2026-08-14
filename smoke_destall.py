#!/usr/bin/env python3
"""Smoke: de-stall ADVANCE (macro_actions.cpp). Paired self-play, champion both
sides, vfBlend=0, MCTS100. Compares draw / nil-nil / TD-per-game vs baseline.
Baseline (cage build, same harness): draws ~75%, nil-nil ~75%, ~0.42 TD/game.
"""
import sys
sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")
from blood_bowl.cpp_runner import CPPRunner

N = int(sys.argv[1]) if len(sys.argv) > 1 else 24
W = "weights_best.json"  # 89% champion, untouched
SIMS = 100

runner = CPPRunner(".")
res = runner.simulate(
    home_ai="macro_mcts", away_ai="macro_mcts",
    matches=N, timeout=180, timeout_per_game=True,
    weights=W, away_weights=W,
    epsilon=0.0, mcts_iterations=SIMS, vf_blend=0.0, workers=12,
)
games = res.results
n = len(games)
draws = sum(1 for r in games if r.home_score == r.away_score)
nilnil = sum(1 for r in games if r.home_score == 0 and r.away_score == 0)
tds = sum(r.home_score + r.away_score for r in games)
print(f"\n=== DE-STALL SMOKE n={n} (champion both sides, MCTS{SIMS}, vfBlend=0) ===")
print(f"draws    : {draws}/{n} = {100*draws/n:.0f}%   (baseline ~75%)")
print(f"nil-nil  : {nilnil}/{n} = {100*nilnil/n:.0f}%   (baseline ~75%)")
print(f"TD/game  : {tds/n:.2f}             (baseline ~0.42)")
print(f"scores   : " + " ".join(f"{r.home_score}-{r.away_score}" for r in games))
