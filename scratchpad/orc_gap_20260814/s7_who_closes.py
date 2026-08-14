#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s7 — kdo zavírá vzdálenost nosič↔soupeř: my, nebo oni?

Trojice snímků: [náš tah t] -> [jejich tah] -> [náš tah t+1], táž půle, bez TD,
náš nosič stojí na všech třech. dmin = min Čebyšev vzdálenost stojícího
soupeře (a zvlášť ST4+) k nosiči.

  delta_our  = dmin(jejich snímek) − dmin(náš snímek t)      … co udělal NÁŠ tah
  delta_their= dmin(náš snímek t+1) − dmin(jejich snímek)    … co udělal JEJICH tah

Záporné = vzdálenost se zavřela. Rozpad podle výchozí dmin (naše tahy,
kdy jsme ZAČÍNALI mimo kontakt, d>=4: vlezli jsme si do dosahu sami?).
"""
import sys, os, glob, gzip, json
from collections import defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
ex = importlib.import_module("diag_exposure_scan_20260812")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


def dmin(board, st4=False):
    c = board.carrier
    grp = [p for p in board.th_st if (p["st"] >= 4 if st4 else True)]
    ds = [max(abs(p["x"] - c["x"]), abs(p["y"] - c["y"])) for p in grp]
    return min(ds) if ds else None


def main():
    agg = defaultdict(lambda: [0.0, 0])   # (race, 'our'/'their', bucket, st4) -> [sum, n]
    into = defaultdict(lambda: [0, 0])    # (race, st4): z d>=4 jsme SAMI skončili <=3

    for f in sorted(glob.glob(os.path.join(DATA, "g*.json.gz"))):
        game = json.load(gzip.open(f, "rt"))
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        us = "home" if game["home_race"] == "dwarf" else "away"
        race = game["away_race"] if us == "home" else game["home_race"]
        logs = game["turn_logs"]
        for i in range(len(logs) - 2):
            t0, t1, t2 = logs[i], logs[i + 1], logs[i + 2]
            if t0["active_team"] != us or t1["active_team"] == us:
                continue
            if t0["touchdown"] or t1["touchdown"]:
                continue
            if t1["half"] != t0["half"] or t2["half"] != t0["half"]:
                continue
            b0, b1, b2 = (ex.Board(t, us) for t in (t0, t1, t2))
            if any(b.carrier is None or b.carrier["state"] != 0 for b in (b0, b1, b2)):
                continue
            for st4 in (False, True):
                d0, d1, d2 = dmin(b0, st4), dmin(b1, st4), dmin(b2, st4)
                if None in (d0, d1, d2):
                    continue
                bck = "start<=3" if d0 <= 3 else "start>=4"
                agg[(race, "our", bck, st4)][0] += d1 - d0
                agg[(race, "our", bck, st4)][1] += 1
                agg[(race, "their", bck, st4)][0] += d2 - d1
                agg[(race, "their", bck, st4)][1] += 1
                if d0 >= 4:
                    into[(race, st4)][1] += 1
                    if d1 <= 3:
                        into[(race, st4)][0] += 1

    races = sorted({k[0] for k in agg})
    for st4, lab in ((False, "VŠICHNI stojící soupeři"), (True, "jen ST4+")):
        print("=== Δ dmin (%s): záporné = zavřeno ===" % lab)
        print("%-8s %14s %14s %14s %14s" %
              ("race", "náš(≤3)", "jejich(≤3)", "náš(≥4)", "jejich(≥4)"))
        for rc in races:
            row = "%-8s" % rc
            for bck in ("start<=3", "start>=4"):
                for side in ("our", "their"):
                    s, n = agg[(rc, side, bck, st4)]
                    row += "%9.2f(%4d)" % (s / n, n) if n else "%14s" % "—"
            print(row)
        print()
    print("=== Z d>=4 jsme PO SVÉM tahu <=3 (sami do kontaktu) ===")
    for rc in races:
        for st4 in (False, True):
            a, n = into[(rc, st4)]
            if n:
                print("%-8s %-6s %4d z %4d (%.1f%%)" %
                      (rc, "ST4+" if st4 else "vše", a, n, 100 * a / n))


if __name__ == "__main__":
    main()
