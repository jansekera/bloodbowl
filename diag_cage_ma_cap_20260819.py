#!/usr/bin/env python3
"""STROP KLECE Z NEJPOMALEJŠÍHO ROHU (uživatel 19.08.)

Mechanika: popojede-li nosič o Δx, musí o Δx popojet i každý roh, jinak klec
zůstane vzadu. ⇒ **strop postupu klece = MA nejpomalejšího rohového těla**
(shora omezené i MA nosiče). Je to kapacita odvozená z PRAVIDEL, ne z historie
-- proto ji nekazí to, že dnešní korpus pravidlo klece skoro nehraje.

Otázka, která rozhoduje, jestli je to k něčemu: **je ta mez SVAZUJÍCÍ?**
Když jedeme hluboko pod ní, klec nebrzdí nejpomalejší roh, ale něco jiného
(odpor, tackle zóny, volba) -- a strop je pak popis, ne páka.
"""
import sys, glob
from collections import Counter, defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = Counter()
    bycap = defaultdict(list)
    corner_names = Counter()
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == "dwarf"), None)
        if ours is None:
            continue
        fwd = 1 if ours == "home" else -1
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if S.get("touchdown") or E["half"] != S["half"]:
                continue
            us = R.players(S, ours)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            carE = next((p for p in R.players(E, ours)
                         if p["has_ball"] and p["id"] == car["id"]), None)
            if carE is None:
                continue
            occ = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
            corners = [occ[(car["x"] + a, car["y"] + b)]
                       for a, b in DIAG if (car["x"] + a, car["y"] + b) in occ]
            if len(corners) < 2:
                continue
            dx = (carE["x"] - car["x"]) * fwd
            cap = min([c["ma"] for c in corners] + [car["ma"]])
            for c in corners:
                corner_names[c["name"].split(" +")[0]] += 1
            st["kol s klecí (>=2 rohy)"] += 1
            bycap[cap].append(dx)
            if dx >= cap:
                st["Δx DOSÁHLO stropu (mez je svazující)"] += 1
            if dx >= cap - 1:
                st["Δx do 1 pole od stropu"] += 1
            if dx == 0:
                st["Δx = 0 (klec stála)"] += 1
            st["součet stropů"] += cap
            st["součet Δx"] += dx
    n = st["kol s klecí (>=2 rohy)"]
    print("kol s postavenou klecí (>=2 rohy): %d\n" % n)
    print("  ⌀ strop z nejpomalejšího rohu   %.2f pole/kolo" % (st["součet stropů"] / n))
    print("  ⌀ skutečné Δx                   %.2f pole/kolo" % (st["součet Δx"] / n))
    print("  ⇒ využití stropu                %.0f %%\n"
          % (100.0 * st["součet Δx"] / st["součet stropů"]))
    for k in ["Δx DOSÁHLO stropu (mez je svazující)", "Δx do 1 pole od stropu",
              "Δx = 0 (klec stála)"]:
        print("  %-42s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / n))
    print("\n  podle stropu (MA nejpomalejšího rohu):")
    print("  %-6s %8s %9s %9s %9s" % ("strop", "kol", "⌀ Δx", "využití", "Δx=0"))
    for cap in sorted(bycap):
        v = bycap[cap]
        if len(v) < 50:
            continue
        print("  %-6d %8d %9.2f %8.0f %% %8.0f %%"
              % (cap, len(v), sum(v) / len(v), 100.0 * sum(v) / (cap * len(v)),
                 100.0 * sum(1 for x in v if x == 0) / len(v)))
    print("\n  kdo stojí v rozích:")
    tot = sum(corner_names.values())
    for k, v in corner_names.most_common():
        print("   %-18s %7d  %5.1f %%" % (k, v, 100.0 * v / tot))


if __name__ == "__main__":
    main()
