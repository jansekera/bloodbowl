#!/usr/bin/env python3
"""K29⭐⭐ — PRAVIDLO KLECE (uživatel 19.08.2026).

  „Optimum klece jsou ČTYŘI rohy, všechny ČISTÉ,
   a ŽÁDNÍ další sousedi s ballcarrierem."

Je to PRAVIDLO, ne výsledek měření ⇒ neptáme se, jestli platí, ale JAK ČASTO
ho plníme a CO ho láme.

⛔ Proč vzniklo: dosavadní `K29full` kontroluje jen DVĚ ze TŘÍ podmínek —
`len(filled) == 4 and not dirty`.  Třetí klauzuli (nosič nemá jiné sousedy)
neumí vůbec vyjádřit, takže kolo, kde soupeř stojí NAVÁZANÝ NA NOSIČE, projde
jako „plná čistá klec".  Táž rodina jako K33: kontrola měří jinou věc,
než jakou má vynutit.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
R = importlib.import_module("diag_rules_checks_20260812")

ORTH = [(-1, 0), (1, 0), (0, -1), (0, 1)]
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
            st["našich kol s míčem na konci"] += 1
            cx, cy = car["x"], car["y"]

            diag = [(cx + dx, cy + dy) for dx, dy in DIAG]
            orth = [(cx + dx, cy + dy) for dx, dy in ORTH]
            occ_us = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
            occ_them = {(p["x"], p["y"]): p for p in them}

            filled = [d for d in diag if d in occ_us]
            threat = [(p["x"], p["y"]) for p in them if R.threatens(p)]
            dirty = [d for d in filled if any(R.adj(d, t) for t in threat)]

            # 3. klauzule: ŽÁDNÍ další sousedi nosiče.  Soused = kdokoli
            # v některém ze čtyř ORTOGONÁLNÍCH polí (diagonály jsou rohy),
            # a navíc soupeř stojící v NĚKTERÉM z osmi polí -- ten je soused
            # nosiče bez ohledu na to, že stojí na "rohovém" poli.
            extra_ours = [o for o in orth if o in occ_us]
            extra_them_orth = [o for o in orth if o in occ_them]
            them_on_diag = [d for d in diag if d in occ_them]
            extra_any = extra_ours + extra_them_orth + them_on_diag

            c1 = len(filled) == 4
            c2 = not dirty
            c3 = not extra_any

            st["① 4 rohy"] += c1
            st["② všechny čisté"] += c2
            st["③ žádní další sousedi"] += c3
            st["PRAVIDLO (①∧②∧③)"] += (c1 and c2 and c3)
            st["dnešní K29⭐ (①∧②)"] += (c1 and c2)
            if c1 and c2 and not c3:
                st["⛔ K29⭐ říká PLNÁ ČISTÁ, a přesto má nosič dalšího souseda"] += 1
                if extra_them_orth or them_on_diag:
                    st["   z toho: SOUPEŘ vedle nosiče (lehlý/omráčený ⇒ 'čisté')"] += 1
                else:
                    st["   z toho: jen NAŠE tělo navíc"] += 1

            # co láme třetí klauzuli
            if extra_them_orth or them_on_diag:
                st["③ lomeno SOUPEŘEM (nosič je v kontaktu)"] += 1
            if extra_ours and not (extra_them_orth or them_on_diag):
                st["③ lomeno JEN NAŠÍM tělem navíc"] += 1
            st["našich těl navíc u nosiče celkem"] += len(extra_ours)
            st["soupeřů u nosiče celkem"] += len(extra_them_orth) + len(them_on_diag)

            # ⭐ STROP PŘED BĚHEM: kolik chybějících rohů by se zaplatilo
            # POUHÝM PŘEKROČENÍM O JEDNO POLE -- tělo, které dnes stojí
            # ortogonálně u nosiče (pravidlo to zakazuje), je od prázdného
            # rohu vzdálené jedno pole a už u nosiče JE.
            empty_diag = [d for d in diag if d not in occ_us and d not in occ_them]
            if len(filled) < 4 and empty_diag and extra_ours:
                st["⭐ chybí roh, a naše tělo stojí ORTOGONÁLNĚ (1 pole vedle)"] += 1
                st["⭐ takto opravitelných rohů celkem"] += min(len(empty_diag), len(extra_ours))

            # kolik rohů vlastně stojí, když pravidlo neplatí
            st[f"rohů obsazeno = {len(filled)}"] += 1
    return st


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = run(paths)
    n = st["našich kol s míčem na konci"]
    print(f"korpus: {len(paths)} her · {n} našich kol s míčem na konci kola\n")
    order = ["① 4 rohy", "② všechny čisté", "③ žádní další sousedi",
             "dnešní K29⭐ (①∧②)", "PRAVIDLO (①∧②∧③)",
             "⛔ K29⭐ říká PLNÁ ČISTÁ, a přesto má nosič dalšího souseda",
             "③ lomeno SOUPEŘEM (nosič je v kontaktu)",
             "③ lomeno JEN NAŠÍM tělem navíc",
             "   z toho: SOUPEŘ vedle nosiče (lehlý/omráčený ⇒ 'čisté')",
             "   z toho: jen NAŠE tělo navíc",
             "⭐ chybí roh, a naše tělo stojí ORTOGONÁLNĚ (1 pole vedle)"]
    for k in order:
        print(f"  {k:<62} {st[k]:6d}  {100.0*st[k]/n:5.1f} %")
    print()
    for k in sorted(st):
        if k.startswith("rohů obsazeno"):
            print(f"  {k:<62} {st[k]:6d}  {100.0*st[k]/n:5.1f} %")
    print(f"\n  ⭐ takto opravitelných rohů celkem {st['⭐ takto opravitelných rohů celkem']}"
          f"  =  {st['⭐ takto opravitelných rohů celkem']/n:.2f} rohu na kolo")
    print(f"\n  ⌀ našich těl navíc u nosiče  {st['našich těl navíc u nosiče celkem']/n:.2f}"
          f"   ·  ⌀ soupeřů u nosiče  {st['soupeřů u nosiče celkem']/n:.2f}")


if __name__ == "__main__":
    main()
