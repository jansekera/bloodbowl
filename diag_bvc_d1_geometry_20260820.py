#!/usr/bin/env python3
"""Geometrie D1 (column defence, grumbbl 20.08.): screenující tělo stojí
1 POLE od nejbližšího soupeře (Čebyšev 2: nutí blitz, ne blok), a ZA ním
do hloubky 1-2 pole (|dy|<=1) stojí další naše tělo.

Otázka: vyskytuje se to v korpusu aspoň náhodou?  Jmenovatel = obranná kola
se stojícím soupeřovým nosičem (týž jako diag_basing_vs_columns).
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def main():
    paths = []
    for a in sys.argv[1:]:
        paths += sorted(glob.glob(a))
    st = Counter()
    per_turn_gap1 = Counter()
    per_turn_backed = Counter()
    for path in paths:
        r = R.load(path)
        if r["home_race"] == "dwarf":
            ours, theirs = "home", "away"
        elif r["away_race"] == "dwarf":
            ours, theirs = "away", "home"
        else:
            continue
        oppfwd = -1 if ours == "home" else 1
        for S in r["turn_logs"]:
            if S["active_team"] != theirs:
                continue
            their_all = R.players(S, theirs)
            car = next((p for p in their_all if p["has_ball"]), None)
            if car is None or car["state"] != STANDING:
                continue
            st["obranná kola (jmenovatel)"] += 1
            our_stand = [p for p in R.players(S, ours) if p["state"] == STANDING]
            their_stand = [p for p in their_all if p["state"] == STANDING]
            if not their_stand:
                continue
            gap1 = []
            for q in our_stand:
                d = min(max(abs(q["x"] - t["x"]), abs(q["y"] - t["y"]))
                        for t in their_stand)
                if d == 2:
                    gap1.append(q)
            st["těl v odstupu PŘESNĚ 1 pole (celkem)"] += len(gap1)
            backed = [q for q in gap1
                      if any(o["id"] != q["id"]
                             and abs(o["y"] - q["y"]) <= 1
                             and 1 <= (o["x"] - q["x"]) * oppfwd <= 2
                             for o in our_stand)]
            st["z nich s HLOUBKOU za sebou (celkem)"] += len(backed)
            per_turn_gap1[min(len(gap1), 4)] += 1
            per_turn_backed[min(len(backed), 3)] += 1
            if len(gap1) >= 2:
                st["kol s ≥2 těly v odstupu 1"] += 1
            if len(backed) >= 2:
                st["kol s ≥2 KRYTÝMI těly (fragment D1)"] += 1

    N = st["obranná kola (jmenovatel)"]
    print("jmenovatel: %d obranných kol" % N)
    for k in sorted(st):
        if k != "obranná kola (jmenovatel)":
            print("  %-46s %7d" % (k, st[k]))
    print("  kol s ≥2 těly v odstupu 1:  %.1f %% z %d" %
          (100.0 * st["kol s ≥2 těly v odstupu 1"] / N, N))
    print("  kol s ≥2 krytými (fragment D1): %.1f %% z %d" %
          (100.0 * st["kol s ≥2 KRYTÝMI těly (fragment D1)"] / N, N))
    print("  histogram těl v odstupu 1 na kolo (0..4+):",
          [per_turn_gap1[i] for i in range(5)])
    print("  histogram krytých na kolo (0..3+):       ",
          [per_turn_backed[i] for i in range(4)])


if __name__ == "__main__":
    main()
