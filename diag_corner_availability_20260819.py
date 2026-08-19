#!/usr/bin/env python3
"""JSOU ROHY VŮBEC K MÁNÍ? (uživatel 19.08.: „nemá se klec posunout jinak,
aby nic nebránilo žádnému rohu?")

Rozklad P34 se ptal „proč tohle tělo nešlo do rohu" -- což je podmíněné tím,
KDE STOJÍ NOSIČ. Tohle se ptá o patro výš: kolik rohů je nedostupných proto,
že je nosič postavil na pole, které nejde obsadit (lajna, soupeř), a ne proto,
že se tělo špatně rozhodlo.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def run(paths, race="dwarf"):
    st = Counter()
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
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if S.get("touchdown") or E["half"] != S["half"]:
                continue
            us = R.players(E, ours)
            them = R.players(E, theirs)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            st["kol"] += 1
            cx, cy = car["x"], car["y"]
            occ_us = {(p["x"], p["y"]) for p in us if p["id"] != car["id"]}
            occ_them = {(p["x"], p["y"]) for p in them}

            filled = 0
            for dx, dy in DIAG:
                sq = (cx + dx, cy + dy)
                if not (0 <= sq[0] <= 25 and 0 <= sq[1] <= 14):
                    st["ROH MIMO HŘIŠTĚ (lajna/endzona)"] += 1
                elif sq in occ_them:
                    st["ROH OBSAZEN SOUPEŘEM"] += 1
                elif sq in occ_us:
                    st["roh obsazen NAŠÍM tělem (splněno)"] += 1
                    filled += 1
                else:
                    st["ROH PRÁZDNÝ — pole je k mání"] += 1
            st[f"rohů obsazeno={filled}"] += 1

            # vzdálenost nosiče od lajny: y=0 a y=14 jsou postranní čáry
            edge = min(cy, 14 - cy)
            st[f"nosič {edge} od lajny — kol"] += 1
            if filled == 4:
                st[f"nosič {edge} od lajny — 4 rohy"] += 1
            if edge == 0:
                st["nosič STOJÍ NA LAJNĚ"] += 1
    return st


def main():
    st = run(sorted(glob.glob(sys.argv[1])))
    n = st["kol"]
    tot = 4 * n
    print("%d našich kol s nosičem  ⇒  %d rohových polí\n" % (n, tot))
    for k in ["roh obsazen NAŠÍM tělem (splněno)", "ROH PRÁZDNÝ — pole je k mání",
              "ROH OBSAZEN SOUPEŘEM", "ROH MIMO HŘIŠTĚ (lajna/endzona)"]:
        print("  %-42s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / tot))
    miss = tot - st["roh obsazen NAŠÍM tělem (splněno)"]
    print("\n  z %d CHYBĚJÍCÍCH rohů:" % miss)
    for k in ["ROH PRÁZDNÝ — pole je k mání", "ROH OBSAZEN SOUPEŘEM",
              "ROH MIMO HŘIŠTĚ (lajna/endzona)"]:
        print("     %-39s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / miss))
    print("\n  nosič stojí přímo na postranní čáře: %d kol (%.1f %%)"
          % (st["nosič STOJÍ NA LAJNĚ"], 100.0 * st["nosič STOJÍ NA LAJNĚ"] / n))
    print("\n  podíl kol se 4 rohy podle vzdálenosti nosiče od lajny:")
    for e in range(8):
        c = st[f"nosič {e} od lajny — kol"]
        if c:
            print("     %d od lajny: %6d kol, 4 rohy v %5.1f %%"
                  % (e, c, 100.0 * st[f"nosič {e} od lajny — 4 rohy"] / c))


if __name__ == "__main__":
    main()
