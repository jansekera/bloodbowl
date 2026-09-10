#!/usr/bin/env python3
"""
STAVÍME VŮBEC KLEC? A JE ČISTÁ? (10.09.2026)

⚑ ZADÁNÍ UŽIVATELE 10.09.: „hlavní je kontrola, že stavíme správnou a čistou
  klec — předtím jsme kontrolovali posun klece a až potom jsme zjistili, že klec
  nestavíme; tak nemůžeme měřit její posun."
⇒ Je to jeho vlastní pravidlo z `celotah_situace.md`: ⛔ NEMĚŘIT, CO MODEL NEUMÍ.
  Dokud se neví, jestli klec vzniká, je každé měření JEJÍHO POSUNU bezcenné.

⭐ PROČ ROZDĚLENÍ A NE PRŮMĚR. Průměr „2,3 rohu ze 4" se dá přečíst dvěma
  neslučitelnými způsoby: „většinou stavíme 2" nebo „půlku kol 4 a půlku 0".
  Rozhoduje o tom, jestli je vada v POČTU těl, nebo v JEJICH VOLBĚ.
  ⇒ Tiskne se histogram 0-4 a zvlášť ČISTOTA.

Definice drží `cageSnapshot` (`cage_advance.cpp:113-135`), aby čísla šla srovnat
s tím, co engine počítá sám:
  · roh drží JEN náš STOJÍCÍ hráč (ležící tělo klec nekryje)
  · roh je OZNAČENÝ, když na něj dosahuje soupeřova tacklezóna
  · ⭐ a podle doktríny (`cage_advance.cpp:35-38`) je NOSIČ V TZ větší díra než
    označený roh — měřeno 11.08.: nosič 40 %, rohy 11 % ⇒ měří se obojí

„ČISTÁ KLEC" = 4 rohy, 0 označených, nosič mimo TZ. To je stav, který doktrína
předpokládá; všechno ostatní je něco jiného, ne slabší klec.

⭐ POZITIVNÍ KONTROLA je vevnitř: kdyby 4 rohy nevyšly NIKDY, nejde poznat
  „neumíme to postavit" od „čítač je rozbitý". Proto se tiskne i nejlepší
  dosažené kolo a rozdělení pro OBĚ strany — soupeř je nezávislá kontrola.

Použití: python3 diag_cage_built_20260910.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "corpus_m11_20260909_data"
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]
DIRS = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]


def analyze(side_race_filter=None):
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    per_side = {}
    for f in files:
        g = json.load(gzip.open(f))
        for us, them in (("home", "away"), ("away", "home")):
            race = g[f"{us}_race"]
            if side_race_filter and race != side_race_filter:
                continue
            st = per_side.setdefault(race, dict(
                turns=0, corners=Counter(), marked=Counter(),
                carrier_marked=0, clean=0, best=None, ours_adjacent=Counter()))
            for t in g["turn_logs"]:
                if t["active_team"] != us:
                    continue
                mine = t[f"{us}_players"]
                theirs = t[f"{them}_players"]
                car = next((p for p in mine if p["has_ball"]), None)
                if car is None or car["state"] != 0:
                    continue
                st["turns"] += 1
                stand_ours = {(p["x"], p["y"]) for p in mine
                              if p["state"] == 0 and p["id"] != car["id"]}
                tz = {(e["x"] + dx, e["y"] + dy)
                      for e in theirs if e["state"] == 0
                      for dx, dy in DIRS}
                corners = marked = 0
                for dx, dy in DIAG:
                    q = (car["x"] + dx, car["y"] + dy)
                    if q in stand_ours:
                        corners += 1
                        if q in tz:
                            marked += 1
                cm = (car["x"], car["y"]) in tz
                st["corners"][corners] += 1
                st["marked"][marked] += 1
                st["carrier_marked"] += cm
                # kolik NAŠICH stojících vůbec sousedí s nosičem (rohy i ortogonály)
                adj = sum(1 for dx, dy in DIRS
                          if (car["x"] + dx, car["y"] + dy) in stand_ours)
                st["ours_adjacent"][adj] += 1
                if corners == 4 and marked == 0 and not cm:
                    st["clean"] += 1
                key = (corners, -marked, 0 if cm else 1)
                if st["best"] is None or key > st["best"]:
                    st["best"] = key
    return per_side


def show(race, st):
    n = st["turns"]
    if not n:
        return
    print(f"\n=== {race}: {n} tahů se STOJÍCÍM nosičem ===")
    print("  ROHY (náš stojící hráč na diagonále nosiče) -- histogram, ne průměr:")
    tot = 0
    for k in range(5):
        v = st["corners"][k]
        tot += v
        bar = "#" * int(40 * v / n)
        print(f"    {k} rohů  {v:6d}  {100*v/n:5.1f} %  {bar}")
    print(f"    ZBYTEK (musí být 0)  {n - tot}")
    avg = sum(k * st["corners"][k] for k in range(5)) / n
    print(f"    průměr {avg:.2f} ze 4  <- tohle jediné se dřív četlo")
    print("  ČISTOTA:")
    for k in range(5):
        v = st["marked"][k]
        if v:
            print(f"    {k} rohů OZNAČENÝCH (v soupeřově TZ)  {v:6d}  {100*v/n:5.1f} %")
    print(f"    NOSIČ sám v soupeřově TZ  {st['carrier_marked']:6d}  "
          f"{100*st['carrier_marked']/n:5.1f} %  <- podle doktríny větší díra než roh")
    print(f"  ⭐ ČISTÁ KLEC (4 rohy, 0 označených, nosič mimo TZ): "
          f"{st['clean']} = {100*st['clean']/n:.2f} %")
    b = st["best"]
    print(f"  pozitivní kontrola -- nejlepší dosažené kolo: {b[0]} rohů, "
          f"{-b[1]} označených, nosič {'mimo' if b[2] else 'v'} TZ")
    print("  KOLIK NAŠICH STOJÍCÍCH VŮBEC SOUSEDÍ S NOSIČEM (8 polí, ne jen rohy):")
    for k in sorted(st["ours_adjacent"]):
        v = st["ours_adjacent"][k]
        if v:
            print(f"    {k}  {v:6d}  {100*v/n:5.1f} %")


if __name__ == "__main__":
    res = analyze()
    for race in sorted(res, key=lambda r: -res[r]["turns"]):
        show(race, res[race])
