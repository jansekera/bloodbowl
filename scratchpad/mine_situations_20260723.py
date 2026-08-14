"""Exploratory situation mining over diag_replay_mine_20260721_data (24 games).

Read-only analysis for a human discussion. Home attacks x=25, away attacks x=0.
Home ids 1-11, away 12-22. Turn snapshots are start-of-turn.
"""
import glob
import gzip
import json
from collections import Counter

DATA = "diag_replay_mine_20260721_data/g*.json.gz"


def endzone_x(team):
    return 25 if team == "home" else 0


def dist_to_endzone(team, x):
    return (25 - x) if team == "home" else x


def carrier_of(t):
    cid = t["ball_carrier_id"]
    if not t["ball_held"] or cid < 0:
        return None
    side = "home" if cid <= 11 else "away"
    plist = t["home_players"] if side == "home" else t["away_players"]
    p = [q for q in plist if q["id"] == cid]
    return (side, p[0]) if p else None


def main():
    files = sorted(glob.glob(DATA))
    noncarrier_gfi_fails = []
    stall_candidates = []       # own carrier close to endzone, didn't score this turn
    halfend_in_range = []       # last own turn of half, carrier in range, no TD
    fouls_ctx = []
    loose_ball_streaks = []
    carrier_retreats = []
    turnover_causes = Counter()
    pass_events = []

    for fp in files:
        rec = json.load(gzip.open(fp, "rt"))
        g = fp.split("/")[-1]
        turns = rec["turn_logs"]

        # loose-ball streak tracking
        streak = 0
        streak_start = None
        for i, t in enumerate(turns):
            if not t["ball_held"]:
                if streak == 0:
                    streak_start = (t["half"], t["turn"], t["active_team"],
                                    t["ball_x"], t["ball_y"])
                streak += 1
            else:
                if streak >= 4:
                    loose_ball_streaks.append((g, streak_start, streak))
                streak = 0
        if streak >= 4:
            loose_ball_streaks.append((g, streak_start, streak))

        for i, t in enumerate(turns):
            at = t["active_team"]
            car = carrier_of(t)
            ev_types = [e["type"] for e in t["events"]]

            # turnover cause = last meaningful event before TURNOVER
            if "TURNOVER" in ev_types:
                idx = ev_types.index("TURNOVER")
                cause = None
                for e in reversed(t["events"][:idx]):
                    if e["type"] in ("GFI", "DODGE", "PICKUP", "CATCH", "PASS",
                                     "BLOCK", "KNOCKED_DOWN", "FOUL"):
                        cause = e["type"] + ("" if e["success"] is False else "?")
                        break
                turnover_causes[cause or "unknown"] += 1

            # non-carrier failed GFI -> turnover
            for e in t["events"]:
                if e["type"] == "GFI" and not e["success"]:
                    pid = e["player_id"]
                    was_carrier = car and car[1]["id"] == pid
                    noncarrier_gfi_fails.append(
                        (g, t["half"], t["turn"], at, pid, was_carrier,
                         (e["from_x"], e["from_y"]), (e["to_x"], e["to_y"]),
                         t["home_score"], t["away_score"]))

            # active team's carrier close to endzone at start of turn
            if car and car[0] == at:
                d = dist_to_endzone(at, car[1]["x"])
                scored = t["touchdown"]
                if d <= 4 and not scored:
                    # count defenders between carrier and endzone-ish (rough)
                    opp = t["away_players"] if at == "home" else t["home_players"]
                    ahead = [p for p in opp if p["state"] == 0 and
                             dist_to_endzone(at, p["x"]) <= d + 1]
                    stall_candidates.append(
                        (g, t["half"], t["turn"], at, car[1]["id"],
                         (car[1]["x"], car[1]["y"]), d, len(ahead),
                         t["home_score"], t["away_score"], t["turnover"]))
                # last own turn of half (turn 8)
                if t["turn"] == 8 and d <= 8 and not scored:
                    halfend_in_range.append(
                        (g, t["half"], at, car[1]["id"],
                         (car[1]["x"], car[1]["y"]), d,
                         t["home_score"], t["away_score"]))

                # carrier net retreat this turn (from events)
                moves = [e for e in t["events"]
                         if e["player_id"] == car[1]["id"] and
                         e["type"] in ("MOVE", "GFI", "DODGE")]
                if moves:
                    x0, x1 = moves[0]["from_x"], moves[-1]["to_x"]
                    prog = (x1 - x0) if at == "home" else (x0 - x1)
                    if prog <= -3:
                        carrier_retreats.append(
                            (g, t["half"], t["turn"], at, car[1]["id"],
                             (x0, moves[0]["from_y"]), (x1, moves[-1]["to_y"]),
                             prog, t["home_score"], t["away_score"]))

            # fouls: context
            for e in t["events"]:
                if e["type"] == "FOUL":
                    fouls_ctx.append((g, t["half"], t["turn"], at,
                                      e["player_id"], e["target_id"],
                                      e["roll"], e["success"],
                                      t["ball_held"],
                                      t["home_score"], t["away_score"]))
                if e["type"] == "PASS":
                    pass_events.append((g, t["half"], t["turn"], at, e))

    print("=== turnover causes ===")
    for k, v in turnover_causes.most_common():
        print(f"  {k}: {v}")

    print(f"\n=== failed GFIs ({len(noncarrier_gfi_fails)}) ===")
    nc = [x for x in noncarrier_gfi_fails if not x[5]]
    print(f"  non-carrier: {len(nc)}, carrier: {len(noncarrier_gfi_fails)-len(nc)}")
    for x in nc[:15]:
        print("  ", x)

    print(f"\n=== stall candidates (carrier d<=4 at own turn start, no TD) "
          f"({len(stall_candidates)}) ===")
    for x in stall_candidates:
        print("  ", x)

    print(f"\n=== half-end turn8 carrier in range d<=8, no TD "
          f"({len(halfend_in_range)}) ===")
    for x in halfend_in_range:
        print("  ", x)

    print(f"\n=== loose-ball streaks >=4 turns ({len(loose_ball_streaks)}) ===")
    for x in loose_ball_streaks:
        print("  ", x)

    print(f"\n=== carrier retreats <= -3 squares ({len(carrier_retreats)}) ===")
    for x in carrier_retreats:
        print("  ", x)

    print(f"\n=== fouls ({len(fouls_ctx)}) ===")
    loose = [x for x in fouls_ctx if not x[8]]
    print(f"  fouls while ball loose: {len(loose)}")
    succ = [x for x in fouls_ctx if x[7]]
    print(f"  'successful' (armor broken?): {len(succ)}")
    for x in loose[:10]:
        print("  loose-ball foul:", x)

    print(f"\n=== passes ({len(pass_events)}) ===")
    for x in pass_events:
        print("  ", x)


if __name__ == "__main__":
    main()
