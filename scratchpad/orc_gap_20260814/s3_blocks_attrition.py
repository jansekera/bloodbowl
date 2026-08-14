#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s3 — bloky, block-die faces, attrition a 'kdo srazil nosiče' per-race.

Vše z eventů + snímků (snímek = začátek kola). Face enum (enums.h):
0=ATTACKER_DOWN 1=BOTH_DOWN 2=PUSHED 3=DEFENDER_STUMBLES 4=DEFENDER_DOWN.
"""
import sys, os, glob, gzip, json
from collections import Counter, defaultdict

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"
FACES = ["AD", "BD", "PUSH", "DS", "DD"]
POSITIONS = ["Troll Slayer", "Longbeard", "Runner", "Blitzer", "Lineman",
             "Catcher", "Thrower", "Black Orc", "Ogre", "Wardancer",
             "Gutter Runner", "Storm Vermin", "Rat Ogre", "Treeman"]


def position_of(name):
    for p in POSITIONS:
        if name.startswith(p):
            return p
    return name.split(" +")[0]


def main():
    # per race aggregates
    n_games = Counter()
    blocks = defaultdict(Counter)        # (race, 'us'/'them') -> face counter
    knocked = Counter()                  # (race, victim 'us'/'them')
    armor_rolls = Counter()              # (race, side) -> n
    armor_broken = Counter()
    casualties = Counter()               # (race, victim side)
    inj = Counter()                      # (race, victim side) INJURY events
    standing = defaultdict(lambda: [0.0, 0])   # (race, side, turn) -> [sum, n]
    out_cnt = defaultdict(lambda: [0.0, 0])
    carrier_kd_by = defaultdict(Counter)  # race -> position of blocker
    carrier_kd_n = Counter()

    for f in sorted(glob.glob(os.path.join(DATA, "g*.json.gz"))):
        game = json.load(gzip.open(f, "rt"))
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        us = "home" if game["home_race"] == "dwarf" else "away"
        race = game["away_race"] if us == "home" else game["home_race"]
        n_games[race] += 1
        id_map = {}
        for tl in game["turn_logs"]:
            for side in ("home", "away"):
                for p in tl[side + "_players"]:
                    id_map[p["id"]] = ("us" if side == us else "them", p["name"])
        for tl in game["turn_logs"]:
            # attrition: stavy na snímku, klíčované číslem kola
            t = tl["turn"]
            for side in ("home", "away"):
                lab = "us" if side == us else "them"
                st = sum(1 for p in tl[side + "_players"] if p["state"] == 0)
                # mimo hřiště = hráč ve snímku chybí (state 3 se neserializuje)
                ou = 11 - len(tl[side + "_players"])
                standing[(race, lab, t)][0] += st
                standing[(race, lab, t)][1] += 1
                out_cnt[(race, lab, t)][0] += ou
                out_cnt[(race, lab, t)][1] += 1
            carrier = tl["ball_carrier_id"] if tl.get("ball_held") else -1
            carrier_side = id_map.get(carrier, (None,))[0] if carrier >= 0 else None
            last_block_on = {}   # target_id -> attacker_id (poslední blok na cíl)
            our_turn = tl["active_team"] == us
            for e in tl["events"]:
                ty = e["type"]
                pid = e["player_id"]
                pside = id_map.get(pid, (None,))[0]
                if ty == "BLOCK":
                    att_lab = pside
                    blocks[(race, att_lab)][e["roll"]] += 1
                    last_block_on[e["target_id"]] = pid
                elif ty == "KNOCKED_DOWN":
                    knocked[(race, pside)] += 1
                    if pid == carrier and carrier_side == "us" and not our_turn:
                        att = last_block_on.get(pid)
                        if att is not None:
                            carrier_kd_by[race][position_of(id_map[att][1])] += 1
                        else:
                            carrier_kd_by[race]["(bez bloku)"] += 1
                        carrier_kd_n[race] += 1
                elif ty == "ARMOR_BREAK":
                    armor_rolls[(race, pside)] += 1
                    if e["success"]:
                        armor_broken[(race, pside)] += 1
                elif ty == "CASUALTY":
                    casualties[(race, pside)] += 1
                elif ty == "INJURY":
                    inj[(race, pside)] += 1
                # sledování nosiče uvnitř tahu (hand-off/catch mění nosiče)
                if ty in ("PICKUP", "CATCH") and e["success"]:
                    carrier = pid
                    carrier_side = pside

    races = sorted(n_games)
    print("=== BLOKY / HRU A ROZDĚLENÍ ZVOLENÝCH FACES (per-race, per-side) ===")
    print("%-8s %-5s %8s %8s | %s" % ("race", "side", "hry", "bloky/hru",
                                      " ".join("%6s" % x for x in FACES)))
    for race in races:
        for lab in ("us", "them"):
            c = blocks[(race, lab)]
            tot = sum(c.values())
            row = " ".join("%5.1f%%" % (100 * c.get(i, 0) / tot) for i in range(5)) if tot else "—"
            print("%-8s %-5s %8d %8.2f | %s" % (race, lab, n_games[race],
                                                tot / n_games[race], row))
    print()
    print("=== KNOCKDOWNY, ARMOR, ZRANĚNÍ (per-race; oběť = side) ===")
    print("%-8s %-5s %10s %12s %12s %10s %10s" %
          ("race", "oběť", "KD/hru", "AVroll/hru", "AVbreak%", "INJ/hru", "CAS/hru"))
    for race in races:
        for lab in ("us", "them"):
            n = n_games[race]
            ar = armor_rolls[(race, lab)]
            print("%-8s %-5s %10.2f %12.2f %11.1f%% %10.2f %10.2f" %
                  (race, lab, knocked[(race, lab)] / n, ar / n,
                   100 * armor_broken[(race, lab)] / ar if ar else 0,
                   inj[(race, lab)] / n, casualties[(race, lab)] / n))
    print()
    print("=== STOJÍCÍ TĚLA NA ZAČÁTKU KOLA t (průměr, obě půle) ===")
    print("%-8s %-5s | %s" % ("race", "side", " ".join("T%d" % t for t in range(1, 9))))
    for race in races:
        for lab in ("us", "them"):
            vals = []
            for t in range(1, 9):
                s, n = standing[(race, lab, t)]
                vals.append(s / n if n else float("nan"))
            print("%-8s %-5s | %s" % (race, lab, " ".join("%4.1f" % v for v in vals)))
    print()
    print("=== MIMO HŘIŠTĚ (state 3) NA ZAČÁTKU KOLA t ===")
    for race in races:
        for lab in ("us", "them"):
            vals = []
            for t in range(1, 9):
                s, n = out_cnt[(race, lab, t)]
                vals.append(s / n if n else float("nan"))
            print("%-8s %-5s | %s" % (race, lab, " ".join("%4.1f" % v for v in vals)))
    print()
    print("=== KDO SRÁŽÍ NAŠEHO NOSIČE (v soupeřově kole, per-race) ===")
    for race in races:
        n = carrier_kd_n[race]
        print("%-8s n=%d (%.2f/hru): %s" %
              (race, n, n / n_games[race],
               ", ".join("%s %d%%" % (k, round(100 * v / n))
                         for k, v in carrier_kd_by[race].most_common())))


if __name__ == "__main__":
    main()
