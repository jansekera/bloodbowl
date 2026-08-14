#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s5 — riziko nosiče per SOUPEŘOVO kolo, per-race.

Snímek t = začátek soupeřova kola s naším STOJÍCÍM nosičem; výsledek =
snímek t+1 (začátek našeho dalšího kola, táž půle, bez TD mezi tím).
Prediktory z diag_exposure_scan (Board, predictors) — REACH, REACH0, BLZ,
CCBAD, ESC, FB, FB2, MARKED — počítané na TÉMŽE snímku t (deska, kterou
soupeř dostal). Výsledky: carrier_down (nosič už nestojí), ball_lost.

Navíc per-race Pearson korelace prediktor × výsledek (hyp. 3 — doktrína
korelující proti orkovi opačně).
"""
import sys, os, glob, gzip, json, math
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
ex = importlib.import_module("diag_exposure_scan_20260812")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


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
            if t["active_team"] == us:
                continue
            if t["touchdown"]:
                continue
            if i + 1 >= len(logs) or logs[i + 1]["half"] != t["half"]:
                continue
            if logs[i + 1]["active_team"] != us:
                continue
            bt = ex.Board(t, us)
            if bt.carrier is None or bt.carrier["state"] != 0:
                continue
            bn = ex.Board(logs[i + 1], us)
            P = ex.predictors(bt)
            cid = bt.carrier["id"]
            nxt = {p["id"]: p for p in bn.us + bn.them}
            q = nxt.get(cid)
            carrier_down = 1 if (q is None or q["state"] != 0) else 0
            ball_lost = 0 if bn.carrier is not None else 1
            adjn = len(bt.neighbors_of(bt.carrier, bt.th_st))
            rows.append(dict(race=race, game=f, turn=t["turn"], adj=adjn,
                             carrier_down=carrier_down, ball_lost=ball_lost, **P))

    races = sorted({r["race"] for r in rows})
    print("=== SOUPEŘOVA KOLA S NAŠÍM STOJÍCÍM NOSIČEM (per-race) ===")
    print("%-8s %7s %12s %10s %7s %7s %7s %7s %7s %7s" %
          ("race", "n", "P(nosič↓)", "P(ztráta)", "adj", "REACH", "REACH0",
           "ESC", "CCBAD", "MARKED"))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        n = len(rs)
        def m(k, sub=rs):
            v = [r[k] for r in sub if r.get(k) is not None]
            return sum(v) / len(v) if v else float("nan")
        print("%-8s %7d %12.3f %10.3f %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f" %
              (rc, n, m("carrier_down"), m("ball_lost"), m("adj"), m("REACH"),
               m("REACH0"), m("ESC"), m("CCBAD"), m("MARKED")))
    print()
    print("=== BLZ (nejlepší kostky blitzu na nosiče) — rozdělení per-race ===")
    print("%-8s %8s | %s" % ("race", "n(BLZ)", " ".join("%7s" % v for v in
                                                        ["-2", "1", "2", "3", "None"])))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        c = Counter(r["BLZ"] for r in rs if "BLZ" in r)
        tot = sum(c.values())
        line = " ".join("%6.1f%%" % (100 * c.get(v, 0) / tot)
                        for v in (-2, 1, 2, 3, None))
        print("%-8s %8d | %s" % (rc, tot, line))
    print()
    print("=== P(nosič↓) PODMÍNĚNĚ BLZ a REACH (per-race) ===")
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        parts = []
        for v in (-2, 1, 2, 3):
            sub = [r for r in rs if r.get("BLZ") == v]
            if len(sub) >= 20:
                parts.append("BLZ=%d: %.3f (n=%d)" %
                             (v, sum(r["carrier_down"] for r in sub) / len(sub), len(sub)))
        nr = [r for r in rs if r.get("REACH") == 0]
        rr = [r for r in rs if r.get("REACH", 0) > 0]
        parts.append("REACH=0: %.3f (n=%d)" %
                     (sum(r["carrier_down"] for r in nr) / len(nr) if nr else float('nan'), len(nr)))
        parts.append("REACH>0: %.3f (n=%d)" %
                     (sum(r["carrier_down"] for r in rr) / len(rr) if rr else float('nan'), len(rr)))
        print("%-8s %s" % (rc, " | ".join(parts)))
    print()
    print("=== PER-RACE KORELACE prediktor × výsledek (Pearson, * = |r|>2/sqrt(n)) ===")
    preds = ["FB", "FB2", "MARKED", "SURF", "ESC", "REACH", "REACH0", "CCBAD", "adj"]
    for out in ("carrier_down", "ball_lost"):
        print("-- výsledek: %s --" % out)
        print("%-8s" % "" + "".join("%10s" % p for p in preds))
        for rc in races:
            rs = [r for r in rows if r["race"] == rc]
            line = "%-8s" % rc
            for p in preds:
                xy = [(r[p], r[out]) for r in rs
                      if r.get(p) is not None and r.get(out) is not None]
                r_, n = ex.pearson([a for a, _ in xy], [b for _, b in xy])
                if r_ is None:
                    line += "%10s" % "—"
                else:
                    star = "*" if abs(r_) > 2 / math.sqrt(n) else " "
                    line += "%9.3f%s" % (r_, star)
            print(line)
    print()
    # expozice: kolik soupeřových kol s nosičem za hru (více kol = více rizika)
    print("=== EXPOZICE: soupeřova kola s naším stojícím nosičem / hra ===")
    byg = defaultdict(set)
    gcount = Counter()
    for r in rows:
        gcount[r["race"]] += 1
        byg[r["race"]].add(r["game"])
    for rc in races:
        print("%-8s %.2f kol/hru (n her=%d)" %
              (rc, gcount[rc] / len(byg[rc]), len(byg[rc])))


if __name__ == "__main__":
    main()
