#!/usr/bin/env python3
"""Item 13 MVP: mine PICKUP-goal turn-start states for the staged-planner
diagnostic harness (diag_item13_staged_planner_harness.cpp).

Reads the READ-ONLY replay corpus diag_replay_mine_20260730_data/ (24
self-play games, post-item7/item10 engine, turn-start board snapshots) and
selects turn-start states where the active team's goal would be PICKUP_BALL:

  - ball on the ground (not held, on pitch),
  - the active team has a plausible picker (standing player within MA+2 of
    the ball),
  - at least `min_support` additional standing teammates within 6 squares of
    the ball (so the safe backup stage has actual work to do -- states with
    zero possible backups can't measure the item 11 closure),
  - at most `per_game` states per game for corpus diversity.

Each state also records what REALLY happened (turnover/touchdown flag of
that turn + whether the team held the ball at the start of its next own
turn) as a reality anchor for the harness report.

Usage:
    python3 diag_item13_mine_states.py [data_dir] [out_json] [max_states]

Defaults: data_dir=../../..../diag_replay_mine_20260730_data is NOT assumed;
pass the main-repo path explicitly, e.g.:
    python3 diag_item13_mine_states.py \
        /home/jan/claude/bloodbowl/diag_replay_mine_20260730_data \
        diag_item13_states.json 12
"""
import gzip
import json
import sys
from pathlib import Path


def chebyshev(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def mine(data_dir: Path, out_path: Path, max_states: int,
         per_game: int = 1, min_support: int = 2) -> None:
    states = []
    files = sorted(data_dir.glob("g*.json.gz"))
    print(f"scanning {len(files)} games in {data_dir}")
    for fp in files:
        if len(states) >= max_states:
            break
        with gzip.open(fp, "rt") as f:
            rec = json.load(f)
        turns = rec["turn_logs"]
        taken = 0
        # Mid-game loose-ball states first: every game trivially opens with a
        # first-turn kickoff pickup, and a corpus of nothing but h1t1 states
        # would validate only the opening. Within a game, prefer turn > 1.
        order = sorted(range(len(turns)),
                       key=lambda i: (turns[i]["half"] == 1 and turns[i]["turn"] == 1, i))
        for i in order:
            t = turns[i]
            if taken >= per_game or len(states) >= max_states:
                break
            if t["ball_held"] or not (0 <= t["ball_x"] <= 25):
                continue
            active = t["active_team"]
            mine_players = t[f"{active}_players"]
            standing = [p for p in mine_players if p["state"] == 0]
            picker_ok = any(
                chebyshev(p["x"], p["y"], t["ball_x"], t["ball_y"]) <= p["ma"] + 2
                for p in standing)
            support = sum(
                1 for p in standing
                if chebyshev(p["x"], p["y"], t["ball_x"], t["ball_y"]) <= 6) - 1
            if not picker_ok or support < min_support:
                continue

            # Reality anchor: did the team hold the ball at the start of its
            # next own turn?
            recovered = None
            for t2 in turns[i + 1:]:
                if t2["active_team"] != active:
                    continue
                if t2["ball_held"] and t2["ball_carrier_id"] > 0:
                    carrier_home = t2["ball_carrier_id"] <= 11
                    recovered = (carrier_home == (active == "home"))
                else:
                    recovered = False
                break

            players = []
            ko = []
            for side in ("home", "away"):
                for p in t[f"{side}_players"]:
                    if p["state"] == 3:
                        ko.append(p["id"])
                    else:
                        players.append({"id": p["id"], "x": p["x"], "y": p["y"],
                                        "st": p["state"]})
            states.append({
                "label": f"{fp.stem.split('.')[0]} h{t['half']}t{t['turn']} "
                         f"{active} ball=({t['ball_x']},{t['ball_y']}) "
                         f"support={support}",
                "game": fp.name,
                "half": t["half"], "turn": t["turn"], "active": active,
                "home_race": rec["home_race"], "away_race": rec["away_race"],
                "home_score": t["home_score"], "away_score": t["away_score"],
                "weather": t["weather"],
                "ball": [t["ball_x"], t["ball_y"]],
                "players": players, "ko": ko,
                "real_turnover": t["turnover"],
                "real_touchdown": t["touchdown"],
                "real_recovered_by_next_own_turn": recovered,
            })
            taken += 1
    with open(out_path, "w") as f:
        json.dump({"source": str(data_dir), "states": states}, f, indent=1)
    print(f"wrote {len(states)} states -> {out_path}")
    for s in states:
        print("  ", s["label"],
              f"real: TO={s['real_turnover']} TD={s['real_touchdown']} "
              f"recovered={s['real_recovered_by_next_own_turn']}")


if __name__ == "__main__":
    data_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("diag_item13_states.json")
    max_states = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    if data_dir is None or not data_dir.is_dir():
        print(__doc__)
        sys.exit(1)
    mine(data_dir, out_path, max_states)
