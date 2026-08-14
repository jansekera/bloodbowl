#!/usr/bin/env python3
"""OQ1 krok 2: per-drive per-turn hodnoty kontrol (definice importované).

Stejné dělení drivů a stejné veličiny jako diag_drive_predictors_20260813.py,
ale (a) bez filtru >=7 kol (filtruje se až v analýze), (b) per-turn sekvence
místo průměrů — kvůli testu 'tempo v prvních 3 kolech' (selekce/tautologie)
a kvůli korelacím kontrol.

Výstup JSONL, řádek na drive:
  {game, opp, receiving, scored, n_our_turns, first_hold_dist, turns_left_at_hold,
   turns:[{t, dx, k9a_need, k9a_got, blocks, filled, clean, dirty, reach0, fb2, idle}]}
dx/k9a jen v kolech, kdy TÝŽ nosič drží míč na začátku i konci kola (jako K9a).
"""
import sys, glob, json, math
sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import (load, players, threatens, adj, STANDING)
from diag_exposure_scan_20260812 import Board, predictors
from diag_drive_failure_20260811 import build_id_map, receiving_side, is_bug_drive

DATA = sys.argv[1]
OUT = sys.argv[2]
LIMIT = int(sys.argv[3]) if len(sys.argv) > 3 else None

paths = sorted(glob.glob(DATA + "/g*.json.gz"))
if LIMIT:
    paths = paths[:LIMIT]

with open(OUT, "w") as out:
    for path in paths:
        r = load(path)
        nm = " ".join(p["name"] for p in r["turn_logs"][0]["home_players"][:5])
        ours = "home" if ("Longbeard" in nm or "Troll Slayer" in nm) else "away"
        opp = r["away_race"] if ours == "home" else r["home_race"]
        fwd = 1 if ours == "home" else -1
        endzone = 25 if fwd == 1 else 0
        id_map = build_id_map(r)
        logs = r["turn_logs"]
        starts = [0]
        for i in range(1, len(logs)):
            if logs[i]["half"] != logs[i - 1]["half"] or logs[i - 1].get("touchdown"):
                starts.append(i)
        starts.append(len(logs))
        for si in range(len(starts) - 1):
            a, b = starts[si], starts[si + 1]
            seg = logs[a:b]
            if is_bug_drive(seg):
                continue
            ours_turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
            if not ours_turns:
                continue
            anom = []
            recv = receiving_side(seg, id_map, anom, "") == ours
            scored = any(logs[i].get("touchdown") and logs[i]["active_team"] == ours
                         for i in range(a, b))
            turns = []
            first_hold = None
            for ti, i in enumerate(ours_turns):
                if i + 1 >= len(logs) or logs[i + 1]["half"] != logs[i]["half"]:
                    continue
                S, E = logs[i], logs[i + 1]
                us = players(E, ours)
                them = players(E, "away" if ours == "home" else "home")
                car = next((p for p in us if p["has_ball"]), None)
                blocks = sum(1 for e in S["events"] if e["type"] == "BLOCK"
                             and any(p["id"] == e["player_id"] for p in us))
                row = {"t": ti, "blocks": blocks}
                # idle (K31) — stejná definice jako diag_rules_checks
                diag = [(car["x"] + dx, car["y"] + dy)
                        for dx in (-1, 1) for dy in (-1, 1)] if car else []
                moved = {e["player_id"] for e in S["events"]}
                idle = 0
                for p in us:
                    if p["state"] != STANDING or p["id"] in moved:
                        continue
                    if p["has_ball"] or (p["x"], p["y"]) in diag:
                        continue
                    if any(adj((p["x"], p["y"]), (o["x"], o["y"])) for o in them):
                        continue
                    idle += 1
                row["idle"] = idle
                P = predictors(Board(E, ours))
                row["fb2"] = P["FB2"]
                row["reach0"] = P.get("REACH0")
                if car is not None:
                    occ = {(p["x"], p["y"]): p for p in us}
                    filled = [d for d in diag if d in occ]
                    threat = [(p["x"], p["y"]) for p in them if threatens(p)]
                    dirty = [d for d in filled if any(adj(d, t) for t in threat)]
                    row["filled"] = len(filled)
                    row["clean"] = len(filled) - len(dirty)
                    row["dirty"] = len(dirty)
                    if first_hold is None:
                        first_hold = {"ti": ti,
                                      "dist": abs(endzone - car["x"]),
                                      "turns_left": 9 - S["turn"] - 1}
                carS = next((p for p in players(S, ours) if p["has_ball"]), None)
                turns_left = 9 - S["turn"]
                if turns_left > 0 and carS and car and carS["id"] == car["id"]:
                    row["k9a_need"] = math.ceil(abs(endzone - carS["x"]) / turns_left)
                    row["k9a_got"] = (car["x"] - carS["x"]) * fwd
                turns.append(row)
            out.write(json.dumps({
                "game": path.split("/")[-1].split(".")[0], "opp": opp,
                "receiving": recv, "scored": bool(scored),
                "n_our_turns": len(ours_turns),
                "first_hold": first_hold, "turns": turns}) + "\n")
