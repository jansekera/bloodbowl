#!/usr/bin/env python3
"""
M11 (27.08.2026) — VOLNÝ NOSIČ STOJÍ: co bylo v nabídce a jak dobře ho kryjeme.

⚑ PROČ AŽ TEĎ A PROČ TAKHLE. Uživatel 26.08.: „jak často necháme nosiče stát,
bych vyhodnocoval až po posunu v tom, jak dobře jej kryjeme klecí." Kvalita
krytí se do 27.08. NELOGOVALA -- `plan.filled_corners` je v korpusu trvale 0,
protože plánovač v produkci neběží (T5.34). Engine to od 27.08. razítkuje sám,
ale STARÝ korpus ta pole nemá. ⇒ tady se počítají TOUTÉŽ definicí ze snímku:
snímek nese pozice všech hráčů, takže rohy i ortogonály jsou dopočitatelné.
Definice se drží `cage_advance.h` (CageSnapshot), aby se čísla dala srovnat
s tím, co bude engine logovat příště.

⭐ CO SE PTÁ (a čím se to liší od 26.08.):
26.08. se změřilo, ŽE volný nosič stojí (529 tahů) a že cestu vpřed zavírají
naši (149/149). Tady se ptáme, CO SE MU MÍSTO TOHO NABÍZELO -- rekonstrukcí
brány (importem z diag_fable_offered_played_20260817) -- a JAK DOBŘE byl krytý,
aby šlo oddělit „stojí a je krytý" od „stojí a je nahý".

⚠️ Omezení dědí rekonstrukce brány: snímek je ZAČÁTEK kola, „nabídnuto" je
podlaha. V dw-dw se čte jen strana `home`.

Použití: nice -n 19 python3 diag_m11_free_carrier_20260827.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter

sys.path.insert(0, ".")
_m = __import__("diag_fable_offered_played_20260817")
analyze_turn_offers = _m.analyze_turn_offers

DATA = sys.argv[1] if len(sys.argv) > 1 else "blitzlanding_replic_20260825_corpus_data"


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def cage_snapshot(car, mine, theirs, dx):
    """Táž definice jako CageSnapshot v cage_advance.h (T5.34)."""
    occ = {(p["x"], p["y"]): p for p in mine + theirs if p["state"] != 1}
    ours = {(p["x"], p["y"]) for p in mine if p["state"] != 1}
    stand_theirs = [p for p in theirs if p["state"] == 0]

    corners = marked = 0
    for sx in (-1, 1):
        for sy in (-1, 1):
            q = (car["x"] + sx, car["y"] + sy)
            p = occ.get(q)
            if p is None or q not in ours or p["state"] != 0:
                continue
            corners += 1
            if any(cheb(q[0], q[1], e["x"], e["y"]) == 1 for e in stand_theirs):
                marked += 1

    ortho = ortho_ours = ahead = ahead_ours = 0
    for ddx, ddy in ((dx, 0), (-dx, 0), (0, -1), (0, 1)):
        q = (car["x"] + ddx, car["y"] + ddy)
        if q not in occ:
            continue
        ortho += 1
        is_ours = q in ours
        if is_ours:
            ortho_ours += 1
        if (ddx, ddy) == (dx, 0):
            ahead = 1
            ahead_ours = 1 if is_ours else 0
    tz = sum(1 for e in stand_theirs
             if cheb(car["x"], car["y"], e["x"], e["y"]) == 1)
    return corners, marked, ortho, ortho_ours, ahead, ahead_ours, tz


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    stood = Counter()      # co bylo v nabídce, když volný nosič STÁL
    moved = Counter()      # totéž, když se pohnul
    cover_stood = Counter()
    cover_moved = Counter()
    n_free = n_stood = 0
    games = 0

    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "dwarf":
            us, them = "home", "away"
        elif g["away_race"] == "dwarf":
            us, them = "away", "home"
        else:
            continue
        games += 1
        ez = 25 if us == "home" else 0
        dx = 1 if us == "home" else -1
        opp_race = g[f"{them}_race"]

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            car = next((p for p in mine if p["has_ball"]), None)
            if car is None or car["state"] != 0:
                continue
            stand_theirs = [p for p in theirs if p["state"] == 0]
            # VOLNÝ = v žádné nepřátelské tacklezóně (definice 26.08.)
            if any(cheb(car["x"], car["y"], e["x"], e["y"]) == 1
                   for e in stand_theirs):
                continue
            n_free += 1

            car_moved = any(e.get("player_id") == car["id"]
                            and e["type"] in ("MOVE", "GFI", "DODGE")
                            for e in t["events"])
            ball = {"x": t["ball_x"], "y": t["ball_y"], "held": t["ball_held"]}
            off = analyze_turn_offers(mine, theirs, "dwarf", opp_race, ez,
                                      t["turn"], t["weather"], ball)
            cov = cage_snapshot(car, mine, theirs, dx)
            tgt, ctgt = (stood, cover_stood) if not car_moved else (moved, cover_moved)
            if not car_moved:
                n_stood += 1
            for k, v in off.items():
                if v > 0 and not k.startswith("_"):
                    tgt[k] += 1
            ctgt["rohy"] += cov[0]
            ctgt["z toho označené"] += cov[1]
            ctgt["ortogonály obsazené"] += cov[2]
            ctgt["  z toho NAŠI"] += cov[3]
            ctgt["pole PŘÍMO VPŘED obsazené"] += cov[4]
            ctgt["  z toho NÁŠ hráč"] += cov[5]
            ctgt["n"] += 1

    print(f"korpus {DATA}: {games} her")
    print(f"volný nosič (v žádné TZ) v {n_free} tazích; "
          f"z toho STÁL v {n_stood} ({n_stood / max(1, n_free):.1%})\n")

    print("CO BYLO V NABÍDCE — podíl tahů, kde makro bylo k dispozici")
    print(f"{'makro':<20}{'nosič STÁL':>14}{'nosič ŠEL':>14}{'rozdíl':>10}")
    print("-" * 58)
    keys = sorted(set(stood) | set(moved),
                  key=lambda k: -(stood[k] / max(1, n_stood)))
    n_moved = n_free - n_stood
    for k in keys:
        a = stood[k] / max(1, n_stood)
        b = moved[k] / max(1, n_moved)
        print(f"{k:<20}{a:>13.1%}{b:>14.1%}{a - b:>+10.1%}")

    print("\nJAK DOBŘE BYL KRYTÝ (průměr na tah, definice = CageSnapshot/T5.34)")
    print(f"{'veličina':<30}{'nosič STÁL':>14}{'nosič ŠEL':>14}")
    print("-" * 58)
    for k in ("rohy", "z toho označené", "ortogonály obsazené", "  z toho NAŠI",
              "pole PŘÍMO VPŘED obsazené", "  z toho NÁŠ hráč"):
        a = cover_stood[k] / max(1, cover_stood["n"])
        b = cover_moved[k] / max(1, cover_moved["n"])
        print(f"{k:<30}{a:>14.2f}{b:>14.2f}")


if __name__ == "__main__":
    main()
