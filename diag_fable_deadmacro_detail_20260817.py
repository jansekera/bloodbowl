#!/usr/bin/env python3
"""
FABLE 17.08. — detail k mrtvým makrům (doplněk k diag_fable_offered_played).

1. BLITZ_AND_SCORE: kolik nabídkových kol se kryje se SCORE nabídkou; v kolech
   „jen B&S" (SCORE nedostupné) — padl v kole TD? proběhl blitz-blok?
2. PASS eventy: kdo hází, ve kterém kole (emergency = kola 7-8), úspěšnost.
3. HAND_OFF (=UNKNOWN) eventy naší strany: kolo, dárce→příjemce, TD příjemce.
4. CHAIN pokusy: kola s PASS i HAND_OFF eventem naráz (P4 říká: nemožné).
Snímek = začátek kola ⇒ „nabídnuto" je PODLAHA.
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260814_dauntless_data"


def cheb_xy(px, py, qx, qy):
    return max(abs(px - qx), abs(py - qy))


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    bs = Counter()
    passes = Counter()
    handoffs = Counter()
    chain = Counter()
    ho_turnno = Counter()
    pass_turnno = Counter()
    ho_pairs = Counter()
    pass_names = Counter()
    games = 0
    hos_turns = 0          # kola s nabídkou hand-off swapu (PASS_ACTION dist1)
    hos_games = set()

    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "dwarf":
            us, them = "home", "away"
        elif g["away_race"] == "dwarf":
            us, them = "away", "home"
        else:
            continue
        games += 1
        ez = 25 if us == "home" else 0

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            my_ids = {p["id"] for p in mine}
            name = {p["id"]: p["name"] for p in mine}
            stand_mine = [p for p in mine if p["state"] == 0]
            stand_theirs = [p for p in theirs if p["state"] == 0]

            carrier = next((p for p in mine if p["has_ball"]), None)
            events = t["events"]

            # --- 1. B&S vs SCORE ---
            if carrier is not None and carrier["state"] == 0:
                d = abs(carrier["x"] - ez)
                score_off = 0 < d <= carrier["ma"] + 2
                bs_off = False
                if 0 < d <= carrier["ma"] + 5:
                    for e in stand_theirs:
                        if abs(e["x"] - ez) >= d:
                            continue
                        ydiff = abs(e["y"] - carrier["y"])
                        xdist = abs(e["x"] - carrier["x"])
                        if ydiff <= 2 and xdist <= 2 and xdist + ydiff <= 3:
                            bs_off = True
                            break
                if bs_off:
                    bs["offered"] += 1
                    td = any(e["type"] == "TOUCHDOWN" and e["player_id"] in my_ids
                             for e in events)
                    moved = set()
                    carrier_blitzed = False
                    for e in events:
                        if e["type"] in ("MOVE", "GFI", "DODGE") and \
                                e["player_id"] == carrier["id"]:
                            moved.add(e["player_id"])
                        if e["type"] == "BLOCK" and e["player_id"] == carrier["id"] \
                                and carrier["id"] in moved:
                            carrier_blitzed = True
                    key = "with_SCORE" if score_off else "only_BS"
                    bs[key] += 1
                    if td:
                        bs[key + "_td"] += 1
                    if carrier_blitzed:
                        bs[key + "_carrier_blitzed"] += 1

            # --- nabídka hand-off swapu (jen subtyp, kola) ---
            if carrier is not None and carrier["state"] == 0:
                SURE = {"Runner +Block", "Thrower +Block"}
                c_poor = carrier["ag"] <= 2 and carrier["name"] not in SURE
                if c_poor:
                    c_dist = abs(carrier["x"] - ez)
                    for tm in stand_mine:
                        if tm["id"] == carrier["id"]:
                            continue
                        if cheb_xy(carrier["x"], carrier["y"], tm["x"], tm["y"]) != 1:
                            continue
                        t_poor = tm["ag"] <= 2 and tm["name"] not in SURE
                        t_dist = abs(tm["x"] - ez)
                        if t_poor or t_dist > c_dist:
                            continue
                        ctz = sum(1 for e in stand_theirs
                                  if cheb_xy(tm["x"], tm["y"], e["x"], e["y"]) == 1)
                        if 7 - tm["ag"] - 1 + ctz <= 4:
                            hos_turns += 1
                            hos_games.add(f)
                            break

            # --- 2.-4. eventy ---
            pass_ev = [e for e in events if e["type"] == "PASS"
                       and e["player_id"] in my_ids]
            ho_ev = [e for e in events if e["type"] == "UNKNOWN"
                     and e["player_id"] in my_ids]
            for e in pass_ev:
                passes["total"] += 1
                pass_turnno[(t["half"], t["turn"])] += 1
                pass_names[name.get(e["player_id"], "?")] += 1
                if e["success"]:
                    passes["success_flag"] += 1
            for e in ho_ev:
                handoffs["total"] += 1
                ho_turnno[(t["half"], t["turn"])] += 1
                ho_pairs[(name.get(e["player_id"], "?"),
                          name.get(e["target_id"], "?"))] += 1
                if any(x["type"] == "TOUCHDOWN" and x["player_id"] == e["target_id"]
                       for x in events):
                    handoffs["receiver_td"] += 1
                # chytil to? CATCH event příjemce po hand-offu
                if any(x["type"] == "CATCH" and x["player_id"] == e["target_id"]
                       and x["success"] for x in events):
                    handoffs["caught"] += 1
            if pass_ev and ho_ev:
                chain["same_turn_pass_and_handoff"] += 1

    print(f"her: {games}")
    print("\n=== 1. BLITZ_AND_SCORE (nabídková kola, snímek=PODLAHA) ===")
    print(f"  nabídnuto celkem:            {bs['offered']} kol")
    print(f"  z toho SCORE také nabídnuto: {bs['with_SCORE']} "
          f"(TD v kole: {bs['with_SCORE_td']}, nosič blitzoval: {bs['with_SCORE_carrier_blitzed']})")
    print(f"  jen B&S (SCORE nedostupné):  {bs['only_BS']} "
          f"(TD v kole: {bs['only_BS_td']}, nosič blitzoval: {bs['only_BS_carrier_blitzed']})")

    print("\n=== 2. PASS eventy naší strany ===")
    print(f"  celkem {passes['total']}, success-flag {passes['success_flag']}")
    print("  podle (půle, kolo):", dict(sorted(pass_turnno.items())))
    print("  házeči:", dict(pass_names.most_common()))

    print("\n=== 3. HAND_OFF eventy naší strany ===")
    print(f"  celkem {handoffs['total']} · chyceno {handoffs['caught']} · "
          f"TD příjemce v témž kole {handoffs['receiver_td']}")
    print("  podle (půle, kolo):", dict(sorted(ho_turnno.items())))
    print("  dvojice dárce→příjemce:", dict(ho_pairs.most_common(10)))

    print("\n=== 4. CHAIN pokusy (PASS i HAND_OFF v témž kole) ===")
    print(f"  {chain['same_turn_pass_and_handoff']} kol (P4 predikuje 0)")

    print(f"\n=== nabídka hand-off SWAPU (PASS_ACTION dist=1, kola) ===")
    print(f"  {hos_turns} kol · {len(hos_games)} her s aspoň jednou nabídkou")


if __name__ == "__main__":
    main()
