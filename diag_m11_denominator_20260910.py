#!/usr/bin/env python3
"""
M11 — ROZPAD JMENOVATELE (10.09.2026)

⚑ PROČ VZNIKL. Přeměření M11(a) na čerstvém korpusu (09.09., engine cf8634e8)
dalo proti baseline 19.08. dvě věci naráz:
    stojící volný nosič       29,2 %  ->  18,7 %
    z toho pole vpřed NAŠE     0,50   ->   0,28
⛔ Ale SOUČASNĚ se posunul JMENOVATEL: volných nosičů na hru ubylo
   4,08 -> 2,85 (-30 %). Dokud nevím, PROČ, nejde říct, jestli vada ustoupila,
   nebo se jen změnilo složení situací, ve kterých se měří.
   ⇒ Tenhle skript tiskne ZBYTEK: kam se ty tahy poděly.

⭐ Tiskne se VŠECHNO, co se sečte do celku, včetně zbytku — záporný nebo
   nesedící zbytek chybu prozradí hned (feedback_take_counters_are_cumulative).

Použití: python3 diag_m11_denominator_20260910.py <korpus> [<korpus2> ...]
"""
import glob
import gzip
import json
import sys


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def analyze(data):
    files = sorted(glob.glob(f"{data}/g*.json.gz"))
    if not files:
        return None

    games = turns_ours = 0
    no_carrier = carrier_down = 0
    free = marked = 0
    free_stood = free_went = 0
    ahead_ours = ahead_theirs = ahead_empty = 0
    tds = 0

    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "dwarf":
            us, them = "home", "away"
        elif g["away_race"] == "dwarf":
            us, them = "away", "home"
        else:
            continue
        games += 1
        tds += g.get(f"{us}_score", 0)
        dx = 1 if us == "home" else -1

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            turns_ours += 1
            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            car = next((p for p in mine if p["has_ball"]), None)
            if car is None:
                no_carrier += 1
                continue
            if car["state"] != 0:
                carrier_down += 1
                continue
            stand_theirs = [p for p in theirs if p["state"] == 0]
            if any(cheb(car["x"], car["y"], e["x"], e["y"]) == 1
                   for e in stand_theirs):
                marked += 1
                continue
            free += 1

            moved = any(e.get("player_id") == car["id"]
                        and e["type"] in ("MOVE", "GFI", "DODGE")
                        for e in t["events"])
            if moved:
                free_went += 1
                continue
            free_stood += 1

            # jen u STOJÍCÍHO volného nosiče: co je na poli PŘÍMO VPŘED
            q = (car["x"] + dx, car["y"])
            ours = {(p["x"], p["y"]) for p in mine if p["state"] != 1}
            occ_them = {(p["x"], p["y"]) for p in theirs if p["state"] != 1}
            if q in ours:
                ahead_ours += 1
            elif q in occ_them:
                ahead_theirs += 1
            else:
                ahead_empty += 1

    return dict(games=games, turns=turns_ours, no_carrier=no_carrier,
                carrier_down=carrier_down, marked=marked, free=free,
                free_stood=free_stood, free_went=free_went,
                ahead_ours=ahead_ours, ahead_theirs=ahead_theirs,
                ahead_empty=ahead_empty, tds=tds)


def show(name, r):
    g, t = r["games"], r["turns"]
    print(f"\n=== {name}  ({g} her, {t} našich tahů, {t/g:.2f} na hru) ===")
    print(f"  TD na hru                      {r['tds']/g:6.3f}")
    print("  --- kam padnou naše tahy (musí dát dohromady 100 %) ---")
    parts = [("nemáme míč", r["no_carrier"]), ("nosič LEŽÍ", r["carrier_down"]),
             ("nosič v cizí TZ (markovaný)", r["marked"]),
             ("nosič VOLNÝ", r["free"])]
    acc = 0
    for label, n in parts:
        acc += n
        print(f"    {label:30s} {n:6d}  {100*n/t:5.1f} %")
    print(f"    {'ZBYTEK (musí být 0)':30s} {t-acc:6d}")
    f = r["free"]
    print(f"  --- volný nosič: {f} tahů, {f/g:.2f} na hru ---")
    print(f"    {'STÁL':30s} {r['free_stood']:6d}  {100*r['free_stood']/f:5.1f} %")
    print(f"    {'ŠEL':30s} {r['free_went']:6d}  {100*r['free_went']/f:5.1f} %")
    s = r["free_stood"]
    print(f"  --- pole PŘÍMO VPŘED u {s} stojících volných nosičů ---")
    sub = [("NÁŠ hráč", r["ahead_ours"]), ("JEJICH hráč", r["ahead_theirs"]),
           ("prázdné", r["ahead_empty"])]
    acc = 0
    for label, n in sub:
        acc += n
        print(f"    {label:30s} {n:6d}  {100*n/s:5.1f} %")
    print(f"    {'ZBYTEK (musí být 0)':30s} {s-acc:6d}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for d in sys.argv[1:]:
        r = analyze(d)
        if r is None:
            print(f"⛔ žádná data v {d}")
        else:
            show(d, r)
