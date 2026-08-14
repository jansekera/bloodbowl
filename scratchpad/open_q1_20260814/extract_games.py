#!/usr/bin/env python3
"""OQ1 krok 1: per-game výsledky + per-drive kategorie (reuse diag_drive_failure).

Výstup: JSONL, jeden řádek na hru:
  {game, opp, our, their, drives:[{cat,subcat,half,start_turn,n_our_turns,
   kickoff_dist, first_hold_turn, first_hold_dist, turns_left, avg_tempo,
   loss_cause, loss_dist, opp_td_in_drive, short_drive}],
   def_drives:[{outcome}]}
Definice drivů a kategorií jsou importované, ne opsané.
"""
import sys, glob, json, gzip
sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_drive_failure_20260811 import (
    load_game, build_id_map, split_drives, holder_side, td_scorer_side,
    receiving_side, is_bug_drive, analyze_receiving_drive)

DATA = sys.argv[1]
OUT = sys.argv[2]

def dwarf_side_of(game):
    tl0 = game["turn_logs"][0]
    for s in ("home", "away"):
        nm = " ".join(p["name"] for p in tl0[s + "_players"][:5])
        if "Longbeard" in nm or "Troll Slayer" in nm:
            return s
    return None

n_anom = 0
with open(OUT, "w") as out:
    for path in sorted(glob.glob(DATA + "/g*.json.gz")):
        g = load_game(path)
        ds = dwarf_side_of(g)
        if ds is None:
            continue
        opp = g["away_race"] if ds == "home" else g["home_race"]
        our = g["home_score"] if ds == "home" else g["away_score"]
        their = g["away_score"] if ds == "home" else g["home_score"]
        id_map = build_id_map(g)
        drives = split_drives(g["turn_logs"])
        anomalies = []
        recv, dfn = [], []
        for di, d in enumerate(drives):
            if is_bug_drive(d):
                continue
            ctx = "%s:%d" % (path, di)
            nxt = drives[di + 1][0] if di + 1 < len(drives) else None
            if receiving_side(d, id_map, anomalies, ctx) == ds:
                r = analyze_receiving_drive(d, nxt, id_map, ds, ctx, anomalies)
                # soupeřův TD v témže drivu (pro výpočet švihu marže u C)
                scorers = [td_scorer_side(tl, id_map) for tl in d if tl["touchdown"]]
                rec = {k: r.get(k) for k in (
                    "cat", "subcat", "half", "start_turn", "n_our_turns",
                    "kickoff_dist", "first_hold_turn", "first_hold_dist",
                    "turns_left", "avg_tempo", "loss_cause", "loss_dist",
                    "short_drive", "end_dist")}
                rec["opp_td_in_drive"] = any(s is not None and s != ds for s in scorers)
                recv.append(rec)
            else:
                our_td = any(td_scorer_side(tl, id_map) == ds for tl in d if tl["touchdown"])
                opp_td = any(td_scorer_side(tl, id_map) not in (None, ds)
                             for tl in d if tl["touchdown"])
                ever = any(holder_side(tl, id_map) == ds for tl in d)
                dfn.append({"our_td": our_td, "opp_td": opp_td, "we_held": ever,
                            "n_turns": len(d)})
        n_anom += len(anomalies)
        out.write(json.dumps({
            "game": path.split("/")[-1].split(".")[0], "opp": opp,
            "our": our, "their": their, "drives": recv, "def_drives": dfn}) + "\n")
print("anomálie (ignorováno, hlášeno už v drives.txt):", n_anom)
