#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s6 — je nosič v dosahu Black Orků / soupeře? (začátek soupeřova kola)

Pro každé soupeřovo kolo s naším stojícím nosičem (jako s5):
  dmin_all  = min Čebyšev vzdálenost stojícího soupeře k nosiči
  dmin_st4  = totéž jen ST>=4 (Black Orc / Ogre / Treeman)
  výsledek  = nosič už na dalším našem snímku nestojí (carrier_down)

P(carrier_down | dmin_all koš) a P(carrier_down | dmin_st4 koš) per-race.
Koše vzdálenosti: 1 (adjacent), 2-3, 4-6 (dosah MA4+2GFI), 7+.
Navíc: na kolech se sraženým nosičem — kdo byl na snímku nejblíž.
Bootstrap po hrách pro orc: P(down | dmin_all=1) vs P(down | dmin_all>=4).
"""
import sys, os, glob, gzip, json, random
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
ex = importlib.import_module("diag_exposure_scan_20260812")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


def bucket(d):
    if d is None:
        return "žádný"
    if d <= 1:
        return "1"
    if d <= 3:
        return "2-3"
    if d <= 6:
        return "4-6"
    return "7+"


BUCKETS = ["1", "2-3", "4-6", "7+", "žádný"]


def main():
    rows = []
    for f in sorted(glob.glob(os.path.join(DATA, "g*.json.gz"))):
        game = json.load(gzip.open(f, "rt"))
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        us = "home" if game["home_race"] == "dwarf" else "away"
        race = game["away_race"] if us == "home" else game["home_race"]
        logs = game["turn_logs"]
        for i, t in enumerate(logs):
            if t["active_team"] == us or t["touchdown"]:
                continue
            if i + 1 >= len(logs) or logs[i + 1]["half"] != t["half"]:
                continue
            if logs[i + 1]["active_team"] != us:
                continue
            bt = ex.Board(t, us)
            if bt.carrier is None or bt.carrier["state"] != 0:
                continue
            c = bt.carrier
            def dmin(group):
                ds = [max(abs(p["x"] - c["x"]), abs(p["y"] - c["y"])) for p in group]
                return min(ds) if ds else None
            d_all = dmin(bt.th_st)
            d_st4 = dmin([p for p in bt.th_st if p["st"] >= 4])
            bn = ex.Board(logs[i + 1], us)
            nxt = {p["id"]: p for p in bn.us + bn.them}
            q = nxt.get(c["id"])
            down = 1 if (q is None or q["state"] != 0) else 0
            rows.append(dict(race=race, game=f, d_all=d_all, d_st4=d_st4,
                             down=down,
                             ez=(25 - c["x"]) if us == "home" else c["x"]))

    races = sorted({r["race"] for r in rows})
    for key, lab in (("d_all", "nejbližší STOJÍCÍ soupeř"),
                     ("d_st4", "nejbližší stojící ST4+")):
        print("=== P(nosič↓) podle vzdálenosti: %s ===" % lab)
        print("%-8s" % "race" + "".join("%16s" % b for b in BUCKETS))
        for rc in races:
            rs = [r for r in rows if r["race"] == rc]
            line = "%-8s" % rc
            for b in BUCKETS:
                sub = [r for r in rs if bucket(r[key]) == b]
                if sub:
                    line += "%9.3f (n=%4d)" % (sum(r["down"] for r in sub) / len(sub), len(sub)) if len(sub) else ""
                else:
                    line += "%16s" % "—"
            print(line)
        print()

    print("=== ROZLOŽENÍ EXPOZICE: podíl soupeřových kol s nosičem dle koše (d_all) ===")
    print("%-8s" % "race" + "".join("%10s" % b for b in BUCKETS) + "%10s" % "n")
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        c = Counter(bucket(r["d_all"]) for r in rs)
        print("%-8s" % rc + "".join("%9.1f%%" % (100 * c.get(b, 0) / len(rs))
                                    for b in BUCKETS) + "%10d" % len(rs))
    print()
    print("=== TOTÉŽ pro ST4+ (d_st4) ===")
    print("%-8s" % "race" + "".join("%10s" % b for b in BUCKETS) + "%10s" % "n")
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        c = Counter(bucket(r["d_st4"]) for r in rs)
        print("%-8s" % rc + "".join("%9.1f%%" % (100 * c.get(b, 0) / len(rs))
                                    for b in BUCKETS) + "%10d" % len(rs))
    print()

    print("=== Vzdálenost nosiče od EZ podle koše d_all (orc) — kde stojíme, když nás bijí ===")
    rs = [r for r in rows if r["race"] == "orc"]
    for b in BUCKETS:
        sub = [r for r in rs if bucket(r["d_all"]) == b]
        if sub:
            print("  d_all %-6s n=%5d  prům. vzdál. od EZ %.1f" %
                  (b, len(sub), sum(r["ez"] for r in sub) / len(sub)))
    print()

    # bootstrap po hrách (orc): down-rate adjacent vs mimo dosah
    rng = random.Random(20260814)
    byg = defaultdict(lambda: [0, 0, 0, 0])  # game -> [n1, d1, n4, d4]
    for r in rs:
        g = byg[r["game"]]
        if bucket(r["d_all"]) == "1":
            g[0] += 1
            g[1] += r["down"]
        elif r["d_all"] is not None and r["d_all"] >= 4:
            g[2] += 1
            g[3] += r["down"]
    games = [g for g in byg.values() if g[0] > 0 or g[2] > 0]
    diffs = []
    for _ in range(2000):
        s = [games[rng.randrange(len(games))] for _ in games]
        n1 = sum(g[0] for g in s); d1 = sum(g[1] for g in s)
        n4 = sum(g[2] for g in s); d4 = sum(g[3] for g in s)
        if n1 and n4:
            diffs.append(d1 / n1 - d4 / n4)
    diffs.sort()
    n1 = sum(g[0] for g in games); d1 = sum(g[1] for g in games)
    n4 = sum(g[2] for g in games); d4 = sum(g[3] for g in games)
    print("=== BOOTSTRAP (orc, po hrách): P(down|adjacent) − P(down|d>=4) ===")
    print("  adjacent %.3f (n=%d) | d>=4 %.3f (n=%d) | diff CI [%.3f, %.3f]" %
          (d1 / n1, n1, d4 / n4, n4,
           diffs[int(0.025 * len(diffs))], diffs[int(0.975 * len(diffs))]))


if __name__ == "__main__":
    main()
