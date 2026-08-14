"""Item 11 (PICKUP backup gap) mining pass, 2026-07-29.

Reuses the existing 24-game 21.07 corpus (diag_replay_mine_20260721_data/),
same source as diag_safefirst_scan_20260722.py.

Two questions, per the user's request:
1. For every PICKUP attempt, was there a friendly backup player already
   adjacent to the pickup square at the moment of the attempt (item11's
   core question -- does the engine ever bring support before a risky
   pickup)?
2. How many enemy tackle zones was the ball/picker in at the moment of
   the attempt (i.e. how many +1 penalty points applied to the pickup
   target number, via calculatePickupTarget's `target += countTacklezones(...)`
   term) -- and does that correlate with failure, as expected?

Position tracking: turn-start snapshot (home_players/away_players) is only
accurate at the START of the turn -- events happen mid-turn and change
positions (a backup player might arrive AFTER turn-start but BEFORE the
pickup). This script replays every event in order, applying
positions[player_id] = (to_x, to_y) after each event, so tackle-zone and
backup checks at each PICKUP event use the ACTUAL position at that moment,
not the turn-start approximation safefirst_scan used.
"""
import gzip
import json
import glob

files = sorted(glob.glob("diag_replay_mine_20260721_data/g*.json.gz"))


def adjacent(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1])) == 1


def same_square(a, b):
    return a == b


total_pickups = 0
failed = 0
tz_hist = {}
backup_present = 0
backup_absent = 0
backup_absent_failed = 0
backup_absent_tz_gt0 = 0
examples_no_backup_failed = []
examples_no_backup_tz = []

for fp in files:
    rec = json.load(gzip.open(fp, "rt"))
    for t in rec["turn_logs"]:
        team_of = {}
        positions = {}
        for p in t["home_players"]:
            team_of[p["id"]] = "home"
            positions[p["id"]] = (p["x"], p["y"], p["state"])
        for p in t["away_players"]:
            team_of[p["id"]] = "away"
            positions[p["id"]] = (p["x"], p["y"], p["state"])

        for ev in t.get("events", []):
            if ev["type"] == "PICKUP":
                total_pickups += 1
                pid = ev["player_id"]
                pteam = team_of.get(pid)
                pos = (ev["from_x"], ev["from_y"])

                # Tackle zones at the pickup square (opponents only, standing).
                tz = 0
                for oid, (ox, oy, ost) in positions.items():
                    if oid == pid or team_of.get(oid) == pteam:
                        continue
                    if ost != 0:  # not STANDING
                        continue
                    if adjacent((ox, oy), pos):
                        tz += 1
                tz_hist[tz] = tz_hist.get(tz, 0) + 1

                # Backup: a friendly teammate (not the picker) already
                # on or adjacent to the pickup square at this moment.
                has_backup = False
                for oid, (ox, oy, ost) in positions.items():
                    if oid == pid or team_of.get(oid) != pteam:
                        continue
                    if ost != 0:
                        continue
                    if same_square((ox, oy), pos) or adjacent((ox, oy), pos):
                        has_backup = True
                        break

                if has_backup:
                    backup_present += 1
                else:
                    backup_absent += 1
                    if not ev["success"]:
                        backup_absent_failed += 1
                        if len(examples_no_backup_failed) < 8:
                            examples_no_backup_failed.append({
                                "game": fp, "half": t["half"], "turn": t["turn"],
                                "team": pteam, "player": pid, "pos": pos, "tz": tz,
                                "roll": ev["roll"],
                            })
                    if tz > 0:
                        backup_absent_tz_gt0 += 1
                        if len(examples_no_backup_tz) < 8:
                            examples_no_backup_tz.append({
                                "game": fp, "half": t["half"], "turn": t["turn"],
                                "team": pteam, "player": pid, "pos": pos, "tz": tz,
                                "success": ev["success"], "roll": ev["roll"],
                            })

                if not ev["success"]:
                    failed += 1

            # Advance position tracking for ANY event that moves a player.
            if ev.get("player_id", -1) > 0:
                ev_to = (ev["to_x"], ev["to_y"])
                if ev_to != (0, 0) or ev["type"] in ("MOVE", "DODGE", "GFI", "PUSH", "PICKUP", "BLOCK"):
                    prev = positions.get(ev["player_id"])
                    if prev is not None:
                        positions[ev["player_id"]] = (ev_to[0], ev_to[1], prev[2])

print(f"games scanned: {len(files)}")
print(f"total PICKUP events: {total_pickups}")
print(f"  failed: {failed} ({100*failed/total_pickups:.1f}%)" if total_pickups else "")
print()
print("tackle-zone count at moment of pickup attempt (histogram):")
for k in sorted(tz_hist):
    print(f"  {k} TZ: {tz_hist[k]} ({100*tz_hist[k]/total_pickups:.1f}%)")
print()
print(f"backup (friendly adjacent/on-square) present: {backup_present} "
      f"({100*backup_present/total_pickups:.1f}%)")
print(f"backup absent: {backup_absent} ({100*backup_absent/total_pickups:.1f}%)")
print(f"  of those, pickup FAILED: {backup_absent_failed} "
      f"({100*backup_absent_failed/backup_absent:.1f}% of no-backup attempts)" if backup_absent else "")
print(f"  of those, attempted WHILE IN >=1 enemy TZ (no backup AND under pressure): "
      f"{backup_absent_tz_gt0}")
print()
print("examples: no backup present AND pickup failed")
for e in examples_no_backup_failed:
    print(" ", e)
print()
print("examples: no backup present AND >=1 tackle zone on the ball")
for e in examples_no_backup_tz:
    print(" ", e)
