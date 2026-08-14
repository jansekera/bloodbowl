#!/usr/bin/env python3
"""
measure_advance_horizon.py  —  NEEDS ENGINE RUN (read-only diagnostic, no training)

Goal: confirm the SEARCH-SIDE root cause before committing the #1 fix.

Two questions this script answers empirically:

  Q1 (depth / TD-reachability): From a realistic mid-drive root, how often does
      macro-MCTS (100 sims) ever EXPAND onto a TOUCHDOWN/GAME_OVER leaf? The
      established claim is "essentially never". This measures it.

  Q2 (forward-pull magnitude): Holding a carrier at varying distance-to-endzone,
      what leaf value does simulate() return for ADVANCE vs CAGE vs END_TURN?
      If ADVANCE's leaf value is NOT monotonically higher than sitting still as
      the carrier nears the endzone, the search has no gradient to score.

  Q3 (ADVANCE under-step): instrument how many squares ADVANCE actually moves the
      carrier early-game (expandAdvance caps at mvRemaining/2 unless turnsRemaining<=2).
      If a MA6 carrier only advances ~3 squares/turn it cannot cross 25 squares in 8
      turns while also caging -> structural stall.

How to run (engine must be built; this does NOT launch training):
    cd /home/jan/claude/bloodbowl
    python3 measure_advance_horizon.py

NOTE: the exact pybind entry points (simulate-one-game, per-decision child-visit
dump) must be wired to whatever bb_engine exposes. Below uses the documented
`bb_engine` sim API; if a needed hook is missing, add a thin read-only getter —
do NOT change control flow. Marked NEEDS-ENGINE-RUN: results are NOT yet measured.
"""
import sys, statistics
sys.path.insert(0, "engine/build")

try:
    import bb_engine
except Exception as e:
    print("NEEDS ENGINE BUILD: could not import bb_engine:", e)
    sys.exit(0)

# ---------------------------------------------------------------------------
# Q1 + Q3: play N self-play games, instrument macro decisions.
# Requires a build that can log child-visits per decision (lastChildVisits is
# already exposed internally; expose a per-game counter of TOUCHDOWN-leaf hits).
# If unavailable, this block prints a TODO and we fall back to Q2 (pure leaf eval).
# ---------------------------------------------------------------------------
def main():
    print("=== measure_advance_horizon.py (NEEDS ENGINE RUN) ===")
    print("Q2 is the decisive cheap measurement and needs only leaf-eval access.\n")

    # Q2: synthetic carrier-distance sweep over simulate() leaf value.
    # Pseudocode against whatever single-state eval the module exposes.
    # We want: leaf(ADVANCE-result-state) as carrier dist 24->0, with vfBlend=0.
    #
    # If bb_engine exposes `eval_leaf(state, perspective, vfBlend)` use it directly.
    # Otherwise build states via the scenario constructor and call the macro policy
    # with sims=100, reading lastBestValue / child-visit split for ADVANCE vs CAGE.
    if not hasattr(bb_engine, "make_scenario") and not hasattr(bb_engine, "eval_leaf"):
        print("TODO/NEEDS-HOOK: add a read-only `eval_leaf(state, side, vfBlend)` or")
        print("`make_scenario(carrier_x=..., turn=...)` getter to bb_module.cpp.")
        print("No control-flow change; just exposes simulate() for offline sweep.")
        print("\nUntil then: results UNMEASURED. Do not claim numbers.")
        return

    # --- sweep (only runs if hooks exist) ---
    dists = list(range(24, -1, -1))
    for vfb in (0.0, 0.5):
        print(f"\n-- vfBlend={vfb} : leaf value vs carrier dist-to-EZ --")
        for d in dists:
            st = bb_engine.make_scenario(carrier_x=d, turn=4)   # NEEDS-HOOK
            v = bb_engine.eval_leaf(st, side=st.active_team, vfBlend=vfb)  # NEEDS-HOOK
            print(f"  dist={d:2d}  leaf={v:+.3f}")
    print("\nINTERPRET: forward pull is healthy iff leaf rises monotonically as dist->0,")
    print("for BOTH vfBlend values. A flat/non-monotone curve at vfBlend=0 means the")
    print("leaf heuristic itself under-rewards progress (fix proposal #1).")

if __name__ == "__main__":
    main()
