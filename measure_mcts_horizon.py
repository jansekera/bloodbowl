#!/usr/bin/env python3
"""Team1 Agent B — NEEDS ENGINE RUN (engine import required).

Measures the EFFECTIVE search horizon of a 100-sim macro-MCTS search rooted at a
pre-pickup state, and whether any leaf in the tree ever reaches a TOUCHDOWN/GAME_OVER
phase (i.e. whether a real TD reward is ever in scope).

Because simulate() in macro_mcts.cpp is a STATIC leaf eval (no rollout), the only way
the search can "see" a TD is if the MCTS tree itself expands a path of macros that ends
in a scoring phase. This script instruments that.

Run on the build host:
  PYTHONPATH=engine/build:python python measure_mcts_horizon.py --weights weights_best.json --sims 100
"""
import argparse, sys
try:
    import bb  # engine pybind module (engine/build)
except Exception as e:
    print("NEEDS ENGINE RUN: cannot import bb module:", e); sys.exit(1)

# Pseudocode of the intended instrumentation (exact API depends on bb_module bindings):
#   1. Build a mid-game state with a loose ball one square from a standing player (pre-pickup).
#   2. Run MacroMCTS search with logging of the most-visited path depth.
#   3. Report: (a) max tree depth in macros, (b) #leaves whose phase==TOUCHDOWN/GAME_OVER,
#      (c) whether the chosen macro is PICKUP / forward vs END_TURN / safe.
# Compare sims in {25,50,100,200,400} to see if more sims ever surfaces a TD line.
print("This script is a template; wire to the bb bindings on the build host.")
print("Expected finding (from code): leaf eval is static; tree depth on most-visited")
print("path at 100 sims is ~1-2 macros; TD phase is reached at a leaf only in the")
print("rare case the carrier is already within MA+2 of the endzone at the ROOT.")
