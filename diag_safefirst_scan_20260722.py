"""Cheap first-pass mining scan for priority 5 (safe-first systemic
check, project_bloodbowl_wrong_direction_dodge_20260721). Reuses the
existing 24-game 21.07 corpus, no new simulation.

Generalizes the earlier N=1 example (a player dodged INTO more tackle
zones than it started in, when it didn't have to) into a corpus-wide
count: for every DODGE event (a MOVE step that required a dodge roll,
i.e. leaving an opponent tacklezone), compare tackle-zone exposure at
the origin vs the destination square using the turn-start player
snapshot as an approximation.

Limitation: this only checks "did this specific step increase exposure
vs where it started", not "was there a strictly safer alternative
destination available for the same macro's purpose" (that needs real
pathfinding/candidate reconstruction, out of scope for a log scan). It's
a cheap necessary-but-not-sufficient signal, same spirit as the other
22.07 proxy scans.
"""
import gzip
import json
import glob

files = sorted(glob.glob("diag_replay_mine_20260721_data/g*.json.gz"))


def tz_count(pos, players):
    x, y = pos
    n = 0
    for p in players:
        if p["state"] != 0:
            continue
        if max(abs(p["x"] - x), abs(p["y"] - y)) == 1:
            n += 1
    return n


total_dodges = 0
increased_exposure = 0
same_or_less = 0
failed_dodges_with_increase = 0
examples = []

for fp in files:
    rec = json.load(gzip.open(fp, "rt"))
    turns = rec["turn_logs"]
    for t in turns:
        home_players = t["home_players"]
        away_players = t["away_players"]
        active = t["active_team"]
        active_players = home_players if active == "home" else away_players
        opp_players = away_players if active == "home" else home_players

        for ev in t.get("events", []):
            if ev["type"] != "DODGE":
                continue
            total_dodges += 1
            frm = (ev["from_x"], ev["from_y"])
            to = (ev["to_x"], ev["to_y"])
            tz_from = tz_count(frm, opp_players)
            tz_to = tz_count(to, opp_players)
            if tz_to > tz_from:
                increased_exposure += 1
                if not ev["success"]:
                    failed_dodges_with_increase += 1
                if len(examples) < 6:
                    examples.append({
                        "game": fp, "half": t["half"], "turn": t["turn"],
                        "player": ev["player_id"], "from": frm, "to": to,
                        "tz_from": tz_from, "tz_to": tz_to, "success": ev["success"],
                    })
            else:
                same_or_less += 1

print(f"games scanned: {len(files)}")
print(f"total DODGE events: {total_dodges}")
print(f"dodged INTO more tackle zones than started with: {increased_exposure} "
      f"({100*increased_exposure/total_dodges:.1f}%)" if total_dodges else "n/a")
print(f"  of those, the dodge roll FAILED (turnover risk materialized): "
      f"{failed_dodges_with_increase}")
print(f"dodged into same or fewer tackle zones: {same_or_less}")
print()
print("examples (dodged into MORE tackle zones):")
for e in examples:
    print(e)
