#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
diag_fable_drives_20260826.py — čím se liší přijímací drivy trpaslíka, které daly TD (A),
od těch, kterým došla kola (D1/D2).

Staví na diag_drive_failure_20260811.py (dělení na drivy, kategorie A/B/C/D1/D2,
definice prvního držení = první START-OF-TURN snímek s trpaslíkem u míče).

Přidává:
  (1) rozklad tahů PŘED prvním držením — co se v nich s míčem dělo
      (pokus o pickup / žádný pokus, dosah k míči, sebrán a hned ztracen,
      míč držel soupeř, míč mimo hřiště)
  (2) tempo po TAZÍCH, ne průměr za drive — rozdělení postupu nosiče v jednom
      našem tahu, a u každého tahu stav nosiče na startu (tacklezóny, prone,
      MA, kolik kroků skutečně udělal, jestli byl sražen)
  (3) start drivu: kam míč dopadl a jak daleko od něj stál nejbližší trpaslík

Pouze čte data. Pouštět pod nice -19.
"""
import argparse
import glob
import os
import sys
import time
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diag_drive_failure_20260811 as base  # noqa: E402


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def players_of(tl, side):
    return tl[side + "_players"]


def find_player(tl, pid):
    for side in ("home", "away"):
        for p in tl[side + "_players"]:
            if p["id"] == pid:
                return side, p
    return None, None


def adj_opp_standing(tl, x, y, opp_side):
    return sum(1 for p in tl[opp_side + "_players"]
               if p["state"] == 0 and 0 <= p["x"] <= 25 and cheb(p["x"], p["y"], x, y) == 1)


def pre_hold_turn_class(tl, nxt, id_map, dwarf_side, opp_side):
    """Klasifikace jednoho NAŠEHO tahu před prvním držením."""
    bx, by = tl["ball_x"], tl["ball_y"]
    if tl.get("ball_held") and base.holder_side(tl, id_map) == opp_side:
        return "míč drží SOUPEŘ na startu tahu", None
    if bx < 0 or by < 0:
        return "míč MIMO HŘIŠTĚ na startu tahu", None
    # dosah: nejbližší stojící trpaslík k míči (Chebyshev), a jeho MA
    best = None
    for p in tl[dwarf_side + "_players"]:
        if p["state"] != 0 or not (0 <= p["x"] <= 25):
            continue
        d = cheb(p["x"], p["y"], bx, by)
        if best is None or d < best[0]:
            best = (d, p["ma"])
    ev = tl["events"]
    our_pick = [e for e in ev if e["type"] == "PICKUP" and id_map.get(e["player_id"], (None,))[0] == dwarf_side]
    if our_pick:
        ok = [e for e in our_pick if e["success"]]
        if ok and nxt is not None and base.holder_side(nxt, id_map) == dwarf_side:
            return "SEBRÁN v tomto tahu (normální tah sebrání)", best
        if ok:
            # sebrán, ale na dalším snímku nedržíme -> ztracen ještě v našem tahu
            pid = ok[-1]["player_id"]
            why = "sebrán a ztracen v témž tahu: "
            after = ev[ev.index(ok[-1]) + 1:]
            for e in after:
                if e["type"] == "KNOCKED_DOWN" and e["player_id"] == pid:
                    why += "nosič SRAŽEN (blok v našem tahu)"
                    break
                if e["type"] in ("DODGE", "GFI") and e["player_id"] == pid and not e["success"]:
                    why += "nosič spadl na %s" % e["type"]
                    break
                if e["type"] == "PASS" and e["player_id"] == pid:
                    why += "přihrávka"
                    break
            else:
                why += "nejasné"
            return why, best
        return "pokus o PICKUP NEÚSPĚŠNÝ", best
    # žádný pokus
    if best is None:
        return "žádný pokus, nikdo nestojí", best
    d, ma = best
    if d - 1 <= ma:
        return "žádný pokus, míč V DOSAHU MA", best
    if d - 1 <= ma + 2:
        return "žádný pokus, míč v dosahu jen s GFI", best
    return "žádný pokus, míč MIMO DOSAH", best


def turn_detail(k, drive, id_map, dwarf_side, opp_side):
    """Detail jednoho našeho tahu s držením: postup, stav nosiče na startu, co dělal."""
    tl = drive[k]
    cid = tl["ball_carrier_id"]
    side, car = find_player(tl, cid)
    if car is None:
        return None
    d0 = base.dist_to_endzone(dwarf_side, tl["ball_x"])
    nxt = drive[k + 1] if k + 1 < len(drive) else None
    det = {"turn": tl["turn"], "d0": d0, "ma": car["ma"], "prone": car["state"] != 0,
           "tz": adj_opp_standing(tl, car["x"], car["y"], opp_side),
           "carrier": base.position_of(id_map[cid][1]) if cid in id_map else "?"}
    ev = tl["events"]
    steps = sum(1 for e in ev if e["type"] == "MOVE" and e["player_id"] == cid and e["success"])
    det["steps"] = steps
    det["gfi"] = sum(1 for e in ev if e["type"] == "GFI" and e["player_id"] == cid)
    det["dodge"] = sum(1 for e in ev if e["type"] == "DODGE" and e["player_id"] == cid)
    det["kd_own"] = any(e["type"] == "KNOCKED_DOWN" and e["player_id"] == cid for e in ev)
    det["handoff"] = any(e["type"] in ("CATCH", "PASS") for e in ev)
    det["turnover"] = any(e["type"] == "TURNOVER" for e in ev)
    # index prvního eventu nosiče mezi eventy tahu (jak brzy v tahu se nosič hýbe)
    first_car = next((i for i, e in enumerate(ev) if e["player_id"] == cid), None)
    det["carrier_event_rank"] = first_car
    det["n_events"] = len(ev)
    to_idx = next((i for i, e in enumerate(ev) if e["type"] == "TURNOVER"), None)
    det["turnover_before_carrier"] = to_idx is not None and (first_car is None or to_idx < first_car)
    det["carrier_idle"] = first_car is None
    # volná pole vpřed (blíž k endzone) na startu tahu
    occ = set((p["x"], p["y"]) for sd in ("home", "away") for p in tl[sd + "_players"] if 0 <= p["x"] <= 25 and p["state"] != 3)
    fx = 1 if dwarf_side == "home" else -1
    fwd, safe = 0, 0
    for dy in (-1, 0, 1):
        nx, ny = car["x"] + fx, car["y"] + dy
        if 0 <= nx <= 25 and 0 <= ny <= 14 and (nx, ny) not in occ:
            fwd += 1
            if adj_opp_standing(tl, nx, ny, opp_side) == 0:
                safe += 1
    det["fwd_free"], det["fwd_safe"] = fwd, safe
    own_occ = set((p["x"], p["y"]) for p in tl[dwarf_side + "_players"] if 0 <= p["x"] <= 25 and p["state"] != 3)
    det["fwd_blocked_by_own"] = sum(1 for dy in (-1, 0, 1) if (car["x"] + fx, car["y"] + dy) in own_occ)
    det["carrier_events"] = tuple(sorted(set(e["type"] for e in ev if e["player_id"] == cid)))
    # odpor v koridoru na startu tahu + nejlepší alternativní pás
    det["res"] = base.corridor_resistance(tl, cid, id_map, dwarf_side)
    lo, hi = (car["x"], 25) if dwarf_side == "home" else (0, car["x"])
    lanes = []
    for cy in range(2, 13):
        lanes.append(sum(1 for p in tl[opp_side + "_players"] if p["state"] == 0 and lo < p["x"] < hi and abs(p["y"] - cy) <= 2))
    det["res_min_lane"] = min(lanes)
    det["res_total_ahead"] = sum(1 for p in tl[opp_side + "_players"] if p["state"] == 0 and lo < p["x"] < hi)
    det["opp_standing"] = sum(1 for p in tl[opp_side + "_players"] if p["state"] == 0 and 0 <= p["x"] <= 25)
    det["own_standing"] = sum(1 for p in tl[dwarf_side + "_players"] if p["state"] == 0 and 0 <= p["x"] <= 25)
    # postup: podle dalšího snímku, jinak z posledního kroku nosiče
    if nxt is not None and base.holder_side(nxt, id_map) == dwarf_side:
        det["adv"] = d0 - base.dist_to_endzone(dwarf_side, nxt["ball_x"])
        det["kept"] = True
        # sražen v soupeřově tahu? (KNOCKED_DOWN na nosiče v následujícím snímku events)
        ncid = nxt["ball_carrier_id"]
        det["kd_opp"] = any(e["type"] == "KNOCKED_DOWN" and e["player_id"] == ncid for e in nxt["events"])
    else:
        last = None
        for e in ev:
            if e["type"] in ("MOVE", "GFI") and e["player_id"] == cid and e["success"]:
                last = e
        det["adv"] = (d0 - base.dist_to_endzone(dwarf_side, last["to_x"])) if last else 0
        det["kept"] = False
        det["kd_opp"] = None
    return det


def analyze(drive, next_start, id_map, dwarf_side, ctx, anomalies):
    r = base.analyze_receiving_drive(drive, next_start, id_map, dwarf_side, ctx, anomalies)
    opp_side = "away" if dwarf_side == "home" else "home"
    s0 = drive[0]
    # start drivu
    bx, by = s0["ball_x"], s0["ball_y"]
    r["ball_off_pitch_start"] = bx < 0 or by < 0
    r["ball_start_y"] = by
    if not r["ball_off_pitch_start"]:
        ds = [cheb(p["x"], p["y"], bx, by) for p in s0[dwarf_side + "_players"]
              if p["state"] == 0 and 0 <= p["x"] <= 25]
        r["nearest_dwarf_to_ball"] = min(ds) if ds else None
        do = [cheb(p["x"], p["y"], bx, by) for p in s0[opp_side + "_players"]
              if p["state"] == 0 and 0 <= p["x"] <= 25]
        r["nearest_opp_to_ball"] = min(do) if do else None
        r["opp_in_our_half_start"] = sum(1 for p in s0[opp_side + "_players"]
                                         if 0 <= p["x"] <= 25 and
                                         ((dwarf_side == "home" and p["x"] <= 12) or (dwarf_side == "away" and p["x"] >= 13)))
    held_snaps = [k for k, tl in enumerate(drive) if base.holder_side(tl, id_map) == dwarf_side]
    j = held_snaps[0] if held_snaps else len(drive)
    # tahy před prvním držením
    pre = []
    for k, tl in enumerate(drive[:j]):
        if tl["active_team"] != dwarf_side:
            continue
        nxt = drive[k + 1] if k + 1 < len(drive) else None
        cls, best = pre_hold_turn_class(tl, nxt, id_map, dwarf_side, opp_side)
        pre.append({"turn": tl["turn"], "cls": cls, "reach": best,
                    "turnover": any(e["type"] == "TURNOVER" for e in tl["events"])})
    r["pre_hold"] = pre
    r["opp_held_before_us"] = any(base.holder_side(tl, id_map) == opp_side for tl in drive[:j])
    # per-turn detail držení
    dets = []
    for k, tl in enumerate(drive):
        if tl["active_team"] == dwarf_side and base.holder_side(tl, id_map) == dwarf_side:
            d = turn_detail(k, drive, id_map, dwarf_side, opp_side)
            if d:
                dets.append(d)
    r["turns"] = dets
    tdk = [tl["turn"] for tl in drive if tl["touchdown"] and base.td_scorer_side(tl, id_map) == dwarf_side]
    if tdk:
        r["td_turn"] = tdk[0]
        if "first_hold_turn" in r:
            used = tdk[0] - r["first_hold_turn"] + 1
            r["tempo_incl_td"] = r["first_hold_dist"] / used if used > 0 else None
            r["turns_used"] = used
    return r


def catkey(r):
    return r.get("subcat", r["cat"]) if r["cat"] == "D" else r["cat"]


def mean(v):
    v = [x for x in v if x is not None]
    return sum(v) / len(v) if v else float("nan")


def hist(vals, bins):
    c = Counter()
    for v in vals:
        for lo, hi, lab in bins:
            if lo <= v < hi:
                c[lab] += 1
                break
    return c


def pct_line(c, order, n):
    return " | ".join("%s %d (%d%%)" % (lab, c.get(lab, 0), round(100 * c.get(lab, 0) / n)) for lab in order) if n else "—"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir")
    ap.add_argument("--limit", type=int, default=0, help="jen prvních N souborů (měření tempa)")
    ap.add_argument("--dump-prehold", type=int, default=0, help="vypiš N D-drivů s 1. držením >= kolo 3")
    ap.add_argument("--full", action="store_true", help="jen plné drivy (start v kole 1)")
    ap.add_argument("--race", default="dwarf", help="sledovaná rasa (default dwarf)")
    args = ap.parse_args()
    t0 = time.time()
    files = sorted(glob.glob(os.path.join(args.data_dir, "g*.json.gz")) + glob.glob(os.path.join(args.data_dir, "*", "g*.json.gz")))
    if args.limit:
        files = files[:args.limit]
    anomalies, recv = [], []
    n_games = 0
    for f in files:
        game = base.load_game(f)
        if args.race not in (game["home_race"], game["away_race"]):
            continue
        if game["home_race"] == game["away_race"] and args.race != "dwarf":
            continue
        n_games += 1
        dwarf_side = "home" if game["home_race"] == args.race else "away"
        opp_race = game["away_race"] if dwarf_side == "home" else game["home_race"]
        id_map = base.build_id_map(game)
        drives = base.split_drives(game["turn_logs"])
        b = os.path.join(os.path.basename(os.path.dirname(f)), os.path.basename(f).replace(".json.gz", ""))
        for di, drive in enumerate(drives):
            if base.is_bug_drive(drive):
                continue
            ctx = "%s:%d" % (b, di)
            next_start = drives[di + 1][0] if di + 1 < len(drives) else None
            if base.receiving_side(drive, id_map, anomalies, ctx) == dwarf_side:
                r = analyze(drive, next_start, id_map, dwarf_side, ctx, anomalies)
                r["opp"] = opp_race
                recv.append(r)
    el = time.time() - t0
    print("Soubory %d | trpasličích her %d | přijímacích drivů %d | čas %.1fs (%.1f ms/hra)"
          % (len(files), n_games, len(recv), el, 1000 * el / max(1, len(files))))
    cc = Counter(catkey(r) for r in recv)
    print("Kategorie:", dict(cc))
    if args.full:
        recv = [r for r in recv if r["start_turn"] == 1]
        print("JEN PLNÉ DRIVY (start v kole 1):", dict(Counter(catkey(r) for r in recv)))
    print()

    A = [r for r in recv if r["cat"] == "A"]
    D = [r for r in recv if r["cat"] == "D"]
    D1 = [r for r in D if r.get("subcat") == "D1"]
    D2 = [r for r in D if r.get("subcat") == "D2"]
    groups = [("A", A), ("D (vše)", D), ("D1", D1), ("D2", D2)]

    # ---------- 0. start drivu ----------
    print("=== 0. START DRIVU (snímek po výkopu) ===")
    for name, g in groups:
        if not g:
            continue
        print("[%s] n=%d | míč mimo hřiště %d (%d%%) | nejbližší trpaslík k míči %.2f | nejbližší soupeř k míči %.2f | "
              "soupeřů v naší půli %.2f | vzdál. míče od endzone %.2f | y míče %.2f"
              % (name, len(g), sum(1 for r in g if r["ball_off_pitch_start"]),
                 round(100 * sum(1 for r in g if r["ball_off_pitch_start"]) / len(g)),
                 mean([r.get("nearest_dwarf_to_ball") for r in g]),
                 mean([r.get("nearest_opp_to_ball") for r in g]),
                 mean([r.get("opp_in_our_half_start") for r in g]),
                 mean([r["kickoff_dist"] for r in g if not r["ball_off_pitch_start"]]),
                 mean([r["ball_start_y"] for r in g if not r["ball_off_pitch_start"]])))
        nd = hist([r["nearest_dwarf_to_ball"] for r in g if r.get("nearest_dwarf_to_ball") is not None],
                  [(0, 2, "≤1"), (2, 4, "2-3"), (4, 6, "4-5"), (6, 8, "6-7"), (8, 99, "8+")])
        print("    nejbližší trpaslík k míči: " + pct_line(nd, ["≤1", "2-3", "4-5", "6-7", "8+"], len(g)))
    print()

    # ---------- 1. první držení ----------
    print("=== 1. PRVNÍ DRŽENÍ — rozdělení kola (start-of-turn snímek) ===")
    for name, g in groups:
        if not g:
            continue
        fh = Counter(r.get("first_hold_turn", "nikdy") for r in g)
        print("[%s] n=%d | " % (name, len(g)) + " ".join("k%s:%d" % (k, fh[k]) for k in sorted(fh, key=str)))
        print("    soupeř držel míč dřív než my: %d (%d%%) | start drivu v kole %s"
              % (sum(1 for r in g if r["opp_held_before_us"]),
                 round(100 * sum(1 for r in g if r["opp_held_before_us"]) / len(g)),
                 "%.2f" % mean([r["start_turn"] for r in g])))
    print()
    print("=== 1b. CO SE DĚLO V NAŠICH TAZÍCH PŘED PRVNÍM DRŽENÍM (jmenovatel = tahy) ===")
    for name, g in groups:
        if not g:
            continue
        cls = Counter()
        n_turns = 0
        for r in g:
            for p in r["pre_hold"]:
                cls[p["cls"]] += 1
                n_turns += 1
        print("[%s] tahů před 1. držením: %d (%.2f/drive)" % (name, n_turns, n_turns / len(g)))
        for k, v in cls.most_common():
            print("    %-55s %5d (%d%%)" % (k, v, round(100 * v / max(1, n_turns))))
    print()
    print("=== 1c. TOTÉŽ PO DRIVECH: klasifikace PRVNÍHO našeho tahu bez držení (jmenovatel = drivy s aspoň 1 takovým tahem) ===")
    for name, g in groups:
        gg = [r for r in g if r["pre_hold"]]
        if not gg:
            continue
        cls = Counter(r["pre_hold"][0]["cls"] for r in gg)
        print("[%s] drivů: %d z %d" % (name, len(gg), len(g)))
        for k, v in cls.most_common():
            print("    %-55s %5d (%d%%)" % (k, v, round(100 * v / len(gg))))
    print()
    # D-drivy s pozdním držením: řetězec tříd
    print("=== 1d. ŘETĚZCE tříd tahů před 1. držením u D-drivů s 1. držením >= kolo 3 (top 15) ===")
    late = [r for r in D if r.get("first_hold_turn", 99) >= 3]
    chains = Counter(" → ".join(p["cls"] for p in r["pre_hold"]) for r in late)
    print("D-drivů s 1. držením >= kolo 3: %d z %d" % (len(late), len(D)))
    for k, v in chains.most_common(15):
        print("  %4d  %s" % (v, k))
    print()
    if args.dump_prehold:
        print("=== ukázky (ctx) ===")
        for r in late[:args.dump_prehold]:
            print(r["ctx"], r["opp"], "1.drž=k%s" % r.get("first_hold_turn"),
                  [(p["turn"], p["cls"], p["reach"], "TO" if p["turnover"] else "") for p in r["pre_hold"]])
        print()

    # ---------- 2. tempo po tazích ----------
    print("=== 2. TEMPO PO TAZÍCH (postup nosiče v jednom našem tahu; jmenovatel = naše tahy s držením) ===")
    bins = [(-99, 0, "<0"), (0, 1, "0"), (1, 2, "1"), (2, 3, "2"), (3, 4, "3"), (4, 5, "4"), (5, 6, "5"), (6, 99, "6+")]
    order = [b[2] for b in bins]
    for name, g in groups:
        ts = [t for r in g for t in r["turns"]]
        if not ts:
            continue
        h = hist([t["adv"] for t in ts], bins)
        print("[%s] tahů %d | průměr %.2f | " % (name, len(ts), mean([t["adv"] for t in ts])) + pct_line(h, order, len(ts)))
        # bez posledního tahu (TD tah zkresluje) — pouze tahy, kde jsme míč udrželi
        kept = [t for t in ts if t["kept"]]
        h2 = hist([t["adv"] for t in kept], bins)
        print("    jen tahy s udrženým míčem do dalšího snímku: n=%d průměr %.2f | " % (len(kept), mean([t["adv"] for t in kept])) + pct_line(h2, order, len(kept)))
    print()
    print("=== 2b. PRŮMĚR TEMPA ZA DRIVE — rozdělení (prahová hypotéza: 2,86 vs 2,02, nic mezi?) ===")
    dbins = [(-99, 1, "<1"), (1, 1.5, "1-1.5"), (1.5, 2, "1.5-2"), (2, 2.5, "2-2.5"), (2.5, 3, "2.5-3"), (3, 3.5, "3-3.5"), (3.5, 4, "3.5-4"), (4, 99, "4+")]
    dorder = [b[2] for b in dbins]
    for name, g in groups:
        v = [r["avg_tempo"] for r in g if r.get("avg_tempo") is not None]
        if not v:
            continue
        h = hist(v, dbins)
        print("[%s] n=%d průměr %.2f | " % (name, len(v), mean(v)) + pct_line(h, dorder, len(v)))
    print()
    print("=== 2c. TEMPO PODLE KOLA DRIVU (průměrný postup v našem tahu k, jen tahy s udrženým míčem) ===")
    for name, g in groups:
        by_turn = defaultdict(list)
        for r in g:
            for t in r["turns"]:
                if t["kept"]:
                    by_turn[t["turn"]].append(t["adv"])
        if by_turn:
            print("[%s] " % name + "  ".join("k%d:%.2f(n%d)" % (k, mean(v), len(v)) for k, v in sorted(by_turn.items())))
    print()

    # ---------- 3. rozklad tahu: proč 0 / málo ----------
    print("=== 3. STAV NOSIČE NA STARTU TAHU vs POSTUP (jmenovatel = naše tahy s držením, bez TD tahu) ===")
    for name, g in groups:
        ts = [t for r in g for t in r["turns"] if t["kept"]]
        if not ts:
            continue
        print("[%s] n=%d" % (name, len(ts)))
        print("    nosič PRONE na startu: %d (%d%%) | v tacklezóně (>=1): %d (%d%%) | v >=2 TZ: %d (%d%%) | MA nosiče %.2f | %s"
              % (sum(1 for t in ts if t["prone"]), round(100 * sum(1 for t in ts if t["prone"]) / len(ts)),
                 sum(1 for t in ts if t["tz"] >= 1), round(100 * sum(1 for t in ts if t["tz"] >= 1) / len(ts)),
                 sum(1 for t in ts if t["tz"] >= 2), round(100 * sum(1 for t in ts if t["tz"] >= 2) / len(ts)),
                 mean([t["ma"] for t in ts]),
                 ", ".join("%s %d%%" % (k, round(100 * v / len(ts))) for k, v in Counter(t["carrier"] for t in ts).most_common(3))))
        print("    kroků nosiče/tah %.2f | tahů s 0 kroky: %d (%d%%) | s 0 kroky a VOLNÝ (ne prone, 0 TZ): %d (%d%%) | dodge/tah %.2f | GFI/tah %.2f | handoff-tahy %d%% | sražen v soupeřově tahu %d%%"
              % (mean([t["steps"] for t in ts]),
                 sum(1 for t in ts if t["steps"] == 0), round(100 * sum(1 for t in ts if t["steps"] == 0) / len(ts)),
                 sum(1 for t in ts if t["steps"] == 0 and not t["prone"] and t["tz"] == 0),
                 round(100 * sum(1 for t in ts if t["steps"] == 0 and not t["prone"] and t["tz"] == 0) / len(ts)),
                 mean([t["dodge"] for t in ts]), mean([t["gfi"] for t in ts]),
                 round(100 * sum(1 for t in ts if t["handoff"]) / len(ts)),
                 round(100 * sum(1 for t in ts if t["kd_opp"]) / len(ts))))
        # postup podle stavu na startu
        for lab, pred in (("volný (0 TZ, stojí)", lambda t: t["tz"] == 0 and not t["prone"]),
                          ("1 TZ, stojí", lambda t: t["tz"] == 1 and not t["prone"]),
                          (">=2 TZ, stojí", lambda t: t["tz"] >= 2 and not t["prone"]),
                          ("prone", lambda t: t["prone"])):
            s = [t for t in ts if pred(t)]
            if s:
                h = hist([t["adv"] for t in s], bins)
                print("    %-22s n=%4d postup %.2f kroků %.2f | " % (lab, len(s), mean([t["adv"] for t in s]), mean([t["steps"] for t in s])) + pct_line(h, order, len(s)))
        # postup u volného nosiče podle kola
        free = defaultdict(list)
        for t in ts:
            if t["tz"] == 0 and not t["prone"]:
                free[t["turn"]].append(t["adv"])
        print("    volný nosič podle kola: " + "  ".join("k%d:%.2f(n%d)" % (k, mean(v), len(v)) for k, v in sorted(free.items())))
    print()

    # ---------- 3b. tahy, kde nosič neudělal krok ----------
    print("=== 3b. TAHY S 0 KROKY NOSIČE — proč? (jmenovatel = tahy s 0 kroky, míč udržen) ===")
    for name, g in groups:
        z = [t for r in g for t in r["turns"] if t["kept"] and t["steps"] == 0]
        if not z:
            continue
        n = len(z)
        print("[%s] n=%d (z %d tahů) | turnover DŘÍV než se nosič pohnul: %d (%d%%) | nosič bez jediné události: %d (%d%%) | turnover v tahu vůbec: %d (%d%%)"
              % (name, n, sum(1 for r in g for t in r["turns"] if t["kept"]),
                 sum(1 for t in z if t["turnover_before_carrier"]), round(100 * sum(1 for t in z if t["turnover_before_carrier"]) / n),
                 sum(1 for t in z if t["carrier_idle"]), round(100 * sum(1 for t in z if t["carrier_idle"]) / n),
                 sum(1 for t in z if t["turnover"]), round(100 * sum(1 for t in z if t["turnover"]) / n)))
        free0 = [t for t in z if t["tz"] == 0 and not t["turnover_before_carrier"]]
        print("    volný nosič, bez předčasného turnoveru: n=%d | mělo volné pole vpřed: %d (%d%%) | mělo BEZPEČNÉ pole vpřed (0 TZ): %d (%d%%) | podle kola: %s"
              % (len(free0),
                 sum(1 for t in free0 if t["fwd_free"] > 0), round(100 * sum(1 for t in free0 if t["fwd_free"] > 0) / max(1, len(free0))),
                 sum(1 for t in free0 if t["fwd_safe"] > 0), round(100 * sum(1 for t in free0 if t["fwd_safe"] > 0) / max(1, len(free0))),
                 " ".join("k%d:%d" % (k, v) for k, v in sorted(Counter(t["turn"] for t in free0).items()))))
        tz0 = [t for t in z if t["tz"] >= 1 and not t["turnover_before_carrier"]]
        print("    nosič v TZ, bez předčasného turnoveru: n=%d | mělo volné pole vpřed: %d (%d%%) | mělo BEZPEČNÉ pole vpřed: %d (%d%%)"
              % (len(tz0),
                 sum(1 for t in tz0 if t["fwd_free"] > 0), round(100 * sum(1 for t in tz0 if t["fwd_free"] > 0) / max(1, len(tz0))),
                 sum(1 for t in tz0 if t["fwd_safe"] > 0), round(100 * sum(1 for t in tz0 if t["fwd_safe"] > 0) / max(1, len(tz0)))))
        z0 = [t for t in z if not t["carrier_idle"]]
        print("    nosič s událostí, ale 0 kroků (n=%d): %s" % (len(z0), ", ".join("%s %d%%" % ("+".join(k), round(100 * v / max(1, len(z0)))) for k, v in Counter(t["carrier_events"] for t in z0).most_common(6))))
        nf = [t for t in free0 if t["fwd_free"] == 0]
        print("    volný nosič bez volného pole vpřed (n=%d): všechna 3 pole vpřed obsazena NAŠIMI: %d (%d%%) | mimo hřiště/okraj: %d"
              % (len(nf), sum(1 for t in nf if t["fwd_blocked_by_own"] == 3), round(100 * sum(1 for t in nf if t["fwd_blocked_by_own"] == 3) / max(1, len(nf))),
                 sum(1 for t in nf if t["fwd_blocked_by_own"] < 3)))
    print()
    print("=== 3d. POSTUP VE STEJNÉM STAVU: volný nosič + BEZPEČNÉ pole vpřed + bez předčasného turnoveru ===")
    for name, g in groups:
        s = [t for r in g for t in r["turns"] if t["kept"] and t["tz"] == 0 and t["fwd_safe"] > 0 and not t["turnover_before_carrier"]]
        if s:
            h = hist([t["adv"] for t in s], bins)
            print("[%s] n=%d postup %.2f kroků %.2f | " % (name, len(s), mean([t["adv"] for t in s]), mean([t["steps"] for t in s])) + pct_line(h, order, len(s)))
            byk = defaultdict(list)
            for t in s:
                byk[t["turn"]].append(t["adv"])
            print("    podle kola: " + "  ".join("k%d:%.2f(n%d)" % (k, mean(v), len(v)) for k, v in sorted(byk.items())))
    print()
    print("=== 3e. ROZKLAD ROZDÍLU A vs D1 v postupu na tah (tahy s udrženým míčem) ===")
    if A and D1:
        ta = [t for r in A for t in r["turns"] if t["kept"]]
        td = [t for r in D1 for t in r["turns"] if t["kept"]]
        def st(t):
            if t["turnover_before_carrier"]:
                return "turnover před nosičem"
            if t["tz"] == 0:
                return "volný"
            if t["tz"] == 1:
                return "1 TZ"
            return ">=2 TZ"
        states = ["volný", "1 TZ", ">=2 TZ", "turnover před nosičem"]
        ma_, md_ = mean([t["adv"] for t in ta]), mean([t["adv"] for t in td])
        print("A %.2f vs D1 %.2f, rozdíl %.2f pole/tah" % (ma_, md_, ma_ - md_))
        tot_mix, tot_beh = 0.0, 0.0
        for sname in states:
            sa = [t["adv"] for t in ta if st(t) == sname]
            sd = [t["adv"] for t in td if st(t) == sname]
            pa, pd = len(sa) / len(ta), len(sd) / len(td)
            ea, ed = (mean(sa) if sa else 0), (mean(sd) if sd else 0)
            mix = (pa - pd) * (ea + ed) / 2
            beh = (ea - ed) * (pa + pd) / 2
            tot_mix += mix
            tot_beh += beh
            print("    %-24s podíl tahů A %2d%% / D1 %2d%% | postup A %.2f / D1 %.2f | příspěvek: skladba stavů %+.2f, chování ve stavu %+.2f"
                  % (sname, round(100 * pa), round(100 * pd), ea, ed, mix, beh))
        print("    SOUČET: skladba stavů (kde nosič stojí = soupeřův tlak + naše vystavení) %+.2f | chování ve stavu (co s tím uděláme) %+.2f" % (tot_mix, tot_beh))
    print()
    print("=== 3c. TURNOVER DŘÍV NEŽ SE NOSIČ POHNUL — podíl na VŠECH tazích s držením ===")
    for name, g in groups:
        ts = [t for r in g for t in r["turns"] if t["kept"]]
        if ts:
            print("[%s] n=%d | %d (%d%%) | nosič je první událost tahu: %d%%"
                  % (name, len(ts), sum(1 for t in ts if t["turnover_before_carrier"]),
                     round(100 * sum(1 for t in ts if t["turnover_before_carrier"]) / len(ts)),
                     round(100 * sum(1 for t in ts if t["carrier_event_rank"] == 0) / len(ts))))
    print()
    print("=== 4a. ODPOR PODLE KOLA (na startu našeho tahu; pás nosiče ±2 | nejlepší pás | všech stojících soupeřů před nosičem | stojící soupeři celkem / naši) ===")
    for name, g in groups:
        by = defaultdict(list)
        for r in g:
            for t in r["turns"]:
                if t["res"] is not None:
                    by[t["turn"]].append(t)
        if by:
            print("[%s]" % name)
            for k, v in sorted(by.items()):
                print("    k%d n=%4d pás %.2f | min pás %.2f | před nosičem %.2f | stojí soupeř %.2f / my %.2f | TZ na nosiči %.2f"
                      % (k, len(v), mean([t["res"] for t in v]), mean([t["res_min_lane"] for t in v]),
                         mean([t["res_total_ahead"] for t in v]), mean([t["opp_standing"] for t in v]),
                         mean([t["own_standing"] for t in v]), mean([t["tz"] for t in v])))
    print()
    print("=== 4b. A: KOLO TD a tempo včetně TD tahu ===")
    if A:
        print("TD v kole: " + " ".join("k%d:%d" % (k, v) for k, v in sorted(Counter(r.get("td_turn") for r in A).items(), key=lambda x: str(x[0]))))
        print("použitých kol od 1. držení: " + " ".join("%s:%d" % (k, v) for k, v in sorted(Counter(r.get("turns_used") for r in A).items(), key=lambda x: str(x[0]))))
        v = [r["tempo_incl_td"] for r in A if r.get("tempo_incl_td")]
        print("tempo VČETNĚ TD tahu: průměr %.2f | " % mean(v) + pct_line(hist(v, dbins), dorder, len(v)))
    print()
    # ---------- 4. odpor ----------
    print("=== 4. ODPOR V KORIDORU podle kola (průměr stojících soupeřů před nosičem, pás ±2) ===")
    # znovu z drivů: base počítá jen průměr; spočítáme po kolech z r['turns']? base nemá per-turn -> použijeme avg_resistance rozdělení
    rb = [(-1, 2, "<2"), (2, 4, "2-4"), (4, 6, "4-6"), (6, 8, "6-8"), (8, 99, "8+")]
    for name, g in groups:
        v = [r["avg_resistance"] for r in g if r.get("avg_resistance") is not None]
        if v:
            print("[%s] n=%d průměr %.2f | " % (name, len(v), mean(v)) + pct_line(hist(v, rb), [b[2] for b in rb], len(v)))
    print()

    # ---------- 5. D drivy: kolik jim chybělo ----------
    print("=== 5. D-DRIVY: kde skončily (end_dist) a kolik chybělo ===")
    eb = [(0, 1, "0"), (1, 4, "1-3"), (4, 7, "4-6"), (7, 11, "7-10"), (11, 16, "11-15"), (16, 99, "16+")]
    for name, g in (("D1", D1), ("D2", D2)):
        v = [r["end_dist"] for r in g if r.get("end_dist") is not None]
        if v:
            print("[%s] n=%d konc. vzdálenost od endzone průměr %.2f | " % (name, len(v), mean(v)) + pct_line(hist(v, eb), [b[2] for b in eb], len(v)))
    print()
    print("=== per soupeř: A vs D 1. držení a tempo ===")
    for opp in sorted(set(r["opp"] for r in recv)):
        a = [r for r in A if r["opp"] == opp]
        d = [r for r in D if r["opp"] == opp]
        print("%-9s A n=%3d 1.drž %.2f tempo %.2f | D n=%4d 1.drž %.2f tempo %.2f"
              % (opp, len(a), mean([r.get("first_hold_turn") for r in a]), mean([r.get("avg_tempo") for r in a]),
                 len(d), mean([r.get("first_hold_turn") for r in d]), mean([r.get("avg_tempo") for r in d])))
    if anomalies:
        print("\nAnomálie: %d (prvních 5)" % len(anomalies))
        for a in anomalies[:5]:
            print("  " + a)


if __name__ == "__main__":
    main()
