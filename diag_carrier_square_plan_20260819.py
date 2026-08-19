#!/usr/bin/env python3
"""JDE POLE NOSIČE DOPOČÍTAT TAK, ABY KLEC VYŠLA CELÁ A ČISTÁ?
(uživatel 19.08.: „podle toho, kde bude stát nosič v našem kole, přece
dopočítáme vše včetně toho, aby byly rohy čisté")

Pravidlo obrací pořadí rozhodování: dnes nosič popojde (rovně kupředu,
`cage_advance.cpp:41`) a klec se dopočítává k místu, kam došel. Podle pravidla
se má cílové pole nosiče VYBÍRAT podle klece, která z něj vyjde.

Otázka na korpus: **existuje mezi poli, kam nosič v tom kole dosáhne, aspoň
jedno, ze kterého jde postavit PLNÁ ČISTÁ klec bez dalších sousedů?**

⚠️ Co to je a co ne:
  * je to STROP dosažitelnosti, ne plán -- neptá se, co to stojí na tempu;
  * dosah se počítá Chebyshevem z `ma` (bez TZ, bez dodge, bez GFI) => horní mez;
  * těla musí být STOJÍCÍ a nesmí to být nosič; ležící se nepočítají (konzervativní);
  * čistota se posuzuje proti STOJÍCÍM soupeřům tam, kde stojí na začátku
    našeho kola -- v jejich kole se pohnou, takže je to podmínka na konec
    NAŠEHO kola, přesně jak ji měří K29**.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]
ORTH = [(-1, 0), (1, 0), (0, -1), (0, 1)]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def can_fill(corners, bodies):
    """Bipartitní párování 4 rohů na těla (bitmask přes rohy)."""
    m = len(corners)
    reach = []
    for c in corners:
        reach.append([i for i, b in enumerate(bodies) if cheb((b["x"], b["y"]), c) <= b["ma"]])
    used = set()

    def go(k, used_mask):
        if k == m:
            return True
        for i in reach[k]:
            if not (used_mask >> i) & 1:
                if go(k + 1, used_mask | (1 << i)):
                    return True
        return False
    return go(0, 0)


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
        for S in r["turn_logs"]:
            if S["active_team"] != ours:
                continue
            us = R.players(S, ours)
            them = R.players(S, theirs)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            st["kol (nosič na začátku našeho kola)"] += 1
            them_std = [p for p in them if p["state"] == STANDING]
            them_all = {(p["x"], p["y"]) for p in them}
            bodies = [p for p in us if p["id"] != car["id"] and p["state"] == STANDING]
            st["volných stojících těl celkem"] += len(bodies)
            if len(bodies) < 4:
                st["⛔ méně než 4 stojící těla — pravidlo je mimo dosah rozpočtem"] += 1
                continue

            cpos = (car["x"], car["y"])
            best = None
            for dx in range(-car["ma"], car["ma"] + 1):
                for dy in range(-car["ma"], car["ma"] + 1):
                    cand = (cpos[0] + dx, cpos[1] + dy)
                    if not (0 <= cand[0] <= 25 and 0 <= cand[1] <= 14):
                        continue
                    if cand != cpos and (cand in them_all
                                         or any((p["x"], p["y"]) == cand for p in us)):
                        continue
                    corners = [(cand[0] + a, cand[1] + b) for a, b in DIAG]
                    if any(not (0 <= c[0] <= 25 and 0 <= c[1] <= 14) for c in corners):
                        continue
                    if any(c in them_all for c in corners):
                        continue
                    # (3) žádný další soused nosiče: 4 ortogonální pole prázdná
                    # a v žádném z 8 polí soupeř
                    orth = [(cand[0] + a, cand[1] + b) for a, b in ORTH]
                    if any(o in them_all for o in orth):
                        continue
                    # (2) všechny rohy čisté: u žádného nesmí stát soupeř
                    if any(any(cheb((p["x"], p["y"]), c) <= 1 for p in them_std)
                           for c in corners):
                        continue
                    # (1) čtyři těla na čtyři rohy
                    if not can_fill(corners, bodies):
                        continue
                    d = cheb(cpos, cand)
                    if best is None or d < best:
                        best = d
            if best is None:
                st["⛔ ŽÁDNÉ dosažitelné pole nedá plnou čistou klec"] += 1
            else:
                st["✅ existuje pole, ze kterého klec vyjde CELÁ a ČISTÁ"] += 1
                st[f"   nejbližší takové pole je {best} polí daleko"] += 1
    return st


def main():
    st = run(sorted(glob.glob(sys.argv[1])))
    n = st["kol (nosič na začátku našeho kola)"]
    print("%d našich kol s nosičem na začátku kola" % n)
    print("⌀ volných stojících těl mimo nosiče: %.2f\n"
          % (st["volných stojících těl celkem"] / n))
    for k in ["✅ existuje pole, ze kterého klec vyjde CELÁ a ČISTÁ",
              "⛔ ŽÁDNÉ dosažitelné pole nedá plnou čistou klec",
              "⛔ méně než 4 stojící těla — pravidlo je mimo dosah rozpočtem"]:
        print("  %-58s %6d  %5.1f %%" % (k, st[k], 100.0 * st[k] / n))
    ok = st["✅ existuje pole, ze kterého klec vyjde CELÁ a ČISTÁ"]
    print("\n  jak daleko je nejbližší takové pole (z těch %d kol):" % ok)
    for d in range(9):
        c = st[f"   nejbližší takové pole je {d} polí daleko"]
        if c:
            print("     %d polí: %6d  %5.1f %%" % (d, c, 100.0 * c / ok))


if __name__ == "__main__":
    main()
