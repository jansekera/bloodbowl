#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s1 — per-race rozklad přijímacích drivů trpaslíka.

Reuse definic z diag_drive_failure_20260811 (import, žádné vlastní předělávky):
kategorie A/B/C/D1/D2, loss_cause, first_hold, tempo, odpor.
Přidává: per-race tabulky příčin ztrát, kola/vzdálenosti ztráty, 1. držení,
tempo, odpor; bootstrap po hrách pro klíčové rozdíly orc vs skaven.
"""
import sys, os, glob, random
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
dd = importlib.import_module("diag_drive_failure_20260811")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


def collect():
    recv = []
    anomalies = []
    per_game_td = []  # (opp, our_td, their_td) z game headeru
    for f in sorted(glob.glob(os.path.join(DATA, "g*.json.gz"))):
        game = dd.load_game(f)
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        dwarf_side = "home" if game["home_race"] == "dwarf" else "away"
        opp_race = game["away_race"] if dwarf_side == "home" else game["home_race"]
        our_sc = game["home_score"] if dwarf_side == "home" else game["away_score"]
        their_sc = game["away_score"] if dwarf_side == "home" else game["home_score"]
        per_game_td.append((opp_race, our_sc, their_sc, os.path.basename(f)))
        id_map = dd.build_id_map(game)
        drives = dd.split_drives(game["turn_logs"])
        base = os.path.basename(f).replace(".json.gz", "")
        for di, drive in enumerate(drives):
            ctx = "%s:%d" % (base, di)
            next_start = drives[di + 1][0] if di + 1 < len(drives) else None
            if dd.is_bug_drive(drive):
                continue
            rside = dd.receiving_side(drive, id_map, anomalies, ctx)
            if rside == dwarf_side:
                r = dd.analyze_receiving_drive(drive, next_start, id_map,
                                               dwarf_side, ctx, anomalies)
                r["opp"] = opp_race
                r["game"] = base
                recv.append(r)
    return recv, per_game_td


def fmt(vals):
    vals = [v for v in vals if v is not None]
    return "%.2f" % (sum(vals) / len(vals)) if vals else "—"


def main():
    recv, per_game_td = collect()
    opps = sorted(set(r["opp"] for r in recv))

    print("=== TD z headerů her (kontrola zadání) ===")
    for opp in opps:
        g = [(o, u, t) for (o, u, t, _) in per_game_td if o == opp]
        print("  %-8s hry=%d naše TD=%d jejich TD=%d" %
              (opp, len(g), sum(u for _, u, _ in g), sum(t for _, _, t in g)))
    print()

    def catkey(r):
        return r.get("subcat", r["cat"]) if r["cat"] == "D" else r["cat"]

    print("=== C DRIVY (ztráta míče): příčiny per-race — VŠECHNY drivy ===")
    for opp in opps:
        cs = [r for r in recv if r["opp"] == opp and catkey(r) == "C"]
        n = len(cs)
        causes = Counter(r.get("loss_cause") for r in cs)
        print("%-8s n=%d | ztraceno kolo %s | vzdál. od EZ %s polí" %
              (opp, n, fmt([r.get("loss_turn") for r in cs]),
               fmt([r.get("loss_dist") for r in cs])))
        for k, v in causes.most_common():
            print("    %-55s %4d (%d%%)" % (k, v, round(100 * v / n)))
    print()

    print("=== VŠECHNY přijímací drivy: 1. držení / tempo / odpor per-race ===")
    print("%-8s %6s %10s %10s %8s %8s %8s" %
          ("opp", "n", "1drž.kolo", "1drž.vzd", "tempo", "odpor", "fumbly"))
    for opp in opps:
        rs = [r for r in recv if r["opp"] == opp]
        print("%-8s %6d %10s %10s %8s %8s %8s" %
              (opp, len(rs),
               fmt([r.get("first_hold_turn") for r in rs]),
               fmt([r.get("first_hold_dist") for r in rs]),
               fmt([r.get("avg_tempo") for r in rs]),
               fmt([r.get("avg_resistance") for r in rs]),
               fmt([r.get("failed_pickups_before_hold") for r in rs])))
    print()

    print("=== PLNÉ drivy (>=7 kol): tempo a odpor per-race per-kategorie ===")
    for opp in opps:
        full = [r for r in recv if r["opp"] == opp and r["n_our_turns"] >= 7]
        for c in ("A", "C", "D1", "D2"):
            rs = [r for r in full if catkey(r) == c]
            if not rs:
                continue
            print("%-8s [%s] n=%3d tempo=%s odpor=%s 1drž.kolo=%s" %
                  (opp, c, len(rs), fmt([r.get("avg_tempo") for r in rs]),
                   fmt([r.get("avg_resistance") for r in rs]),
                   fmt([r.get("first_hold_turn") for r in rs])))
    print()

    # nosič v C drivech per-race (nese Runner i proti orkovi?)
    print("=== C drivy: kola s míčem podle pozice nosiče per-race ===")
    for opp in opps:
        car = Counter()
        for r in recv:
            if r["opp"] == opp and catkey(r) == "C":
                car.update(r.get("carrier_turns", {}))
        tot = sum(car.values())
        print("%-8s " % opp + ", ".join("%s %d%%" % (k, round(100 * v / tot))
                                        for k, v in car.most_common()) if tot else opp)
    print()

    # bootstrap po hrách: rozdíl orc–skaven v podílu C (plné drivy) a v podílu
    # příčiny 'blitz srazil nosiče' mezi C drivy
    def game_stat(rs_by_game, statfn):
        vals = []
        for g, rs in rs_by_game.items():
            v = statfn(rs)
            if v is not None:
                vals.append(v)
        return vals

    rng = random.Random(20260814)

    def boot_diff(stat_orc, stat_skv, B=2000):
        diffs = []
        for _ in range(B):
            a = [rng.choice(stat_orc) for _ in stat_orc]
            b = [rng.choice(stat_skv) for _ in stat_skv]
            diffs.append(sum(a) / len(a) - sum(b) / len(b))
        diffs.sort()
        return diffs[int(0.025 * B)], diffs[int(0.975 * B)]

    print("=== BOOTSTRAP PO HRÁCH (2000 vzorků, 95% CI) ===")
    for label, statfn, subset in [
        ("podíl C mezi plnými drivy",
         lambda rs: (sum(1 for r in rs if catkey(r) == "C") / len(rs)) if rs else None,
         lambda r: r["n_our_turns"] >= 7),
        ("podíl 'blitz srazil nosiče' mezi C",
         lambda rs: (sum(1 for r in rs if r.get("loss_cause") == "soupeřův blitz/blok srazil nosiče") / len(rs)) if rs else None,
         lambda r: catkey(r) == "C"),
    ]:
        per = {}
        for opp in ("orc", "skaven"):
            byg = defaultdict(list)
            for r in recv:
                if r["opp"] == opp and subset(r):
                    byg[r["game"]].append(r)
            per[opp] = game_stat(byg, statfn)
        mo = sum(per["orc"]) / len(per["orc"])
        ms = sum(per["skaven"]) / len(per["skaven"])
        lo, hi = boot_diff(per["orc"], per["skaven"])
        print("  %-42s orc=%.3f (n_her=%d) skaven=%.3f (n_her=%d) diff CI [%.3f, %.3f]" %
              (label, mo, len(per["orc"]), ms, len(per["skaven"]), lo, hi))


if __name__ == "__main__":
    main()
