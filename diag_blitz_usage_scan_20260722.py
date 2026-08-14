"""Cheap first-pass mining scan for two open blitz questions, reusing the
existing 24-game 21.07 corpus (no new simulation, engine unchanged):

1. Finding 1 proxy (project_bloodbowl_blitz_offense_floor_gap_20260722):
   is there a usage-rate gap between offense and defense blitz that's
   consistent with the offense-side prior-floor starvation theory? Not
   proof by itself (could also be genuinely correct tactical behavior),
   but a cheap first signal before committing to a full ADVANCE-style
   harness reconstruction.

2. Old priority 6 (lineman-fallback check,
   project_bloodbowl_kickoff_blitz_doctrine_20260721 item 3): among BLITZ
   events, what fraction target a "free" (zero enemy-tacklezone) standing
   opponent vs an already-covered one, and when NO free target exists,
   what actually gets targeted (dice-count proxy: opponent's own tackle
   zone count on that square, as a stand-in for "weak/lineman-like"
   target since race/skill stats aren't in this log format).
"""
import gzip
import json
import glob
from collections import Counter

files = sorted(glob.glob("diag_replay_mine_20260721_data/g*.json.gz"))


def side_of(pid, home_players, away_players):
    if any(p["id"] == pid for p in home_players):
        return "home"
    return "away"


def tz_count(pos, players, exclude_id=None):
    x, y = pos
    n = 0
    for p in players:
        if p["id"] == exclude_id:
            continue
        if p["state"] != 0:  # only standing players project tackle zones
            continue
        if max(abs(p["x"] - x), abs(p["y"] - y)) == 1:
            n += 1
    return n


off_opportunity = off_used = 0
def_opportunity = def_used = 0

blitz_free_target = 0
blitz_covered_target = 0
no_free_target_cases = []

for fp in files:
    rec = json.load(gzip.open(fp, "rt"))
    turns = rec["turn_logs"]
    for t in turns:
        home_players = t["home_players"]
        away_players = t["away_players"]
        active = t["active_team"]  # "home" or "away"
        ball_held = t["ball_held"]
        carrier_id = t.get("ball_carrier_id", -1)
        active_players = home_players if active == "home" else away_players
        opp_players = away_players if active == "home" else home_players

        active_has_ball = ball_held and carrier_id > 0 and any(
            p["id"] == carrier_id for p in active_players)
        on_offense = bool(active_has_ball)
        on_defense = ball_held and not active_has_ball

        any_standing_enemy = any(p["state"] == 0 for p in opp_players)
        blitz_events = [ev for ev in t.get("events", []) if ev["type"] == "BLOCK"
                        and ev.get("target_id") is not None]
        # crude blitz detector: reuse BLOCK events, can't distinguish blitz
        # from plain Block in this log format either -- so this measures
        # "any block used" as a proxy for "blitz opportunity taken",
        # acknowledged limitation below.
        used_any_block = len(t.get("events", [])) and any(
            ev["type"] == "BLOCK" for ev in t["events"])

        if on_offense and any_standing_enemy:
            off_opportunity += 1
            if used_any_block:
                off_used += 1
        elif on_defense and any_standing_enemy:
            def_opportunity += 1
            if used_any_block:
                def_used += 1

        # Target "freeness" at time of BLOCK event, using turn-start snapshot
        # (approximation -- positions may have shifted slightly by the time
        # of the actual block within the turn).
        for ev in t.get("events", []):
            if ev["type"] != "BLOCK":
                continue
            target_id = ev["target_id"]
            attacker_id = ev["player_id"]
            target_side = side_of(target_id, home_players, away_players)
            target_players = home_players if target_side == "home" else away_players
            attacker_side = "away" if target_side == "home" else "home"
            attacker_players = home_players if attacker_side == "home" else away_players
            tp = next((p for p in target_players if p["id"] == target_id), None)
            if tp is None:
                continue
            friendly_tz_on_target = tz_count((tp["x"], tp["y"]), attacker_players,
                                              exclude_id=attacker_id)
            if friendly_tz_on_target == 0:
                blitz_free_target += 1
            else:
                blitz_covered_target += 1

        # Any turn where offense/defense had a standing enemy but ALL
        # standing enemies were already covered by >=1 friendly tz --
        # what got targeted, if anything?
        if any_standing_enemy:
            all_covered = True
            for op in opp_players:
                if op["state"] != 0:
                    continue
                if tz_count((op["x"], op["y"]), active_players) == 0:
                    all_covered = False
                    break
            if all_covered:
                for ev in t.get("events", []):
                    if ev["type"] == "BLOCK":
                        no_free_target_cases.append(ev["target_id"])

print(f"games scanned: {len(files)}")
print()
print("=== Finding 1 proxy: block/blitz usage rate, offense vs defense ===")
print(f"offense: {off_used}/{off_opportunity} turns used a BLOCK/BLITZ when >=1 standing enemy existed "
      f"({100*off_used/off_opportunity:.1f}%)" if off_opportunity else "no offense opportunities found")
print(f"defense: {def_used}/{def_opportunity} turns used a BLOCK/BLITZ when >=1 standing enemy existed "
      f"({100*def_used/def_opportunity:.1f}%)" if def_opportunity else "no defense opportunities found")
print("(NOTE: this counts ALL Block-family macros, not just BLITZ specifically --")
print(" this log format doesn't distinguish blitz from plain Block. Directional")
print(" signal only, not a clean isolation of the offense-floor mechanism.)")
print()
print("=== Old priority 6: target freeness ===")
total_blitzish = blitz_free_target + blitz_covered_target
print(f"targeted a 'free' (0 friendly-tz) standing opponent: {blitz_free_target}/{total_blitzish} "
      f"({100*blitz_free_target/total_blitzish:.1f}%)" if total_blitzish else "n/a")
print(f"targeted an already-covered opponent: {blitz_covered_target}/{total_blitzish}")
print(f"turns where ALL standing enemies were already covered (no free target existed) "
      f"and a block/blitz still happened: {len(no_free_target_cases)}")
