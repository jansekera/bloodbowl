#!/usr/bin/env python3
"""Vykreslení konkrétní situace z korpusu k projití u stolu (11.08.2026).

Uživatel 11.08.: „zjistíme, jak by sis to hrál ty — protože chci, ať máš
co nejvíc informací — zjistíme, jak bych to hrál já jako průměrný hráč;
experta nemáme."

⇒ U každé pozice budou TŘI odpovědi: moje, uživatelova, a co doopravdy
udělal engine (ten je v korpusu). Zajímavé je přesně tam, kde se rozejdou.

Usage:
    python3 diag_situation_20260811.py <korpus> <hra> <půle> <kolo> [strana]
    python3 diag_situation_20260811.py --find <korpus>     # nabídne pozice
"""
import gzip
import json
import sys
from pathlib import Path

ROLE = {   # zkratka pro vykreslení, podle názvu pozice
    "Runner": "R", "Blitzer": "B", "Troll": "S", "Longbeard": "L",
    "Lineman": "l", "Thrower": "T", "Catcher": "C", "Gutter": "G",
    "Wardancer": "W", "Blocker": "K", "Storm": "V", "Rat": "O",
    "Black": "K", "Big": "O", "Treeman": "O", "Deathroller": "D",
}


def short(name):
    for k, v in ROLE.items():
        if name.startswith(k):
            return v
    return name[:1].upper() if name else "?"


def render(turn, us, ez_us):
    """Hřiště 26x15. NAŠI velkými písmeny, SOUPEŘ malými. Ležící v závorce."""
    them = "away" if us == "home" else "home"
    grid = [["." for _ in range(26)] for _ in range(15)]
    for p in turn[f"{us}_players"]:
        if p["state"] == 3:
            continue
        c = short(p["name"])
        grid[p["y"]][p["x"]] = c if p["state"] == 0 else c.lower()
    for p in turn[f"{them}_players"]:
        if p["state"] == 3:
            continue
        c = short(p["name"]).lower()
        grid[p["y"]][p["x"]] = c if p["state"] == 0 else ","
    bx, by = turn["ball_x"], turn["ball_y"]
    if 0 <= bx < 26 and 0 <= by < 15 and not turn["ball_held"]:
        grid[by][bx] = "*"

    out = ["     " + "".join(str(x % 10) for x in range(26))]
    for y in range(15):
        mark = "<" if ez_us == 0 else " "
        end = ">" if ez_us == 25 else " "
        out.append(f"  {y:>2} {mark}" + "".join(grid[y]) + end)
    return "\n".join(out)


