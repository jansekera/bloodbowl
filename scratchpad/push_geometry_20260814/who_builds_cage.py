#!/usr/bin/env python3
"""Kdo u které rasy nese míč a kdo jí staví klec (14.08.2026).

Otázka uživatele. Motivace: ork nám dovolí 2kostkový blitz na svého nosiče jen
v 7,5 % kol (proti 54,4 % u skavena) — je to tím, KDO mu stojí v rozích?

Rohy = čtyři diagonály nosiče, stejná definice jako K29
(`diag_rules_checks_20260812.py`). Počítá se jen v kolech, kdy daná strana
míč DRŽÍ.
"""
import glob, gzip, json, sys
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import load, players, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"

carrier = defaultdict(Counter)
corner = defaultdict(Counter)
filled = defaultdict(list)
st_at_corner = defaultdict(Counter)
n_games = 0

for path in sorted(glob.glob(DATA + "/*.json.gz")):
    g = json.load(gzip.open(path, "rt"))
    if "dwarf" not in (g["home_race"], g["away_race"]):
        continue
    n_games += 1
    for side in ("home", "away"):
        race = g[side + "_race"]
        for tl in g["turn_logs"]:
            us = players(tl, side)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            carrier[race][car.get("name", "?")] += 1
            diag = [(car["x"] + dx, car["y"] + dy)
                    for dx in (-1, 1) for dy in (-1, 1)]
            occ = {(p["x"], p["y"]): p for p in us if p["state"] == STANDING}
            k = 0
            for d in diag:
                p = occ.get(d)
                if p is None:
                    continue
                k += 1
                corner[race][p.get("name", "?")] += 1
                st_at_corner[race][p["st"] if "st" in p else
                                   p.get("strength", "?")] += 1
            filled[race].append(k)

print("her: %d\n" % n_games)
for race in sorted(carrier):
    tot_c = sum(carrier[race].values())
    tot_k = sum(corner[race].values())
    f = filled[race]
    print("=== %s ===  kol s míčem: %d | obsazených rohů průměrně: %.2f / 4"
          % (race.upper(), tot_c, sum(f) / max(len(f), 1)))
    print("   NOSIČ:")
    for nm, v in carrier[race].most_common(5):
        print("      %-30s %5.1f %%" % (nm, 100.0 * v / max(tot_c, 1)))
    print("   ROHY KLECE:")
    for nm, v in corner[race].most_common(6):
        print("      %-30s %5.1f %%" % (nm, 100.0 * v / max(tot_k, 1)))
    print()
