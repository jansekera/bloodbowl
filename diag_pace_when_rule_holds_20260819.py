#!/usr/bin/env python3
"""KAPACITA FÁZE SE NESMÍ ČÍST Z KOL, KDE SE PRAVIDLO NEHRAJE (19.08.)

Uživatel: „tady předpokládám záplavu starých čísel neaplikovatelných na poslední
rozvrh." Přesně tak: tempo klece 1,93 pole/kolo je výkon klece, která má ⌀ 1,3
rohu a pravidlo (K29**) plní ve 2,7 % kol. Zapsat to jako KAPACITU znamená
povýšit dnešní neschopnost na strop.

⇒ Kapacita se čte JEN z kol, kde klec skutečně stojí. Řezy podle přísnosti:
   všechna · >=2 rohy · 4 rohy · 4 rohy a všechny čisté · CELÉ PRAVIDLO
"""
import sys, glob
from collections import defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]
ORTH = [(-1, 0), (1, 0), (0, -1), (0, 1)]


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    agg = defaultdict(list)
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
            dx = (carE["x"] - car["x"]) * fwd
            them = R.players(S, theirs)
            them_std = [p for p in them if p["state"] == STANDING]
            occ = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
            them_at = {(p["x"], p["y"]) for p in them}
            diag = [(car["x"] + a, car["y"] + b) for a, b in DIAG]
            orth = [(car["x"] + a, car["y"] + b) for a, b in ORTH]
            filled = [d for d in diag if d in occ]
            dirty = [d for d in filled
                     if any(max(abs(p["x"] - d[0]), abs(p["y"] - d[1])) <= 1
                            for p in them_std)]
            extra = ([o for o in orth if o in occ] + [o for o in orth if o in them_at]
                     + [d for d in diag if d in them_at])

            agg["všechna kola s nosičem"].append(dx)
            if len(filled) >= 2:
                agg["≥2 rohy"].append(dx)
            if len(filled) == 4:
                agg["4 rohy"].append(dx)
                if not dirty:
                    agg["4 rohy, všechny čisté"].append(dx)
                    if not extra:
                        agg["⭐ CELÉ PRAVIDLO (K29⭐⭐)"].append(dx)
    print("Δx nosiče podle toho, JAK DOBŘE klec stojí (%d her)\n" % len(paths))
    print("%-32s %8s %9s %9s %9s" % ("řez", "kol", "⌀ Δx", "medián", "p75"))
    for k in ["všechna kola s nosičem", "≥2 rohy", "4 rohy",
              "4 rohy, všechny čisté", "⭐ CELÉ PRAVIDLO (K29⭐⭐)"]:
        v = sorted(agg[k])
        if not v:
            continue
        print("%-32s %8d %9.2f %9.1f %9.1f"
              % (k, len(v), sum(v) / len(v), v[len(v) // 2], v[int(len(v) * 0.75)]))


if __name__ == "__main__":
    main()
