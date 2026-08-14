#!/usr/bin/env python3
"""Outcome-level follow-up (2026-07-03) to diag_cage_after_pickup.py.

Question: when the ball-holding team has CAGE in the MCTS candidate list, is
choosing BLOCK instead of CAGE a legitimate strategy or a search bias?
Selection frequencies alone can't answer that -- this script traces what
happens to the DRIVE after each such decision.

Method:
- Simulate fresh self-mirror games (macro_mcts vs macro_mcts, production
  config: weights_best.json, mcts=100, policy_blend=0, vf_blend=0).
- Align each policy decision to its team-turn via the exact key
  (perspective, half*8+turnNumber) recovered from state feature f3
  (= (turnNumber+(half-1)*8)/16, feature_extractor.cpp).
- For each decision with i_have_ball (f12) and CAGE in the candidate list,
  record chosen macro, CAGE/BLOCK visit fractions, and context:
  f15*26 carrier dist-to-endzone, f32*8 turns remaining in half,
  f40*4 tacklezones on carrier, f56*4 cage corners already filled.
- Trace forward over turn logs (snapshots at turn start, with per-snapshot
  scores and per-player has_ball) until: own-team score increases (TD),
  team no longer holds ball at a turn boundary (LOST), half/game ends (END).

Emits one JSON line per qualifying decision to the output path.
Usage: diag_cage_vs_block_outcomes.py SEED_START N_GAMES OUT.jsonl
"""
import sys, json
import numpy as np

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
import bb_engine

MACRO_NAMES = ["SCORE", "ADVANCE", "CAGE", "BLITZ", "BLOCK", "PICKUP",
               "PASS", "FOUL", "REPOSITION", "END_TURN"]

SEED_START = int(sys.argv[1])
N_GAMES = int(sys.argv[2])
OUT = sys.argv[3]


def macro_of(action_features):
    for i, name in enumerate(MACRO_NAMES):
        if i < len(action_features) and action_features[i] > 0.5:
            return name
    return None


def team_holds_ball(turn, team):
    players = turn["home_players"] if team == "home" else turn["away_players"]
    return any(p["has_ball"] for p in players)


def trace_outcome(turns, ti, team, final_home, final_away):
    """Outcome of the drive for `team` holding the ball during turn index ti."""
    my_key = "home_score" if team == "home" else "away_score"
    opp_key = "away_score" if team == "home" else "home_score"
    half0 = turns[ti]["half"]
    for k in range(ti, len(turns)):
        # scores after the events of turn k = snapshot k+1 (or final result)
        if k + 1 < len(turns):
            my_next, opp_next = turns[k + 1][my_key], turns[k + 1][opp_key]
        else:
            my_next = final_home if team == "home" else final_away
            opp_next = final_away if team == "home" else final_home
        if my_next > turns[k][my_key]:
            return "TD", k - ti
        if opp_next > turns[k][opp_key]:
            return "LOST_OPP_TD", k - ti
        if k + 1 >= len(turns):
            return "END", k - ti
        nxt = turns[k + 1]
        if nxt["half"] != half0:
            return "END", k - ti
        if not team_holds_ball(nxt, team):
            return "LOST", k + 1 - ti
    return "END", len(turns) - 1 - ti


records = []
for gi in range(N_GAMES):
    seed = SEED_START + gi
    home = bb_engine.get_developed_roster("human", 1000)
    away = bb_engine.get_developed_roster("human", 1000)
    lgr = bb_engine.simulate_game_logged(
        home, away, "macro_mcts", "macro_mcts",
        seed=seed,
        weights_path="weights_best.json",
        epsilon=0.0,
        mcts_iterations=100,
        policy_weights_path="weights_policy.json",
        policy_blend=0.0,
        vf_blend=0.0,
    )
    decisions = lgr.get_policy_decisions()
    turns = lgr.get_turn_logs()
    final_home, final_away = lgr.result.home_score, lgr.result.away_score

    # index turn logs by (team, total_turn); on duplicates keep FIRST
    turn_index = {}
    for i, t in enumerate(turns):
        key = (t["active_team"], (t["half"] - 1) * 8 + t["turn"])
        turn_index.setdefault(key, i)

    n_unaligned = 0
    for dec in decisions:
        sf = dec["state_features"]
        visits = dec["visits"]
        if not visits:
            continue
        if sf[12] <= 0.5:          # i_have_ball
            continue
        cage_vfs, block_vfs = [], []
        for v in visits:
            m = macro_of(v["action_features"])
            if m == "CAGE":
                cage_vfs.append(v["visit_fraction"])
            elif m == "BLOCK":
                block_vfs.append(v["visit_fraction"])
        if not cage_vfs:           # CAGE not available
            continue
        best = max(visits, key=lambda v: v["visit_fraction"])
        chosen = macro_of(best["action_features"])

        total_turn = int(round(sf[3] * 16))
        key = (dec["perspective"], total_turn)
        ti = turn_index.get(key)
        if ti is None:
            n_unaligned += 1
            continue
        outcome, dt = trace_outcome(turns, ti, dec["perspective"],
                                    final_home, final_away)
        records.append({
            "seed": seed,
            "team": dec["perspective"],
            "total_turn": total_turn,
            "ti": ti,
            "chosen": chosen,
            "chosen_vf": float(best["visit_fraction"]),
            "cage_vf_max": float(max(cage_vfs)),
            "cage_vf_sum": float(sum(cage_vfs)),
            "cage_n": len(cage_vfs),
            "block_vf_max": float(max(block_vfs)) if block_vfs else None,
            "block_vf_sum": float(sum(block_vfs)) if block_vfs else None,
            "block_n": len(block_vfs),
            "dist_td": float(sf[15] * 26.0),
            "turns_rem": float(sf[32] * 8.0),
            "carrier_tz": float(sf[40] * 4.0),
            "cage_corners": float(sf[56] * 4.0),
            "outcome": outcome,
            "turns_to_outcome": dt,
        })
    print(f"game {gi+1}/{N_GAMES} seed={seed} done "
          f"(unaligned={n_unaligned})", flush=True)

with open(OUT, "w") as f:
    for r in records:
        f.write(json.dumps(r) + "\n")
print(f"wrote {len(records)} records to {OUT}")
