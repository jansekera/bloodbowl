#!/usr/bin/env python3
"""Follow-up investigation (2026-07-03) after the pickup-cage-throttle fix
(commit 2af4252) showed no measurable draw-rate change at N=150.

The fix only *frees up* movement/decision-window for a cage to form after a
pickup -- it doesn't force the AI to actually choose CAGE. This script mines
fresh decision logs from the CURRENT build (post-fix) to check whether the
MCTS macro-policy actually picks (or even seriously considers) CAGE in the
turns right after a PICKUP macro fires, using the same decision-log
methodology as evidence/fable_replay_mining_findings.md (macro one-hot order:
[0]=SCORE [1]=ADVANCE [2]=CAGE [3]=BLITZ [4]=BLOCK [5]=PICKUP [6]=PASS
[7]=FOUL [8]=REPOSITION [9]=END_TURN; state feature f12=I-have-ball,
f14=ball-on-ground, per feature_extractor.cpp).
"""
import sys
import numpy as np
from collections import Counter

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
import bb_engine

MACRO_NAMES = ["SCORE", "ADVANCE", "CAGE", "BLITZ", "BLOCK", "PICKUP",
               "PASS", "FOUL", "REPOSITION", "END_TURN"]

N_GAMES = int(sys.argv[1]) if len(sys.argv) > 1 else 20
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"

after_pickup_chosen = Counter()
after_pickup_cage_available = 0
after_pickup_total = 0
cage_decisions_total = 0
cage_chosen_total = 0
cage_available_but_not_chosen_top_visit = Counter()

for gi in range(N_GAMES):
    home = bb_engine.get_developed_roster("human", 1000)
    away = bb_engine.get_developed_roster("human", 1000)
    lgr = bb_engine.simulate_game_logged(
        home, away, "macro_mcts", "macro_mcts",
        seed=1000 + gi,
        weights_path=W,
        epsilon=0.0,
        mcts_iterations=100,
        policy_weights_path=POLICY_PATH,
        policy_blend=0.0,
        vf_blend=0.0,
    )
    print(f"  game {gi+1}/{N_GAMES} simulated", flush=True)
    decisions = lgr.get_policy_decisions()

    prev_was_pickup_chosen = False
    prev_perspective = None

    for dec in decisions:
        sf = dec["state_features"]
        visits = dec["visits"]
        if not visits:
            continue
        # argmax visit fraction = chosen macro (mcts.cpp argmax convention)
        best = max(visits, key=lambda v: v["visit_fraction"])
        best_idx = int(np.argmax(best["action_features"][:10])) if len(best["action_features"]) >= 10 else None
        # Determine chosen macro name via one-hot in the chosen visit's action_features
        chosen_name = None
        af = best["action_features"]
        for i, name in enumerate(MACRO_NAMES):
            if i < len(af) and af[i] > 0.5:
                chosen_name = name
                break

        i_have_ball = sf[12] > 0.5
        ball_on_ground = sf[14] > 0.5

        if i_have_ball:
            cage_available = False
            for v in visits:
                vaf = v["action_features"]
                if len(vaf) > 2 and vaf[2] > 0.5:
                    cage_available = True
                    break
            if cage_available:
                cage_decisions_total += 1
                if chosen_name == "CAGE":
                    cage_chosen_total += 1

            if prev_was_pickup_chosen and prev_perspective == dec["perspective"]:
                after_pickup_total += 1
                if chosen_name:
                    after_pickup_chosen[chosen_name] += 1
                if cage_available:
                    after_pickup_cage_available += 1

        prev_was_pickup_chosen = (chosen_name == "PICKUP")
        prev_perspective = dec["perspective"]

print(f"=== {N_GAMES} games, current build (post pickup-cage-throttle fix) ===")
print(f"\nDecisions with ball held AND CAGE macro available in the candidate list: {cage_decisions_total}")
print(f"  Of those, CAGE actually chosen (top visit fraction): {cage_chosen_total} "
      f"({100*cage_chosen_total/max(1,cage_decisions_total):.1f}%)")
print(f"\nDecisions immediately following a chosen PICKUP (same perspective): {after_pickup_total}")
print(f"  Of those, CAGE was in the candidate list: {after_pickup_cage_available} "
      f"({100*after_pickup_cage_available/max(1,after_pickup_total):.1f}%)")
print(f"  Macro actually chosen right after PICKUP:")
for name, cnt in after_pickup_chosen.most_common():
    print(f"    {name}: {cnt} ({100*cnt/max(1,after_pickup_total):.1f}%)")
