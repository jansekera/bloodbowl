#!/usr/bin/env python3
"""ČÍM SE ŘÍDÍ CENA LAJNY — MA, nebo SÍLA? (uživatel 19.08.)

Zobecnění U1 na úroveň KOLA, ne rasy. Se čtyřmi rasami se dá proložit cokoli
(n=4); tady je jmenovatel ~20 000 kol a ptáme se, čím soupeř KOLEM NOSIČE
vysvětluje ztrátu míče — svým MA, nebo svou silou.

Pro každé naše kolo s nosičem na konci: vzdálenost od postranní čáry, a mezi
soupeři, kteří na nosiče DOSÁHNOU (Chebyshev <= MA+2, tedy i přes GFI),
jejich max MA a max ST. Výsledek = ztratili jsme míč v jejich následujícím kole?
"""
import sys, glob
from collections import defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    cell = defaultdict(lambda: [0, 0])       # (edge_band, key) -> [kol, ztrát]
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
            them = [p for p in R.players(E, theirs) if p["state"] == STANDING]
            reach = [p for p in them
                     if max(abs(p["x"] - car["x"]), abs(p["y"] - car["y"])) <= p["ma"] + 2]
            if not reach:
                continue
            edge = min(car["y"], 14 - car["y"])
            band = "u lajny (0-2)" if edge <= 2 else "střed (3+)"
            maxma = max(p["ma"] for p in reach)
            maxst = max(p["st"] for p in reach)
            lost = not any(p["has_ball"] for p in R.players(F, ours))
            for key in (("MA", min(maxma, 9)), ("ST", min(maxst, 5))):
                c = cell[(band, key)]
                c[0] += 1
                c[1] += lost
    print("ztráta míče v soupeřově následujícím kole; jmenovatel = naše kola,")
    print("kde na nosiče dosáhne aspoň jeden stojící soupeř\n")
    for dim, lo, hi in (("MA", 5, 9), ("ST", 2, 5)):
        print("=== podle NEJVYŠŠÍHO %s mezi soupeři, kteří na nosiče dosáhnou ===" % dim)
        print("%-6s %10s %10s %10s %10s %9s"
              % (dim, "kol lajna", "ztrát", "kol střed", "ztrát", "násobek"))
        for v in range(lo, hi + 1):
            a = cell[("u lajny (0-2)", (dim, v))]
            b = cell[("střed (3+)", (dim, v))]
            if a[0] < 100 or b[0] < 100:
                continue
            pa, pb = 100.0 * a[1] / a[0], 100.0 * b[1] / b[0]
            print("%-6d %10d %9.1f %% %9d %9.1f %% %8.2fx"
                  % (v, a[0], pa, b[0], pb, pa / pb if pb else 0))
        print()


if __name__ == "__main__":
    main()
