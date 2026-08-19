#!/usr/bin/env python3
"""ZPĚTNÝ ROZVRH Z MECHANICKÝCH STROPŮ (uživatel 19.08.)

Kapacita fází se NEČTE Z HISTORIE -- odvozuje se z pravidel a rosteru:
    SÓLO   = MA nosiče
    KLEC   = min(MA čtyř rohů, MA nosiče)     # klec jede tak rychle, jak stačí roh
    VÝBĚH  = MA nosiče
⇒ nekazí to, že dnešní korpus pravidlo klece skoro nehraje.

Rozvrh se skládá od 8. kola zpět:
    M(8) = strop(VÝBĚH),  M(t) = strop(fáze v kole t) + M(t+1)
a otázka zní: **je vzdálenost k endzoně vůbec někdy větší než M(t)?**
Tedy: prohráváme drivy ROZVRHEM (nebylo to stihnutelné), nebo VYUŽITÍM
(stihnutelné bylo, ale nejeli jsme)?
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = Counter()
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == "dwarf"), None)
        if ours is None:
            continue
        theirs = "away" if ours == "home" else "home"
        fwd = 1 if ours == "home" else -1
        endzone = 25 if fwd == 1 else 0
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
            n_left = 9 - S["turn"]
            if n_left <= 0:
                continue
            dist = abs(endzone - car["x"])
            dx = (carE["x"] - car["x"]) * fwd
            occ = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
            corners = [occ[(car["x"] + a, car["y"] + b)]
                       for a, b in DIAG if (car["x"] + a, car["y"] + b) in occ]
            cage_cap = min([c["ma"] for c in corners] + [car["ma"]]) if corners else car["ma"]

            # Zpětný rozvrh: poslední kolo je VÝBĚH (strop = MA nosiče), kola
            # před ním jedou v aktuální fázi. Konzervativně: všechna kola kromě
            # posledního se počítají stropem KLECE, i když část z nich bude sólo
            # -- sólo má strop vyšší, takže tohle je DOLNÍ odhad kapacity.
            M = car["ma"] + (n_left - 1) * cage_cap
            quota = dist * cage_cap / M if M > 0 else dist

            st["kol"] += 1
            st["součet M"] += M
            st["součet vzdálenost"] += dist
            if dist > M:
                st["ROZVRH NESPLNITELNÝ (mechanicky)"] += 1
            else:
                st["rozvrh splnitelný"] += 1
                if dx >= quota:
                    st["kvótu splnil"] += 1
                if dx == 0:
                    st["kvóta splnitelná, a nehnuli jsme se VŮBEC"] += 1
    n = st["kol"]
    print("kol s nosičem a zbývajícím kolem: %d\n" % n)
    print("  ⌀ kapacita do konce půle (M)      %.1f polí" % (st["součet M"] / n))
    print("  ⌀ zbývající vzdálenost            %.1f polí\n" % (st["součet vzdálenost"] / n))
    for k in ["ROZVRH NESPLNITELNÝ (mechanicky)", "rozvrh splnitelný"]:
        print("  %-46s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / n))
    ok = st["rozvrh splnitelný"]
    print("\n  ze splnitelných kol (%d):" % ok)
    for k in ["kvótu splnil", "kvóta splnitelná, a nehnuli jsme se VŮBEC"]:
        print("  %-46s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / ok))


if __name__ == "__main__":
    main()