def describe(corpus, game, half, turn_no, side=None):
    f = Path(corpus) / f"g{game:04d}.json.gz"
    rec = json.load(gzip.open(f, "rt"))
    us = side or ("home" if rec["home_race"] == "dwarf" else "away")
    them = "away" if us == "home" else "home"
    ez_us = 25 if us == "home" else 0

    L = rec["turn_logs"]
    idx = next((i for i, t in enumerate(L)
                if t["half"] == half and t["turn"] == turn_no
                and t["active_team"] == us), None)
    if idx is None:
        print("taková situace v záznamu není")
        return
    t = L[idx]

    print("=" * 74)
    print(f"{rec['home_race']} (home) vs {rec['away_race']} (away)   "
          f"konečné skóre {rec['home_score']}:{rec['away_score']}   seed={rec['seed']}")
    print(f"HRAJEME ZA: {us} ({rec[f'{us}_race']}), útočíme na x={ez_us}")
    print(f"PŮLE {half}, KOLO {turn_no}  ·  skóre {t['home_score']}:{t['away_score']}"
          f"  ·  počasí {t['weather']}")
    print("=" * 74)
    print(render(t, us, ez_us))
    print()

    # kdo je kdo
    print("NAŠI (velká písmena · malá = leží):")
    for p in sorted(t[f"{us}_players"], key=lambda p: (p["x"], p["y"])):
        if p["state"] == 3:
            continue
        st = {0: "stojí", 1: "leží", 2: "omráčen"}.get(p["state"], "?")
        ball = "  ← MÁ MÍČ" if p["has_ball"] else ""
        print(f"   {short(p['name'])} ({p['x']:>2},{p['y']:>2}) {p['name']:<28}"
              f" MA{p['ma']} ST{p['st']} AG{p['ag']} AV{p['av']}  {st}{ball}")
    print("SOUPEŘ (malá písmena · čárka = leží):")
    for p in sorted(t[f"{them}_players"], key=lambda p: (p["x"], p["y"])):
        if p["state"] == 3:
            continue
        st = {0: "stojí", 1: "leží", 2: "omráčen"}.get(p["state"], "?")
        ball = "  ← MÁ MÍČ" if p["has_ball"] else ""
        print(f"   {short(p['name']).lower()} ({p['x']:>2},{p['y']:>2}) {p['name']:<28}"
              f" MA{p['ma']} ST{p['st']} AG{p['ag']} AV{p['av']}  {st}{ball}")

    # rozpočet
    print()
    if t["ball_held"]:
        cid = t["ball_carrier_id"]
        mine = {p["id"] for p in t[f"{us}_players"]}
        if cid in mine:
            c = next(p for p in t[f"{us}_players"] if p["id"] == cid)
            dist = abs(c["x"] - ez_us)
            left = 9 - turn_no
            print(f"ROZPOČET: nosič {dist} polí od endzone · zbývá {left} kol"
                  f" · nutné tempo {dist/max(left,1):.2f} pole/kolo")
        else:
            print("MÍČ DRŽÍ SOUPEŘ")
    else:
        print(f"MÍČ LEŽÍ na ({t['ball_x']},{t['ball_y']})")

    # co udělal engine
    print()
    print("CO UDĚLAL ENGINE V TOMHLE KOLE:")
    ev = t.get("events", [])
    if not ev:
        print("   (nic — kolo bez událostí)")
    for e in ev:
        d = f"   {e['type']:<13}"
        if e.get("player_id", -1) >= 0:
            d += f" hráč {e['player_id']}"
        if e.get("target_id", -1) >= 0:
            d += f" → {e['target_id']}"
        if e.get("from_x", -1) >= 0:
            d += f"  ({e['from_x']},{e['from_y']})→({e['to_x']},{e['to_y']})"
        if e.get("roll"):
            d += f"  hod {e['roll']} {'✔' if e.get('success') else '✘'}"
        print(d)
    pl = t.get("plan")
    if pl and pl.get("written"):
        print(f"   [plán: cíl={pl['goal']} verdikt={pl['verdict']} "
              f"krok={pl['step']} odpor={pl['resistance']}]")


def find(corpus):
    """Nabídne zaseknuté pozice: náš míč, ADVANCE, velký odpor, málo rohů."""
    print("kandidáti (naše kolo, držíme míč, klec proti zdi):")
    n = 0
    for f in sorted(Path(corpus).glob("g*.json.gz")):
        rec = json.load(gzip.open(f, "rt"))
        us = "home" if rec["home_race"] == "dwarf" else "away"
        them = "away" if us == "home" else "home"
        ez = 25 if us == "home" else 0
        for t in rec["turn_logs"]:
            if t["active_team"] != us or not t["ball_held"]:
                continue
            mine = {p["id"] for p in t[f"{us}_players"]}
            if t["ball_carrier_id"] not in mine:
                continue
            c = next(p for p in t[f"{us}_players"] if p["id"] == t["ball_carrier_id"])
            dist = abs(c["x"] - ez)
            if dist <= c["ma"] + 2:
                continue
            foes = [p for p in t[f"{them}_players"] if p["state"] == 0]
            res = sum(1 for p in foes
                      if abs(p["x"] - ez) <= dist
                      and max(abs(p["x"] - c["x"]), abs(p["y"] - c["y"])) <= 4)
            mates = {(p["x"], p["y"]) for p in t[f"{us}_players"]
                     if p["state"] == 0 and p["id"] != c["id"]}
            corners = sum(1 for dx in (-1, 1) for dy in (-1, 1)
                          if (c["x"] + dx, c["y"] + dy) in mates)
            if res >= 4:
                print(f"   {f.name} půle {t['half']} kolo {t['turn']:>2}  "
                      f"odpor {res}  rohů {corners}  {dist} polí od endzone")
                n += 1
                if n >= 12:
                    return


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--find":
        find(sys.argv[2])
    elif len(sys.argv) >= 5:
        describe(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]),
                 sys.argv[5] if len(sys.argv) > 5 else None)
    else:
        print(__doc__)
