#!/usr/bin/env python3
"""LAJNA PODLE SOUPEŘE (uživatel 19.08.: „proti skavenům se tomu má vyhnout striktně")

Doktrína 15.5 je dnes rasově slepá. Otázka: liší se cena za nosiče u lajny
podle toho, kdo stojí proti nám? Měří se ZTRÁTA MÍČE V SOUPEŘOVĚ NÁSLEDUJÍCÍM
KOLE podle vzdálenosti našeho nosiče od postranní čáry na konci NAŠEHO kola.
"""
import sys, glob
from collections import Counter, defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

RACE_MARK = {"Gutter Runner": "skaven", "Storm Vermin": "skaven",
             "Black Orc": "orc", "Blitzer": "?", "Catcher": "?",
             "Wardancer": "wood-elf", "Ogre": "human"}


def opp_race(turn, side):
    # jména nesou i dovednosti ("Gutter Runner +Sure Feet"), takže substring
    names = " ".join(p["name"] for p in turn[f"{side}_players"])
    if "Gutter Runner" in names: return "skaven"
    if "Black Orc" in names: return "orc"
    if "Wardancer" in names or "Treeman" in names: return "wood-elf"
    if "Ogre" in names: return "human"
    return "?"


def run(paths, race="dwarf"):
    st = defaultdict(Counter)
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == race), None)
        if ours is None:
            continue
        theirs = "away" if ours == "home" else "home"
        orace = opp_race(r["turn_logs"][0], theirs)
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 2 >= len(logs):
                continue
            E, F = logs[i + 1], logs[i + 2]
            if S.get("touchdown") or E["half"] != S["half"] or F["half"] != E["half"]:
                continue
            us = R.players(E, ours)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            edge = min(car["y"], 14 - car["y"])
            band = "0-1 od lajny" if edge <= 1 else ("2-3" if edge <= 3 else "4+")
            still = any(p["has_ball"] for p in R.players(F, ours))
            st[orace][band + " | kol"] += 1
            if not still:
                st[orace][band + " | ZTRÁTA"] += 1
    return st


def main():
    st = run(sorted(glob.glob(sys.argv[1])))
    print("ztráta míče v soupeřově NÁSLEDUJÍCÍM kole podle vzdálenosti našeho")
    print("nosiče od postranní čáry na konci NAŠEHO kola\n")
    print("%-10s %-14s %8s %8s %8s" % ("soupeř", "pásmo", "kol", "ztrát", "podíl"))
    for orace in sorted(st):
        base = None
        for band in ["0-1 od lajny", "2-3", "4+"]:
            n = st[orace][band + " | kol"]
            l = st[orace][band + " | ZTRÁTA"]
            if not n:
                continue
            pct = 100.0 * l / n
            if band == "4+":
                base = pct
            print("%-10s %-14s %8d %8d %7.1f %%" % (orace, band, n, l, pct))
        if base is not None:
            n0 = st[orace]["0-1 od lajny | kol"]
            if n0:
                p0 = 100.0 * st[orace]["0-1 od lajny | ZTRÁTA"] / n0
                print("%-10s %-14s %s" % ("", "", "  ⇒ u lajny %+.1f pp proti středu" % (p0 - base)))
        print()


if __name__ == "__main__":
    main()
