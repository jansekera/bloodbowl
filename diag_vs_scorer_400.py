#!/usr/bin/env python3
"""Decisive cheap diagnosis (bb-metrics-eval): does the champion DEFEND and ANSWER
against an opponent that actually advances/scores, or does it get scored on and fail?
If it wins comfortably vs greedy, the 0-0 collapse is a same-search mirror artifact
(deprioritize panic). If greedy scores freely and champion can't answer -> genuine defect.
"""
import sys
sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from blood_bowl.cpp_runner import CPPRunner

N = int(sys.argv[1]) if len(sys.argv) > 1 else 48
W = "weights_best.json"
runner = CPPRunner(".")
for opp in ("greedy", "random"):
    res = runner.simulate(home_ai="macro_mcts", away_ai=opp,
        matches=N, timeout=180, timeout_per_game=True,
        weights=W, epsilon=0.0, mcts_iterations=400, vf_blend=0.0, workers=12)
    g = res.results
    n = len(g)
    homew = sum(r.home_score > r.away_score for r in g)
    draws = sum(r.home_score == r.away_score for r in g)
    away_td = sum(r.away_score for r in g) / n
    home_td = sum(r.home_score for r in g) / n
    print(f"\n=== champion(macro_mcts) vs {opp}  n={n} ===")
    print(f"home win : {homew}/{n} = {100*homew/n:.0f}%   draws {draws}/{n}")
    print(f"home TD/g: {home_td:.2f}   AWAY(conceded) TD/g: {away_td:.2f}")
    print(f"  PASS(defends): home win >=70% AND away TD/g < 1.0")
