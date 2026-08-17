#!/usr/bin/env python3
"""STROP OPRAVY VÝBĚRU CÍLE FAULU (P8) — spočítat PŘED psaním kódu.

`macro_actions.cpp:792` nabídne na každého faulujícího **právě jeden** faul —
na prvního ležícího soupeře v pevném pořadí sousedních polí, a pak `return`.
Když vedle něj leží Black Ork i lineman, na menu se dostane jen jeden z nich,
vybraný geometrií mřížky. Search tedy nevybírá špatně; nedostane na výběr.

⚑ OTÁZKA: jak často je vůbec z čeho vybírat?
   Když u faulujícího leží skoro vždy jen jeden cíl, je oprava bezcenná,
   ať je heuristika jakkoli chytrá. Tohle je ten strop.

⚑ PROČ TO NEJDE ZE SNÍMKU
   Snímek je ZAČÁTEK kola, ale faul se dělá až po blocích — většina ležících
   tam v tu chvíli ještě stojí. Snímek by dal hrubě podhodnocený strop.
   ⇒ Rekonstruuje se stav v OKAMŽIKU FAULU:
     * ležící soupeři na začátku kola (ze snímku), plus
     * každý `KNOCKED_DOWN` dřív v témž kole (`from_x/from_y` = kde padl).
   V NAŠEM kole soupeř nevstává a ležícími se nehýbe, takže množina jen roste
   a pozice jsou stálé. `FOUL` nese `from_x/from_y` faulujícího ⇒ okolí známe.

⚠️ Co to NEMĚŘÍ: kolik faulů by šlo udělat jiným tělem. Měří se volba cíle
   u toho faulujícího, který faul opravdu provedl — tedy přesně to, co by
   oprava P8 změnila, nic víc.
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260814_dauntless_data"
PRONE_STATES = (1, 2)          # PRONE, STUNNED


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    setsize = Counter()        # kolik cílů bylo na výběr
    chosen_rank = Counter()    # co se z nich vybralo
    bo_available_not_taken = 0
    bo_available = 0
    fouls = 0
    games = 0
    by_race = Counter()

    for f in files:
        g = json.load(gzip.open(f))
        us = "home" if g["home_race"] == "dwarf" else "away"
        them = "away" if us == "home" else "home"
        opp_race = g[f"{them}_race"]
        games += 1

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            names = {p["id"]: p["name"] for p in t["home_players"] + t["away_players"]}
            enemy_ids = {p["id"] for p in t[f"{them}_players"]}
            # ležící soupeři na začátku kola
            prone = {p["id"]: (p["x"], p["y"])
                     for p in t[f"{them}_players"] if p["state"] in PRONE_STATES}

            for e in t.get("events", []):
                if e["type"] == "KNOCKED_DOWN":
                    pid = e.get("player_id")
                    if pid in enemy_ids:
                        prone[pid] = (e.get("from_x"), e.get("from_y"))
                elif e["type"] == "FOUL":
                    tid = e.get("target_id")
                    if tid not in enemy_ids:
                        continue
                    fx, fy = e.get("from_x"), e.get("from_y")
                    if fx is None:
                        continue
                    # množina, ze které SE DALO vybrat
                    choice = [pid for pid, (px, py) in prone.items()
                              if px is not None and cheb(fx, fy, px, py) == 1]
                    # cíl, který si engine vzal, tam patří i kdyby nám v
                    # rekonstrukci unikl (např. padl mimo naše kolo)
                    if tid not in choice:
                        choice.append(tid)
                    fouls += 1
                    setsize[len(choice)] += 1
                    by_race[opp_race] += len(choice)

                    bo = [pid for pid in choice if "Black Orc" in names.get(pid, "")]
                    if bo:
                        bo_available += 1
                        if tid not in bo:
                            bo_available_not_taken += 1
                    chosen_rank["Black Orc" if "Black Orc" in names.get(tid, "")
                                else "jiný"] += 1

    print(f"korpus: {DATA}  ·  her {games}  ·  našich faulů {fouls}")
    print("\n=== KOLIK CÍLŮ BYLO NA VÝBĚR V OKAMŽIKU FAULU ===")
    tot = sum(setsize.values())
    cum_multi = 0
    for k in sorted(setsize):
        share = 100.0 * setsize[k] / tot
        if k >= 2:
            cum_multi += setsize[k]
        print(f"  {k} cíl(ů){'':6} {setsize[k]:6d}  {share:5.1f} %")
    print(f"\n  ⭐ STROP: faulů, kde bylo z čeho vybírat (>=2 cíle): "
          f"{cum_multi} = {100.0 * cum_multi / tot:.1f} %")
    print(f"     průměrná velikost výběru: {sum(k * v for k, v in setsize.items()) / tot:.2f}")

    print("\n=== BLACK ORC ===")
    print(f"  faulů, kde byl Black Orc mezi dostupnými: {bo_available}"
          f"  ({100.0 * bo_available / tot:.1f} % faulů)")
    print(f"  z toho jsme faulli NĚKOHO JINÉHO:         {bo_available_not_taken}"
          f"  ({100.0 * bo_available_not_taken / max(1, bo_available):.1f} %)")
    print(f"  ⇒ tohle je počet faulů, které by dokonalá oprava P8 přesměrovala:"
          f" {bo_available_not_taken} za {games} her"
          f" = {bo_available_not_taken / games:.3f} na zápas")


if __name__ == "__main__":
    main()
