#!/usr/bin/env python3
"""ZAVÁZÍ SOUPEŘ TAM, KAM NEUMÍME UHNOUT? (20.08.2026)

Uživatel 20.08.: *„máme v enginu cíl — zavazet — a proti tomu klec, co se
neumí hnout do strany."* A odhad: *„ztrácíme hodně, a hbití elfové ještě víc."*

Obě strany sdílí `expandReposition`, takže soupeř zavazí podle TÉHOŽ kódu,
který používáme my — a jeho screen stojí na natvrdo daných `y ∈ {3,5,7,9,11}`.

Měříme na úrovni kola s míčem: je pole rovně vpřed obsazené/pokryté, zatímco
o řadu výš nebo níž je volno? To je přesně situace, kterou uhnutí řeší
a přímka ne.
"""
import sys, glob
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from collections import Counter

STANDING = 0


def scan(paths, race):
    st = Counter()
    for path in paths:
        r = R.load(path)
        if race == "dwarf":
            side = "home" if r.get("home_race") == "dwarf" else "away"
        else:
            side = "away" if r.get("home_race") == "dwarf" else "home"
        opp = "away" if side == "home" else "home"
        fwd = 1 if side == "home" else -1
        for S in r["turn_logs"]:
            if S["active_team"] != side:
                continue
            car = next((p for p in R.players(S, side) if p["has_ball"]), None)
            if car is None or car["state"] != STANDING:
                continue
            occ = {(p["x"], p["y"]) for p in R.players(S, side)}
            them = [p for p in R.players(S, opp) if p["state"] == STANDING]
            tz = {(p["x"] + dx, p["y"] + dy) for p in them
                  for dx in (-1, 0, 1) for dy in (-1, 0, 1)}
            them_at = {(p["x"], p["y"]) for p in them}

            def blocked(sq):
                return sq in occ or sq in them_at or sq in tz

            st["kol s nosičem"] += 1
            straight = (car["x"] + fwd, car["y"])
            side_up = (car["x"] + fwd, car["y"] - 1)
            side_dn = (car["x"] + fwd, car["y"] + 1)
            if not straight[0] in range(0, 26):
                continue
            sb = blocked(straight)
            free_side = [s for s in (side_up, side_dn)
                         if 0 <= s[1] <= 14 and not blocked(s)]
            if sb:
                st["  rovně VPŘED zablokováno"] += 1
                if free_side:
                    st["    …ale DO BOKU volno ⇒ uhnutí by pomohlo"] += 1
                else:
                    st["    …a do boku taky ne"] += 1
            else:
                st["  rovně vpřed volno"] += 1
    return st


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    for race in ("dwarf", "soupeř"):
        st = scan(paths, "dwarf" if race == "dwarf" else "opp")
        n = st["kol s nosičem"]
        b = st["  rovně VPŘED zablokováno"]
        print(f"\n=== {race} ===  kol s nosičem: {n}")
        for k in ("  rovně vpřed volno", "  rovně VPŘED zablokováno"):
            print(f"  {k:44s} {st[k]:6d}  {100.0*st[k]/n:5.1f} %")
        if b:
            for k in ("    …ale DO BOKU volno ⇒ uhnutí by pomohlo",
                      "    …a do boku taky ne"):
                print(f"  {k:44s} {st[k]:6d}  {100.0*st[k]/b:5.1f} % ze zablokovaných"
                      f"  ({100.0*st[k]/n:4.1f} % kol)")


if __name__ == "__main__":
    main()
