#!/usr/bin/env python3
"""P9c — přísnější nástupce Fableho „61 % polluterů jde udeřit zdarma" (14.08.2026).

Uživatel 14.08.: *„priorita u špinavého rohu je odklidit protihráče pryč od
rohu — ne jej nechat u rohu a posunout blíž k balonu."*

Fableho 61 % počítá, kdo MŮŽE udeřit. Tohle počítá, jestli ten úder pollutera
od rohu opravdu ODKLIDÍ. Tři odsunová pole jsou dána vektorem
`polluter − blokující` (`helpers.cpp:194 getPushbackSquares`: rovně + 45° CW +
45° CCW), takže KDO udeří určuje, KAM se dá odsunout — proto se to počítá
přes všechny kandidáty na blok, ne přes jednoho.

Definice se NEVYMÝŠLEJÍ: rohy, polluteři a „volný stojící soused" se berou
z `diag_rules_checks_20260812` a z Fableho `extract.py`, aby čísla byla
srovnatelná s reportem z téhož dne.

⚠️ Rozhodovací bod je ZAČÁTEK kola — přesně proto tahle otázka nepotřebuje
nové logování. Je to volba „koho pošlu", ne rekonstrukce průběhu kola.
Výhrada zůstává: skutečné pořadí akcí uvnitř kola může desku změnit dřív,
takže tohle je horní mez proveditelnosti, ne záruka.
"""
import glob, gzip, json, sys
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING
from diag_exposure_scan_20260812 import Board

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 0

COMPASS = [(0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1), (-1, 0), (-1, -1)]
IDX = {c: i for i, c in enumerate(COMPASS)}


def on_pitch(x, y):
    return 0 <= x <= 25 and 0 <= y <= 14


def pushback_squares(ax, ay, dx_, dy_):
    """helpers.cpp:194 — rovně dozadu, pak 45° CW a CCW, jen pole na hřišti."""
    vx, vy = dx_ - ax, dy_ - ay
    vx = (vx > 0) - (vx < 0)
    vy = (vy > 0) - (vy < 0)
    i = IDX.get((vx, vy), 0)
    out = []
    for d in (i, (i + 1) % 8, (i + 7) % 8):
        px, py = dx_ + COMPASS[d][0], dy_ + COMPASS[d][1]
        if on_pitch(px, py):
            out.append((px, py))
    return out


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def main():
    paths = sorted(glob.glob(DATA + "/*.json.gz"))
    if LIMIT:
        paths = paths[:LIMIT]

    n = Counter()
    per_opp = defaultdict(Counter)
    dest_kind = Counter()
    n_games = n_turns = 0

    for path in paths:
        r = load(path)
        if r["home_race"] == "dwarf":
            ours, opp_race = "home", r["away_race"]
        elif r["away_race"] == "dwarf":
            ours, opp_race = "away", r["home_race"]
        else:
            continue
        n_games += 1
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]

        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            if S.get("touchdown") or logs[i + 1]["half"] != S["half"]:
                continue
            n_turns += 1
            us_S, them_S = players(S, ours), players(S, theirs)
            car = next((p for p in us_S if p["has_ball"]), None)
            if car is None:
                continue
            b = Board(S, ours)
            carpos = (car["x"], car["y"])
            diag = [(car["x"] + ddx, car["y"] + ddy)
                    for ddx in (-1, 1) for ddy in (-1, 1)]
            occ = {(p["x"], p["y"]): p for p in us_S}
            filled = [d for d in diag if d in occ]
            if not filled:
                continue
            threat = [p for p in them_S if threatens(p)]
            polluters = [t for t in threat
                         if any(adj(d, (t["x"], t["y"])) for d in filled)]
            if not polluters:
                continue

            occupied = {(p["x"], p["y"]) for p in us_S + them_S}

            for t in polluters:
                tpos = (t["x"], t["y"])
                n["polluters"] += 1
                per_opp[opp_race]["polluters"] += 1

                # Fableho C1: existuje volný stojící soused (= horní mez)
                elig = [p for p in b.us_st
                        if cheb((p["x"], p["y"]), tpos) == 1
                        and not p["has_ball"]
                        and (p["x"], p["y"]) not in set(filled)]
                if not elig:
                    continue
                n["has_hitter"] += 1
                per_opp[opp_race]["has_hitter"] += 1

                # P9c: existuje ÚDER, po kterém polluter roh nešpiní
                # a zároveň se nepřiblíží k nosiči?
                any_clear = any_clear_safe = any_safe_only = False
                for h in elig:
                    for d in pushback_squares(h["x"], h["y"], t["x"], t["y"]):
                        if d in occupied:
                            dest_kind["obsazené (odsun by řetězil)"] += 1
                            continue
                        clears = not any(adj(c, d) for c in filled)
                        nearer = cheb(d, carpos) < cheb(tpos, carpos)
                        touches = cheb(d, carpos) == 1
                        safe = not nearer and not touches
                        dest_kind["čistí+bezpečné" if (clears and safe) else
                                  "čistí, ale k míči" if clears else
                                  "nečistí (zůstane u rohu)"] += 1
                        if clears:
                            any_clear = True
                            if safe:
                                any_clear_safe = True
                        if safe:
                            any_safe_only = True
                if any_clear:
                    n["clearable"] += 1
                    per_opp[opp_race]["clearable"] += 1
                if any_clear_safe:
                    n["clearable_safe"] += 1
                    per_opp[opp_race]["clearable_safe"] += 1
                if any_safe_only and not any_clear:
                    n["safe_but_stuck"] += 1

    def pct(a, b_):
        return 100.0 * a / b_ if b_ else 0.0

    P = n["polluters"]
    print("korpus: %d her | našich kol: %d | polluterů: %d\n" % (n_games, n_turns, P))
    print("=== ŽEBŘÍČEK, JAK PŘÍSNĚ SE PTÁME ===")
    print("  má volného stojícího souseda (Fable C1)   %6d  %5.1f %%"
          % (n["has_hitter"], pct(n["has_hitter"], P)))
    print("  ...a existuje odsun, co ho ODKLIDÍ OD ROHU %6d  %5.1f %%"
          % (n["clearable"], pct(n["clearable"], P)))
    print("  ...a zároveň ho NEPŘIBLÍŽÍ K NOSIČI        %6d  %5.1f %%   <== P9c"
          % (n["clearable_safe"], pct(n["clearable_safe"], P)))
    print()
    print("  polluterů, kde jde odsun bezpečně, ale roh nepustí: %d (%.1f %%)"
          % (n["safe_but_stuck"], pct(n["safe_but_stuck"], P)))
    print("  ztráta proti Fableho hornímu odhadu: %.1f pp"
          % (pct(n["has_hitter"], P) - pct(n["clearable_safe"], P)))
    print()
    print("=== JEDNOTLIVÁ CÍLOVÁ POLE (přes všechny kandidáty na blok) ===")
    tot = sum(dest_kind.values())
    for k, v in dest_kind.most_common():
        print("  %-34s %7d  %5.1f %%" % (k, v, pct(v, tot)))
    print()
    print("=== PO SOUPEŘÍCH ===")
    print("  %-10s %8s %10s %10s %10s" % ("soupeř", "pollut.", "soused", "odklidí", "P9c"))
    for o in sorted(per_opp):
        c = per_opp[o]
        print("  %-10s %8d %9.1f %% %9.1f %% %9.1f %%"
              % (o, c["polluters"], pct(c["has_hitter"], c["polluters"]),
                 pct(c["clearable"], c["polluters"]),
                 pct(c["clearable_safe"], c["polluters"])))


main()
