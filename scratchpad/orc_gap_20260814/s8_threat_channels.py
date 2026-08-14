#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s8 — sjednocená hrozba na nosiče přes VŠECHNY kanály (oprava metodiky 14.08.).

Námitka: BLZ>=2 měří jen SILOVÝ kanál (orkův); skaven hrozí dovednostmi
(Wrestle, Strip Ball). Model na kostku bloku proti NAŠEMU nosiči (všichni
trpasličí nosiči mají Block, žádný Dodge; Runner má Sure Hands):

  q_down  = DD(1) + DS(1) [nosič bez Dodge]
            + BD(1), pokud útočník má Wrestle a NEMÁ Block
              (block_handler.cpp:494 attWrestle; oba k zemi, BEZ brnění,
               bez turnoveru útočníka)
  q_strip = PUSHED(2), pokud útočník má Strip Ball a nosič NEMÁ Sure Hands
            (block_handler.cpp:627 — Sure Hands strip ruší)
  q_ball  = q_down + q_strip (z 6 faces)

  p(útočník) = 1-(1-q_ball/6)^n pro n kostek útočníka;
               (q_ball/6)^|n| pro bloky do kopce (vybírá obránce)
  THREAT(kolo) = max přes soupeře, kteří na nosiče dosáhnou
                 (dosah/kostky/asistence PŘESNĚ jako diag_exposure_scan BLZ)

  THREAT_raw  = totéž BEZ odečtu Sure Hands (syrová hrozba — „hrušky")
  THREAT_net  = s odečtem (skutečná hrozba — „jablka")

Části: A) model per-race + rozpad kanálů + kalibrace na empirii,
B) empirická atribuce faces u sražení nosiče (DD/DS/BD-Wrestle) + Sure
Hands negace + strip úspěchy z eventů, C) zrcadlo — hrozba/pád JEJICH
nosiče v NAŠEM kole (odkud jsou krádežové TD).
"""
import sys, os, glob, gzip, json, math
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
ex = importlib.import_module("diag_exposure_scan_20260812")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"
WRESTLE, STRIPBALL, SUREHANDS = 36, 9, 10   # enums.h: SecretWeapon=35, Wrestle=36

# skilly z roster.cpp (TV1200) podle prefixu jména — jen ty, co tu hrají roli
def att_skills(name):
    s = set()
    if name.startswith(("Blitzer", "Thrower +Block", "Black Orc", "Catcher +Block",
                        "Ogre +Block", "Wardancer")):
        s.add("Block")
    if "ball-hunter" in name:
        s.add("StripBall")
    if "+Wrestle" in name:
        s.add("Wrestle")
    return s
# (trpasličí nosiči: Block všichni; Sure Hands jen Runner; Dodge nikdo)


def dice_for(att, carrier, already_adj, b):
    """Kostky nejlepšího blitzu útočníka `att` na nosiče — TÝŽ výpočet jako
    ex.predictors (asistence = jejich hráči UŽ stojící u nosiče)."""
    a = sum(1 for r in already_adj if r is not att and
            (ex.guard(r) or not any(e is not carrier
                                    for e in b.neighbors_of(r, b.us_st))))
    A, D = att["st"] + a, carrier["st"]
    return 3 if A > 2 * D else 2 if A > D else 1 if A == D else -2


def p_ball_off(n, q):
    if n >= 1:
        return 1.0 - (1.0 - q / 6.0) ** n
    return (q / 6.0) ** abs(n)


def threat_of_board(b):
    """(THREAT_net, THREAT_raw, kanál argmaxu net, detaily) pro b.carrier."""
    c = b.carrier
    ring = {s for s in ex.adj(c["x"], c["y"]) if s not in b.occ}
    already_adj = b.neighbors_of(c, b.th_st)
    car_sh = c["name"].startswith("Runner")
    best_n = best_r = 0.0
    ch = None
    strip_reach_raw = strip_reach_net = wrestle_reach = 0
    for p in b.th_st:
        if p in already_adj:
            dn = 0
        else:
            dn = b.bfs((p["x"], p["y"]), ring, p["ma"] + 2)
        if dn is None:
            continue
        sk = att_skills(p["name"])
        n = dice_for(p, c, already_adj, b)
        q = 2 + (1 if ("Wrestle" in sk and "Block" not in sk) else 0)
        q_raw = q + (2 if "StripBall" in sk else 0)
        q_net = q + (2 if ("StripBall" in sk and not car_sh) else 0)
        if "StripBall" in sk:
            strip_reach_raw += 1
            if not car_sh:
                strip_reach_net += 1
        if "Wrestle" in sk and "Block" not in sk:
            wrestle_reach += 1
        pr, pn = p_ball_off(n, q_raw), p_ball_off(n, q_net)
        if pr > best_r:
            best_r = pr
        if pn > best_n:
            best_n = pn
            if n >= 2:
                ch = "síla≥2k" + ("+skill" if q_net > 2 else "")
            elif n == 1:
                ch = ("1k Wrestle" if ("Wrestle" in sk and "Block" not in sk)
                      else "1k Strip" if (q_net > 2)
                      else "1k prostý")
            else:
                ch = "do kopce"
    if ch is None:
        ch = "nedosáhne"
    return best_n, best_r, ch, strip_reach_raw, strip_reach_net, wrestle_reach


def main():
    rowsA = []          # naše expozice (soupeřovo kolo, náš nosič)
    rowsC = []          # zrcadlo (naše kolo, jejich nosič)
    # část B — empirické eventy
    kd_face = defaultdict(Counter)      # race -> face sražení nosiče
    sh_negate = Counter()               # race -> Sure Hands negace stripu (na nás)
    strip_ok = Counter()                # race -> strip bez sražení (na nás)
    wrestle_used_on_carrier = Counter()
    wrestle_used_total = Counter()
    FACE = {0: "AD", 1: "BD", 2: "PUSH", 3: "DS", 4: "DD"}

    for f in sorted(glob.glob(os.path.join(DATA, "g*.json.gz"))):
        game = json.load(gzip.open(f, "rt"))
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        us = "home" if game["home_race"] == "dwarf" else "away"
        opp = "away" if us == "home" else "home"
        race = game["away_race"] if us == "home" else game["home_race"]
        logs = game["turn_logs"]
        id_map = {}
        for tl in logs:
            for side in ("home", "away"):
                for p in tl[side + "_players"]:
                    id_map[p["id"]] = ("us" if side == us else "them", p["name"])

        for i, t in enumerate(logs):
            if t["touchdown"]:
                continue
            if i + 1 >= len(logs) or logs[i + 1]["half"] != t["half"]:
                continue
            if logs[i + 1]["active_team"] == t["active_team"]:
                continue
            if t["active_team"] != us:
                # ---- část A: soupeřovo kolo, náš nosič ----
                bt = ex.Board(t, us)
                if bt.carrier is not None and bt.carrier["state"] == 0:
                    tn, tr, ch, srr, srn, wr = threat_of_board(bt)
                    bn = ex.Board(logs[i + 1], us)
                    nxt = {p["id"]: p for p in bn.us + bn.them}
                    q = nxt.get(bt.carrier["id"])
                    down = 1 if (q is None or q["state"] != 0) else 0
                    lost = 0 if bn.carrier is not None else 1
                    rowsA.append(dict(race=race, tn=tn, tr=tr, ch=ch,
                                      srr=srr, srn=srn, wr=wr,
                                      down=down, lost=lost,
                                      sh=bt.carrier["name"].startswith("Runner")))
                # ---- část B: eventy soupeřova kola ----
                carrier = t["ball_carrier_id"] if t.get("ball_held") else -1
                cside = id_map.get(carrier, (None,))[0] if carrier >= 0 else None
                last_face_on = {}
                last_att_on = {}
                for e in t["events"]:
                    ty, pid = e["type"], e["player_id"]
                    if ty == "BLOCK":
                        last_face_on[e["target_id"]] = e["roll"]
                        last_att_on[e["target_id"]] = pid
                    elif ty == "SKILL":
                        if e["roll"] == WRESTLE:
                            wrestle_used_total[race] += 1
                            if carrier >= 0 and last_face_on.get(carrier) == 1:
                                wrestle_used_on_carrier[race] += 1
                        elif e["roll"] == SUREHANDS and cside == "us":
                            sh_negate[race] += 1
                    elif ty == "KNOCKED_DOWN" and pid == carrier and cside == "us":
                        kd_face[race][FACE.get(last_face_on.get(pid, -1), "bez bloku")] += 1
                    elif ty == "BALL_BOUNCE" and cside == "us" and carrier >= 0:
                        fa = last_face_on.get(carrier)
                        att = last_att_on.get(carrier)
                        if fa == 2 and att is not None and \
                           "StripBall" in att_skills(id_map[att][1]):
                            strip_ok[race] += 1
                            carrier, cside = -1, None
                    if ty in ("PICKUP", "CATCH") and e["success"]:
                        carrier = pid
                        cside = id_map.get(pid, (None,))[0]
                    elif ty == "KNOCKED_DOWN" and pid == carrier:
                        carrier, cside = -1, None
            else:
                # ---- část C: naše kolo, JEJICH nosič (zrcadlo) ----
                bo = ex.Board(t, opp)   # „us" = soupeř -> carrier je jejich
                if bo.carrier is not None and bo.carrier["state"] == 0:
                    P = ex.predictors(bo)   # BLZ = NAŠE kostky na jejich nosiče
                    bn = ex.Board(logs[i + 1], opp)
                    nxt = {p["id"]: p for p in bn.us + bn.them}
                    q = nxt.get(bo.carrier["id"])
                    down = 1 if (q is None or q["state"] != 0) else 0
                    lost = 0 if bn.carrier is not None else 1
                    rowsC.append(dict(race=race, blz=P.get("BLZ"),
                                      reach=P.get("REACH"), down=down, lost=lost,
                                      who=bo.carrier["name"].split(" +")[0]))

    races = sorted({r["race"] for r in rowsA})
    TD = {"skaven": 451, "wood-elf": 260, "human": 178, "orc": 86}

    print("=== A) SJEDNOCENÁ HROZBA NA NOSIČE / SOUPEŘOVO KOLO (model) ===")
    print("%-8s %6s %10s %10s %12s %12s %8s" %
          ("race", "n", "THREATnet", "THREATraw", "P(nosič↓)emp", "P(ztráta)emp", "naše TD"))
    for rc in races:
        rs = [r for r in rowsA if r["race"] == rc]
        n = len(rs)
        print("%-8s %6d %10.3f %10.3f %12.3f %12.3f %8d" %
              (rc, n, sum(r["tn"] for r in rs) / n, sum(r["tr"] for r in rs) / n,
               sum(r["down"] for r in rs) / n, sum(r["lost"] for r in rs) / n,
               TD.get(rc, 0)))
    print()
    print("=== A2) KANÁL nejlepší hrozby (argmax THREAT_net), podíl kol ===")
    chs = ["síla≥2k", "síla≥2k+skill", "1k Wrestle", "1k Strip", "1k prostý",
           "do kopce", "nedosáhne"]
    print("%-8s" % "race" + "".join("%14s" % c for c in chs))
    for rc in races:
        rs = [r for r in rowsA if r["race"] == rc]
        c = Counter(r["ch"] for r in rs)
        print("%-8s" % rc + "".join("%13.1f%%" % (100 * c.get(x, 0) / len(rs))
                                    for x in chs))
    print()
    print("=== A3) STRIP BALL: dosažitelný stripper / kolo — syrově vs po Sure Hands ===")
    print("%-8s %14s %14s %20s" % ("race", "raw (kol %)", "net (kol %)",
                                   "nosič bez SureHands"))
    for rc in races:
        rs = [r for r in rowsA if r["race"] == rc]
        raw = sum(1 for r in rs if r["srr"] > 0) / len(rs)
        net = sum(1 for r in rs if r["srn"] > 0) / len(rs)
        nsh = sum(1 for r in rs if not r["sh"]) / len(rs)
        print("%-8s %13.1f%% %13.1f%% %19.1f%%" % (rc, 100 * raw, 100 * net, 100 * nsh))
    print()
    print("=== A4) KALIBRACE: empirie podle pásem THREAT_net (pool všech ras) ===")
    bins = [(0, .05), (.05, .15), (.15, .25), (.25, .35), (.35, .45), (.45, 1.01)]
    print("%-12s %8s %10s %10s" % ("THREAT", "n", "P(↓)emp", "P(ztráta)"))
    for lo, hi in bins:
        sub = [r for r in rowsA if lo <= r["tn"] < hi]
        if sub:
            print("%5.2f–%4.2f %8d %10.3f %10.3f" %
                  (lo, hi, len(sub), sum(r["down"] for r in sub) / len(sub),
                   sum(r["lost"] for r in sub) / len(sub)))
    # kalibrace per-race v překrývajícím se pásmu
    print("\n-- P(↓)emp v pásmu THREAT_net 0.15–0.35 per-race (test 'stejná hrozba, stejná konverze') --")
    for rc in races:
        sub = [r for r in rowsA if r["race"] == rc and .15 <= r["tn"] < .35]
        if len(sub) >= 50:
            print("  %-8s %.3f (n=%d)" % (rc, sum(r["down"] for r in sub) / len(sub), len(sub)))
    print()

    print("=== B) EMPIRIE Z EVENTŮ: čím nosič reálně padá (face posledního bloku) ===")
    print("%-8s %s" % ("race", "faces sražení nosiče v soupeřově kole"))
    for rc in races:
        c = kd_face[rc]
        tot = sum(c.values())
        print("%-8s n=%4d | %s" % (rc, tot, ", ".join(
            "%s %d%%" % (k, round(100 * v / tot)) for k, v in c.most_common())))
    print()
    print("%-8s %18s %18s %22s %14s" %
          ("race", "Wrestle použit/hru", "…z toho BD na nosiče", "SureHands negace/hru",
           "strip uspěl/hru"))
    for rc in races:
        print("%-8s %18.2f %18d %22.2f %14.2f" %
              (rc, wrestle_used_total[rc] / 750, wrestle_used_on_carrier[rc],
               sh_negate[rc] / 750, strip_ok[rc] / 750))
    print()

    print("=== C) ZRCADLO: JEJICH nosič v NAŠEM kole (odkud jsou krádeže) ===")
    print("%-8s %6s %12s %12s %12s %16s" %
          ("race", "n", "P(nosič↓)", "P(ztráta)", "náš BLZ≥2", "STEAL+TD/750"))
    steals = {"skaven": 198, "wood-elf": 102, "human": 82, "orc": 31}
    for rc in races:
        rs = [r for r in rowsC if r["race"] == rc]
        n = len(rs)
        b2 = sum(1 for r in rs if isinstance(r["blz"], int) and r["blz"] >= 2) / n
        print("%-8s %6d %12.3f %12.3f %11.1f%% %16d" %
              (rc, n, sum(r["down"] for r in rs) / n,
               sum(r["lost"] for r in rs) / n, 100 * b2, steals.get(rc, 0)))
    print()
    print("-- kdo u nich nosí (kola s jejich držením) --")
    for rc in races:
        c = Counter(r["who"] for r in rowsC if r["race"] == rc)
        tot = sum(c.values())
        print("%-8s %s" % (rc, ", ".join("%s %d%%" % (k, round(100 * v / tot))
                                         for k, v in c.most_common(4))))


if __name__ == "__main__":
    main()
