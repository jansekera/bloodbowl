#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s9 — rozklad zrcadla: ČÍM si soupeř hlídá nosiče (struktura vs pozice).

Naše kolo (snímek začátku), jejich stojící nosič; výsledek = jejich nosič
na začátku jejich kola už nestojí. Prediktory z diag_exposure_scan s Board
postaveným Z JEJICH perspektivy (BLZ = NAŠE nejlepší kostky na jejich
nosiče, REACH = kolik NAŠICH na něj dosáhne).

Rozklad "proč na orka nemáme kostky":
  POZICE    — REACH=0 (nikdo náš nedosáhne), vzdálenost našeho nejbližšího
  STRUKTURA — ST nosiče (skaven GR ST2 = 2k zadarmo), Guard clona
              (jejich stojící Guard vedle nosiče = obranné asistence)
Kanály nabídky (P15):
  - blok: jejich nosič vedle našeho stojícího; všichni naši mají Block ⇒
    1k blok se nabízí; NEnabízí se jen "do kopce" (dice<1)
  - blitz: nabízí se vždy (carrier +10), ale s BLZ=-2 s malou vahou
"""
import sys, os, glob, gzip, json
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
        opp = "away" if us == "home" else "home"
        race = game["away_race"] if us == "home" else game["home_race"]
        logs = game["turn_logs"]
        for i, t in enumerate(logs):
            if t["active_team"] != us or t["touchdown"]:
                continue
            if i + 1 >= len(logs) or logs[i + 1]["half"] != t["half"]:
                continue
            if logs[i + 1]["active_team"] == us:
                continue
            bo = ex.Board(t, opp)      # "us" = soupeř; th_st = MY
            c = bo.carrier
            if c is None or c["state"] != 0:
                continue
            P = ex.predictors(bo)
            bn = ex.Board(logs[i + 1], opp)
            nxt = {p["id"]: p for p in bn.us + bn.them}
            q = nxt.get(c["id"])
            down = 1 if (q is None or q["state"] != 0) else 0
            # struktura: jejich Guard clona u nosiče (stojící sousedé s Guard)
            g_adj = sum(1 for p in bo.neighbors_of(c, bo.us_st) if ex.guard(p))
            scr_adj = len(bo.neighbors_of(c, bo.us_st))       # jejich sousedé
            our_adj = len(bo.neighbors_of(c, bo.th_st))       # naši sousedé
            dmin = min((max(abs(p["x"] - c["x"]), abs(p["y"] - c["y"]))
                        for p in bo.th_st), default=None)
            rows.append(dict(race=race, st=c["st"], who=c["name"].split(" +")[0],
                             blz=P.get("BLZ"), reach=P.get("REACH", 0),
                             g_adj=g_adj, scr_adj=scr_adj, our_adj=our_adj,
                             dmin=dmin, down=down))

    races = sorted({r["race"] for r in rows})

    print("=== ZRCADLO: náš útok na JEJICH nosiče — plné rozdělení BLZ ===")
    print("%-8s %6s | %6s %6s %6s %6s %9s | %8s" %
          ("race", "n", "-2", "1", "2", "3", "nedosáh.", "P(↓)"))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        n = len(rs)
        cb = Counter(r["blz"] for r in rs)
        print("%-8s %6d | %5.1f%% %5.1f%% %5.1f%% %5.1f%% %8.1f%% | %8.3f" %
              (rc, n, *(100 * cb.get(v, 0) / n for v in (-2, 1, 2, 3, None)),
               sum(r["down"] for r in rs) / n))
    print()
    print("=== P(jejich nosič ↓) podle NAŠEHO BLZ (konverze, per-race) ===")
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        parts = []
        for v in (-2, 1, 2, 3, None):
            sub = [r for r in rs if r["blz"] == v]
            if len(sub) >= 30:
                parts.append("BLZ=%s: %.3f (n=%d)" %
                             (v, sum(r["down"] for r in sub) / len(sub), len(sub)))
        print("%-8s %s" % (rc, " | ".join(parts)))
    print()
    print("=== ROZKLAD: pozice vs struktura ===")
    print("%-8s %10s %12s %12s %14s %12s %12s" %
          ("race", "REACH=0", "nosič ST2", "dmin (prům.)", "kolo s adj.blok",
           "GuardCl./nosič", "clona vše"))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        n = len(rs)
        print("%-8s %9.1f%% %11.1f%% %12.2f %13.1f%% %12.2f %12.2f" %
              (rc, 100 * sum(1 for r in rs if r["reach"] == 0) / n,
               100 * sum(1 for r in rs if r["st"] == 2) / n,
               sum(r["dmin"] for r in rs if r["dmin"] is not None) / n,
               100 * sum(1 for r in rs if r["our_adj"] > 0) / n,
               sum(r["g_adj"] for r in rs) / n,
               sum(r["scr_adj"] for r in rs) / n))
    print()
    print("=== BLZ>=2 podmíněně (izolace struktury) ===")
    print("%-8s %12s %16s %22s %26s" %
          ("race", "vše", "| nosič ST2", "| ST3+ a REACH>0", "| ST3+, REACH>0, Guard=0"))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        def share(sub):
            sub = list(sub)
            if not sub:
                return "     —"
            return "%5.1f%% (n=%d)" % (
                100 * sum(1 for r in sub if isinstance(r["blz"], int) and r["blz"] >= 2) / len(sub),
                len(sub))
        print("%-8s %12s %16s %22s %26s" %
              (rc, share(rs),
               share(r for r in rs if r["st"] == 2),
               share(r for r in rs if r["st"] >= 3 and r["reach"] > 0),
               share(r for r in rs if r["st"] >= 3 and r["reach"] > 0 and r["g_adj"] == 0)))
    print()
    print("=== P15 zóna: REACH>0 a BLZ<2 (dnes se nenabízí blok, blitz jen se slabou vahou) ===")
    print("%-8s %14s %20s %20s" %
          ("race", "podíl kol", "z toho BLZ=1", "z toho BLZ=-2"))
    for rc in races:
        rs = [r for r in rows if r["race"] == rc]
        z = [r for r in rs if r["reach"] > 0 and (not isinstance(r["blz"], int) or r["blz"] < 2)]
        n1 = sum(1 for r in z if r["blz"] == 1)
        n2 = sum(1 for r in z if r["blz"] == -2)
        print("%-8s %13.1f%% %19.1f%% %19.1f%%" %
              (rc, 100 * len(z) / len(rs),
               100 * n1 / len(z) if z else 0, 100 * n2 / len(z) if z else 0))
    print()
    print("=== kdo nese, podle rasy (podíl kol; ST v závorce) ===")
    for rc in races:
        c = Counter((r["who"], r["st"]) for r in rows if r["race"] == rc)
        tot = sum(c.values())
        print("%-8s %s" % (rc, ", ".join("%s(ST%d) %d%%" % (k[0], k[1], round(100 * v / tot))
                                         for k, v in c.most_common(4))))


if __name__ == "__main__":
    main()
