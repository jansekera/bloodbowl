#!/usr/bin/env python3
"""PROČ KLEC ČASTO NESTOJÍ VŮBEC (20.08.2026)

Uživatel 20.08.: *„to, že klec často nestojí vůbec, je zajímavější než
čistota… čím to je, že nestojí?"*

Snímek stavu klece ukázal: **v 8 237 z 24 754 kol s míčem nemá nosič ANI
JEDEN obsazený roh** (diagonálu). To je jiná vada než špinavé rohy.

Vynechává **první a poslední kolo půle** (uživatel) — tam je stav vynucený
výkopem, resp. koncem.
"""
import sys, glob
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from collections import Counter

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]
ORTH = [(-1, 0), (1, 0), (0, -1), (0, 1)]


def main():
    st = Counter()
    for path in sorted(glob.glob(sys.argv[1])):
        r = R.load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if E["half"] != S["half"] or S.get("touchdown"):
                continue
            if S["turn"] <= 1 or S["turn"] >= 8:       # uživatel: vynech krajní
                continue
            car = next((p for p in R.players(E, ours) if p["has_ball"]), None)
            if car is None or car["state"] != STANDING:
                continue
            us = [p for p in R.players(E, ours) if p["id"] != car["id"]]
            occ = {(p["x"], p["y"]): p for p in us}
            diag = [(car["x"] + a, car["y"] + b) for a, b in DIAG]
            orth = [(car["x"] + a, car["y"] + b) for a, b in ORTH]
            filled = [d for d in diag if d in occ]
            st["kol s nosičem (bez krajních)"] += 1
            if filled:
                st["  klec stojí (≥1 roh)"] += 1
                continue
            st["  ⛔ ANI JEDEN ROH"] += 1

            # --- proč? kategorie, vzájemně výlučné, v tomto pořadí ---
            near_orth = [occ[o] for o in orth if o in occ]
            standing_free = [p for p in us if p["state"] == STANDING]
            def dist(p, q): return max(abs(p[0] - q[0]), abs(p[1] - q[1]))
            # dosáhlo by některé volné tělo na roh? (Chebyshev z ma, horní mez)
            could = [p for p in standing_free
                     if any(dist((p["x"], p["y"]), d) <= p.get("ma", 6) for d in diag)]
            nearest = min((dist((p["x"], p["y"]), (car["x"], car["y"])) for p in us),
                          default=99)
            if near_orth:
                st["    (a) tělo stojí ORTOGONÁLNĚ u nosiče (zakázané pole)"] += 1
                st["        z toho by na roh DOSÁHLO"] += 1 if could else 0
            elif not standing_free:
                st["    (b) žádné volné stojící tělo (všichni leží)"] += 1
            elif not could:
                st["    (c) nikdo na roh NEDOSÁHNE (nosič utekl vpřed)"] += 1
                st["        ⌀ vzdálenost nejbližšího těla"] += nearest
            else:
                st["    (d) DOSÁHLI BY, a přesto tam nikdo nestojí"] += 1
                st["        ⌀ vzdálenost nejbližšího těla (d)"] += nearest
                st["        ⌀ kolik těl by dosáhlo"] += len(could)

    n = st["kol s nosičem (bez krajních)"]
    z = st["  ⛔ ANI JEDEN ROH"]
    print(f"kol s naším stojícím nosičem (kola 2–7): {n}")
    print(f"  klec stojí (≥1 roh)          {st['  klec stojí (≥1 roh)']:6d}  {100.0*st['  klec stojí (≥1 roh)']/n:5.1f} %")
    print(f"  ⛔ ANI JEDEN ROH             {z:6d}  {100.0*z/n:5.1f} %\n")
    print(f"  PROČ (jmenovatel = {z} kol bez jediného rohu):")
    for k in ["    (a) tělo stojí ORTOGONÁLNĚ u nosiče (zakázané pole)",
              "    (b) žádné volné stojící tělo (všichni leží)",
              "    (c) nikdo na roh NEDOSÁHNE (nosič utekl vpřed)",
              "    (d) DOSÁHLI BY, a přesto tam nikdo nestojí"]:
        print(f"  {k:58s} {st[k]:6d}  {100.0*st[k]/z:5.1f} %")
    if st["    (c) nikdo na roh NEDOSÁHNE (nosič utekl vpřed)"]:
        print(f"      (c) ⌀ vzdálenost nejbližšího těla: "
              f"{st['        ⌀ vzdálenost nejbližšího těla']/st['    (c) nikdo na roh NEDOSÁHNE (nosič utekl vpřed)']:.1f}")
    if st["    (d) DOSÁHLI BY, a přesto tam nikdo nestojí"]:
        d = st["    (d) DOSÁHLI BY, a přesto tam nikdo nestojí"]
        print(f"      (d) ⌀ vzdálenost nejbližšího těla: {st['        ⌀ vzdálenost nejbližšího těla (d)']/d:.1f}"
              f"   ⌀ těl, která by dosáhla: {st['        ⌀ kolik těl by dosáhlo']/d:.1f}")


if __name__ == "__main__":
    main()
