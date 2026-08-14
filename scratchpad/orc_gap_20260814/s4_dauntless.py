#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""s4 — Dauntless offer-gap: kolik bloků na ST4+ se Slayerovi nenabídne.

Nabídka (macro_actions.cpp getBlockDiceCount) Dauntless NEZNÁ; provedení ano.
Měřím na snímcích ZAČÁTKU našeho kola (statická aproximace — nabídky vznikají
i uvnitř tahu po pohybech, takže tohle je spodní odhad příležitostí):

  Slayer stojící vedle stojícího soupeře ST>=4:
    dice_now  = nabídková logika bez Dauntless (assists dle Board.assists,
                Guard z názvu — týž kód jako diag_exposure_scan)
    offered   = dice_now >= 2 nebo dice_now == 1 (Slayer má Block)
    dice_dnt  = totéž s attST = defST (Dauntless úspěch; vs ST4 je to 83 %)
    GAP       = žádný sousední ST4+ cíl nabídnutý bez Dauntless, ale aspoň
                jeden s Dauntless >= 1 kostka

Empirická křížová kontrola: BLOCK eventy Slayer -> cíl ST>=4 (mělo by být
vzácné a jen přes převahu asistencí).
"""
import sys, os, glob, gzip, json
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
import importlib
ex = importlib.import_module("diag_exposure_scan_20260812")

DATA = "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


def offer_dice(A, D):
    if A > 2 * D:
        return 3
    if A > D:
        return 2
    if A == D:
        return 1
    return -3 if D > 2 * A else -2


def main():
    n_games = Counter()
    slayer_turns = Counter()          # race -> naše kola se stojícím Slayerem
    adj_st4 = Counter()               # race -> (slayer,kolo) případy s ST4+ sousedem
    offered_now = Counter()
    gap = Counter()                   # nenabídnuto, s Dauntless >=1 kostka
    gap_2dice = Counter()             # z toho s Dauntless >=2 kostky
    gap_target = defaultdict(Counter)  # race -> jméno cíle
    gap_near_carrier = Counter()      # cíl do 2 polí od našeho nosiče
    gap_carrier_exists = Counter()
    slayer_blocked_st4_ev = Counter()  # eventová kontrola
    slayer_blocks_ev = Counter()
    slayer_idle = Counter()           # gap-kolo a Slayer ten tah vůbec neblokoval

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
                    id_map[p["id"]] = ("us" if side == us else "them", p["name"], p["st"])
        for tl in game["turn_logs"]:
            if tl["active_team"] != us:
                continue
            # eventová kontrola: bloky Slayera
            blocked_ids = defaultdict(set)   # slayer id -> cíle
            for e in tl["events"]:
                if e["type"] == "BLOCK":
                    ent = id_map.get(e["player_id"])
                    if ent and ent[0] == "us" and ent[1].startswith("Troll Slayer"):
                        slayer_blocks_ev[race] += 1
                        blocked_ids[e["player_id"]].add(e["target_id"])
                        tgt = id_map.get(e["target_id"])
                        if tgt and tgt[2] >= 4:
                            slayer_blocked_st4_ev[race] += 1
            b = ex.Board(tl, us)
            slayers = [p for p in b.us_st if p["name"].startswith("Troll Slayer")]
            if slayers:
                slayer_turns[race] += 1
            carrier = b.carrier
            for s in slayers:
                tgts = [q for q in b.neighbors_of(s, b.th_st) if q["st"] >= 4]
                if not tgts:
                    continue
                adj_st4[race] += 1
                any_off = False
                best_dnt = -9
                best_tgt = None
                for q in tgts:
                    a, d = b.assists(s, q, b.us_st, b.th_st)
                    dn = offer_dice(s["st"] + a, q["st"] + d)
                    if dn >= 1:      # Slayer má Block -> 1 kostka se nabízí
                        any_off = True
                    ddnt = offer_dice(max(s["st"], q["st"]) + a, q["st"] + d)
                    if ddnt > best_dnt:
                        best_dnt, best_tgt = ddnt, q
                if any_off:
                    offered_now[race] += 1
                elif best_dnt >= 1:
                    gap[race] += 1
                    if best_dnt >= 2:
                        gap_2dice[race] += 1
                    gap_target[race][best_tgt["name"]] += 1
                    if carrier is not None:
                        gap_carrier_exists[race] += 1
                        if max(abs(best_tgt["x"] - carrier["x"]),
                               abs(best_tgt["y"] - carrier["y"])) <= 2:
                            gap_near_carrier[race] += 1
                    if not blocked_ids.get(s["id"]):
                        slayer_idle[race] += 1

    races = sorted(n_games)
    print("=== DAUNTLESS OFFER-GAP (snímky začátku našeho kola) ===")
    print("%-8s %6s %10s %10s %10s %8s %8s %10s" %
          ("race", "hry", "kolSlayer", "adjST4+", "nabídnuto", "GAP", "GAP2k", "GAP/hru"))
    for race in races:
        print("%-8s %6d %10d %10d %10d %8d %8d %10.2f" %
              (race, n_games[race], slayer_turns[race], adj_st4[race],
               offered_now[race], gap[race], gap_2dice[race],
               gap[race] / n_games[race]))
    print()
    print("=== GAP: cíle, blízkost nosiče, nečinnost Slayera ===")
    for race in races:
        if not gap[race]:
            continue
        print("%-8s cíle: %s" % (race, ", ".join(
            "%s %d×" % (k, v) for k, v in gap_target[race].most_common())))
        ce = gap_carrier_exists[race]
        print("         cíl do 2 polí od nosiče: %d z %d (kde nosič existoval)"
              % (gap_near_carrier[race], ce))
        print("         Slayer v tom kole vůbec neblokoval: %d z %d"
              % (slayer_idle[race], gap[race]))
    print()
    print("=== EVENTOVÁ KONTROLA: bloky Slayera ===")
    print("%-8s %14s %18s" % ("race", "bloky Slayera", "z toho na ST4+"))
    for race in races:
        print("%-8s %14d %18d" % (race, slayer_blocks_ev[race],
                                  slayer_blocked_st4_ev[race]))


if __name__ == "__main__":
    main()
