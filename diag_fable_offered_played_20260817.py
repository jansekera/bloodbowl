#!/usr/bin/env python3
"""
FABLE 17.08. — NABÍDNUTO vs ZAHRÁNO pro všech 14 typů maker

Rekonstrukce brány `getAvailableMacros` (engine/src/macro_actions.cpp:285-1036,
stav HEAD 79382711, dauntlessInOffer=True — korpus 14.08. běžel s DAUNTLESS=1)
nad snímky ZAČÁTKU našich kol, proti tomu klasifikace zahraných akcí
z `turn_logs[].events[]`.

⚠️ OMEZENÍ SNÍMKU (začátek kola) — všechna „nabídnuto" čísla jsou PODLAHA:
  - hasActed/hasMoved/blitzUsed/passUsed/foulUsed = false (pravda jen na
    začátku kola; makra generovaná PO jiných makrech v témž kole nevidíme),
  - movementRemaining = MA (pravda na začátku kola),
  - lostTacklezones ze snímku nejde poznat (TakeRoot má jen soupeřův Treeman,
    naše nabídky to neovlivní),
  - stav vzniklý až během kola (soupeř sražen → FOUL, míč upuštěn → PICKUP)
    se do „nabídnuto" nezapočítá, do „zahráno" ANO. Poměr zahráno/nabídnuto
    proto může u FOUL/PICKUP legálně přesáhnout 1.

⚠️ KLÍČOVÝ NÁLEZ CESTOU: bb_module.cpp:325 serializuje event typu HAND_OFF
   (index 21) jako "UNKNOWN", protože stráž je `typeIdx < 21` a pole
   eventNames má 22 položek. „0 hand-offů za 3000 her" byl artefakt logu.
   Tady se UNKNOWN počítá jako HAND_OFF (ověřeno: soused, týž tým, následuje
   CATCH/SKILL).

Zahráno — co jde z eventů poznat:
  - BLOCK event bez předchozího pohybu útočníka v kole = BLOCK makro,
    s pohybem = BLITZ (blitz z už-sousední pozice se chybně počítá jako
    BLOCK; podhodnocuje BLITZ, nadhodnocuje BLOCK),
  - FOUL/PICKUP/PASS/HAND_OFF eventy přímo,
  - TOUCHDOWN: přiřazen SCORE-rodině podle toho, jak střelec přišel k míči
    (hand-off → HAND_OFF_SCORE, pass → PASS_SCORE, pass+handoff → CHAIN_SCORE,
    vlastní blitz v kole → BLITZ_AND_SCORE, jinak SCORE),
  - ADVANCE/CAGE/REPOSITION jsou z eventů NEROZLIŠITELNÉ (samé MOVE);
    hlásí se jen proxy: kola s pohybem nosiče / ne-nosičů.

Použití: nice -n 19 python3 diag_fable_offered_played_20260817.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260814_dauntless_data"

# Skilly podle (rasa, jméno pozice) — přepis engine/src/roster.cpp:498-597
# (developed rostery TV1200, přesně ty, které korpus používá).
SKILLS = {
    ("dwarf", "Longbeard"): {"Block", "Tackle", "ThickSkull"},
    ("dwarf", "Longbeard +Guard"): {"Block", "Tackle", "ThickSkull", "Guard"},
    ("dwarf", "Blitzer +Guard+Tackle"): {"Block", "ThickSkull", "Guard", "Tackle"},
    ("dwarf", "Troll Slayer +Guard+Tackle"): {"Block", "Frenzy", "ThickSkull",
                                              "Dauntless", "Guard", "Tackle"},
    ("dwarf", "Runner +Block"): {"SureHands", "ThickSkull", "Block"},
    ("orc", "Lineman"): set(),
    ("orc", "Blitzer +Guard"): {"Block", "Guard"},
    ("orc", "Blitzer +Mighty Blow"): {"Block", "MightyBlow"},
    ("orc", "Blitzer ball-hunter"): {"Block", "StripBall", "Tackle"},
    ("orc", "Black Orc +Guard+Block"): {"Guard", "Block"},
    ("orc", "Thrower +Block"): {"SureHands", "Pass", "Block"},
    ("human", "Lineman"): set(),
    ("human", "Blitzer +Guard"): {"Block", "Guard"},
    ("human", "Blitzer +Mighty Blow"): {"Block", "MightyBlow"},
    ("human", "Blitzer ball-hunter"): {"Block", "StripBall", "Tackle"},
    ("human", "Thrower +Block"): {"SureHands", "Pass", "Block"},
    ("human", "Catcher +Block"): {"Catch", "Dodge", "Block"},
    ("human", "Ogre +Block"): {"Loner", "BoneHead", "MightyBlow", "ThickSkull",
                               "ThrowTeamMate", "Block"},
    ("skaven", "Lineman"): set(),
    ("skaven", "Gutter Runner +Sure Feet"): {"Dodge", "SureFeet"},
    ("skaven", "Blitzer +Guard"): {"Block", "Guard"},
    ("skaven", "Blitzer ball-hunter"): {"Block", "StripBall", "Tackle"},
    ("skaven", "Thrower +Block"): {"SureHands", "Pass", "Block"},
    ("skaven", "Lineman +Wrestle"): {"Wrestle"},
    ("wood-elf", "Lineman"): set(),
    ("wood-elf", "Wardancer ball-hunter"): {"Block", "Dodge", "Leap", "StripBall"},
    ("wood-elf", "Wardancer +Side Step"): {"Block", "Dodge", "Leap", "SideStep"},
    ("wood-elf", "Catcher +Block"): {"Catch", "Dodge", "Sprint", "Block"},
    ("wood-elf", "Thrower +Block"): {"Pass", "Block"},
    ("wood-elf", "Treeman +Guard"): {"Loner", "TakeRoot", "StandFirm",
                                     "MightyBlow", "ThickSkull", "Guard"},
}

# passRangeFromOffset — přepis enums.h:204-233 (Q=0,S=1,L=2,B=3, X=None)
Q, S, L, B, X = 0, 1, 2, 3, None
GRID = [
    [Q, Q, Q, Q, S, S, S, L, L, L, L, B, B, B],
    [Q, Q, Q, Q, S, S, S, L, L, L, L, B, B, B],
    [Q, Q, Q, S, S, S, S, L, L, L, L, B, B, B],
    [Q, Q, S, S, S, S, S, L, L, L, B, B, B, X],
    [S, S, S, S, S, S, L, L, L, L, B, B, B, X],
    [S, S, S, S, S, L, L, L, L, B, B, B, X, X],
    [S, S, S, S, L, L, L, L, L, B, B, B, X, X],
    [L, L, L, L, L, L, L, L, B, B, B, X, X, X],
    [L, L, L, L, L, L, L, B, B, B, X, X, X, X],
    [L, L, L, L, L, B, B, B, B, X, X, X, X, X],
    [L, L, L, B, B, B, B, B, X, X, X, X, X, X],
    [B, B, B, B, B, B, B, X, X, X, X, X, X, X],
    [B, B, B, B, B, X, X, X, X, X, X, X, X, X],
    [B, B, B, X, X, X, X, X, X, X, X, X, X, X],
]
PASS_MOD = {Q: 1, S: 0, L: -1, B: -2}


def pass_range(dx, dy):
    adx, ady = abs(dx), abs(dy)
    if adx >= 14 or ady >= 14:
        return None
    return GRID[ady][adx]


def cheb(a, b):
    return max(abs(a["x"] - b["x"]), abs(a["y"] - b["y"]))


def cheb_xy(px, py, qx, qy):
    return max(abs(px - qx), abs(py - qy))


MACROS = ["SCORE", "ADVANCE", "CAGE", "BLITZ", "BLOCK", "PICKUP", "PASS_ACTION",
          "FOUL", "REPOSITION", "END_TURN", "BLITZ_AND_SCORE", "HAND_OFF_SCORE",
          "PASS_SCORE", "CHAIN_SCORE"]


def skills(race, p):
    return SKILLS[(race, p["name"])]


def analyze_turn_offers(mine, theirs, race, opp_race, ez, turn_no, weather, ball):
    """Vrací dict typ → počet kandidátů vložených do nabídky (0 = nenabídnuto).
    Přesný přepis getAvailableMacros(state, out, dauntlessInOffer=True) pro
    snímek začátku kola (hasActed=false, movementRemaining=MA, žádný
    passUsed/blitzUsed/foulUsed)."""
    off = Counter()
    off["END_TURN"] = 1

    on_mine = [p for p in mine if p["state"] <= 2]        # STANDING/PRONE/STUNNED
    on_theirs = [p for p in theirs if p["state"] <= 2]
    stand_mine = [p for p in on_mine if p["state"] == 0]
    stand_theirs = [p for p in on_theirs if p["state"] == 0]
    occ = {}
    for p in on_mine:
        occ[(p["x"], p["y"])] = ("mine", p)
    for p in on_theirs:
        occ[(p["x"], p["y"])] = ("theirs", p)

    def tz(px, py, enemies=stand_theirs, exclude_id=None):
        return sum(1 for e in enemies
                   if e["id"] != exclude_id and cheb_xy(px, py, e["x"], e["y"]) == 1)

    def tz_on_me(px, py, exclude_id=None):
        # tackle zóny NA NAŠEM hráči = stojící soupeři
        return tz(px, py, stand_theirs, exclude_id)

    def tz_on_them(px, py, exclude_id=None):
        return sum(1 for e in stand_mine
                   if e["id"] != exclude_id and cheb_xy(px, py, e["x"], e["y"]) == 1)

    carrier = next((p for p in on_mine if p["has_ball"]), None)
    if carrier is not None and carrier["state"] != 0:
        carrier_can_act = False
    else:
        carrier_can_act = carrier is not None
    i_have_ball = carrier is not None
    ball_on_ground = (not ball["held"]) and 0 <= ball["x"] <= 25

    def dist_ez(p):
        return abs(p["x"] - ez)

    # --- SCORE ---
    if i_have_ball and carrier_can_act:
        d = dist_ez(carrier)
        if 0 < d <= carrier["ma"] + 2:
            off["SCORE"] += 1

    # --- HAND_OFF_SCORE / PASS_SCORE (carrierStuck s TZ klauzulí) ---
    if i_have_ball and carrier_can_act:
        d = dist_ez(carrier)
        ctz = tz_on_me(carrier["x"], carrier["y"])
        stuck = (d > carrier["ma"] + 2) or (ctz >= 2 and d > 0)
        if stuck:
            for tm in stand_mine:
                if tm["id"] == carrier["id"]:
                    continue
                if cheb(carrier, tm) > 2:
                    continue
                rd = abs(tm["x"] - ez)
                if 0 < rd <= tm["ma"] + 2:
                    off["HAND_OFF_SCORE"] += 1
            # PASS_SCORE: vybere se max 1 cíl (bestTarget)
            found = False
            for tm in stand_mine:
                if tm["id"] == carrier["id"]:
                    continue
                pd = cheb(carrier, tm)
                if pd < 2 or pd > 10:
                    continue
                rd = abs(tm["x"] - ez)
                if 0 < rd <= tm["ma"] + 2:
                    found = True
            if found:
                off["PASS_SCORE"] += 1

    # --- CHAIN_SCORE (stuck bez TZ klauzule) ---
    if i_have_ball and carrier_can_act:
        d = dist_ez(carrier)
        if d > carrier["ma"] + 2:
            found = False
            for relay in stand_mine:
                if found or relay["id"] == carrier["id"]:
                    continue
                pd = cheb(carrier, relay)
                if pd < 1 or pd > 10:
                    continue
                for sc in stand_mine:
                    if sc["id"] in (carrier["id"], relay["id"]):
                        continue
                    if cheb(relay, sc) > 2:
                        continue
                    sd = abs(sc["x"] - ez)
                    if 0 < sd <= sc["ma"] + 2:
                        found = True
                        break
            if found:
                off["CHAIN_SCORE"] += 1

    # --- ADVANCE ---
    if i_have_ball and carrier_can_act and carrier["ma"] > 0:
        if dist_ez(carrier) > carrier["ma"] + 2:
            off["ADVANCE"] += 1

    # --- CAGE ---
    if i_have_ball:
        if any(p["id"] != carrier["id"] for p in stand_mine):
            off["CAGE"] += 1

    # --- BLITZ (nabídka existuje pro každého stojícího soupeře, pokud máme
    #     aspoň jedno volné tělo; emituje se top-1, na obraně top-2) ---
    if stand_mine and stand_theirs:
        on_def = (not i_have_ball) and (not ball_on_ground)
        n_cand = len(stand_theirs)
        off["BLITZ"] += min(2 if (on_def and n_cand > 1) else 1, n_cand)

    # --- BLITZ_AND_SCORE ---
    if i_have_ball and carrier_can_act:
        d = dist_ez(carrier)
        if 0 < d <= carrier["ma"] + 2 + 3:
            best = None
            best_dist = 999
            for e in stand_theirs:
                if abs(e["x"] - ez) >= d:
                    continue
                ydiff = abs(e["y"] - carrier["y"])
                if ydiff > 2:
                    continue
                xdist = abs(e["x"] - carrier["x"])
                if xdist <= 2 and xdist + ydiff <= 3:
                    if xdist + ydiff < best_dist:
                        best_dist = xdist + ydiff
                        best = e
            if best is not None:
                off["BLITZ_AND_SCORE"] += 1

    # --- BLOCK (přesné kostky vč. Dauntless a Guard asistencí) ---
    def count_assists(target, att_id, side_players, tz_enemies, tz_exclude):
        cnt = 0
        for a in side_players:
            if a["id"] in (att_id, target["id"]):
                continue
            if a["state"] != 0:
                continue
            if cheb(a, target) != 1:
                continue
            if "Guard" in skills(a["_race"], a):
                cnt += 1
            else:
                etz = sum(1 for e in tz_enemies
                          if e["id"] != tz_exclude and e["state"] == 0
                          and cheb(a, e) == 1)
                if etz == 0:
                    cnt += 1
        return cnt

    for p in mine:
        p["_race"] = race
    for p in theirs:
        p["_race"] = opp_race

    for att in stand_mine:
        for dxx in (-1, 0, 1):
            for dyy in (-1, 0, 1):
                if dxx == 0 and dyy == 0:
                    continue
                cell = occ.get((att["x"] + dxx, att["y"] + dyy))
                if not cell or cell[0] != "theirs" or cell[1]["state"] != 0:
                    continue
                d = cell[1]
                att_st, def_st = att["st"], d["st"]
                if "Dauntless" in skills(race, att) and def_st > att_st:
                    att_st = def_st                     # dauntlessInOffer=True
                aa = count_assists(d, att["id"], on_mine, on_theirs, d["id"])
                da = count_assists(att, d["id"], on_theirs, on_mine, att["id"])
                a_tot, d_tot = att_st + aa, def_st + da
                if a_tot > 2 * d_tot:
                    dice = 3
                elif a_tot > d_tot:
                    dice = 2
                elif a_tot == d_tot:
                    dice = 1
                elif d_tot > 2 * a_tot:
                    dice = -3
                else:
                    dice = -2
                one_ok = (dice == 1 and "Block" in skills(race, att)
                          and not att["has_ball"])
                if dice >= 2 or one_ok:
                    off["BLOCK"] += 1

    # --- PICKUP ---
    if ball_on_ground:
        cands = []
        for p in stand_mine:
            dist = cheb_xy(p["x"], p["y"], ball["x"], ball["y"])
            if dist > p["ma"] + 2:
                continue
            target = 6 - p["ag"] + tz_on_me(ball["x"], ball["y"])
            if weather == "pouring_rain":
                target += 1
            target = max(2, min(6, target))
            chance = (7.0 - target) / 6.0
            if "SureHands" in skills(race, p):
                chance += (1.0 - chance) * chance
            cands.append(round(chance * 100) - dist * 5)
        if cands:
            cands.sort(reverse=True)
            off["PICKUP"] += 1
            if len(cands) > 1 and cands[0] - cands[1] <= 25:
                off["PICKUP"] += 1

    # --- PASS_ACTION (vč. hand-off větve; počítá se zvlášť) ---
    if i_have_ball and carrier_can_act:
        c_dist = dist_ez(carrier)
        ctz = tz_on_me(carrier["x"], carrier["y"])
        c_poor = carrier["ag"] <= 2 and "SureHands" not in skills(race, carrier)
        for tm in stand_mine:
            if tm["id"] == carrier["id"]:
                continue
            dist = cheb(carrier, tm)
            if dist > 10 or dist < 1:
                continue
            t_dist = abs(tm["x"] - ez)
            hand_off = dist == 1
            t_poor = tm["ag"] <= 2 and "SureHands" not in skills(race, tm)
            swap = c_poor and not t_poor and t_dist <= c_dist
            if not hand_off and t_dist >= c_dist:
                continue
            rng = pass_range(tm["x"] - carrier["x"], tm["y"] - carrier["y"])
            if rng is None:
                continue
            if weather == "blizzard" and rng in (L, B):
                continue
            throw_t = 7 - carrier["ag"] - PASS_MOD[rng] + ctz
            catch_t = 7 - tm["ag"] - 1 + tz_on_me(tm["x"], tm["y"])
            ch = lambda t: (7.0 - max(2, min(6, t))) / 6.0
            complete = ch(catch_t) if hand_off else ch(throw_t) * ch(catch_t)
            turns_left = 9 - turn_no
            emergency = (turns_left <= 2 and c_dist > carrier["ma"] + 2
                         and t_dist <= tm["ma"] + 2)
            worth = (swap and complete >= 0.5) if hand_off else complete >= 0.5
            if worth or emergency:
                off["PASS_ACTION"] += 1
                if hand_off:
                    off["_PASS_ACTION_handoff"] += 1
                else:
                    off["_PASS_ACTION_throw"] += 1

    # --- FOUL (jeden kandidát na foulera) ---
    downed = [e for e in on_theirs if e["state"] in (1, 2)]
    if downed:
        for p in stand_mine:
            if any(cheb(p, e) == 1 for e in downed):
                off["FOUL"] += 1

    # --- REPOSITION ---
    for p in stand_mine:
        if i_have_ball and p["id"] == carrier["id"]:
            continue
        if any(cheb(p, e) == 1 for e in stand_theirs):
            continue
        if ball_on_ground:
            if cheb_xy(p["x"], p["y"], ball["x"], ball["y"]) == 1:
                off["REPOSITION"] += 1
            else:
                free_adj = False
                for dxx in (-1, 0, 1):
                    for dyy in (-1, 0, 1):
                        if dxx == 0 and dyy == 0:
                            continue
                        ax, ay = ball["x"] + dxx, ball["y"] + dyy
                        if 0 <= ax <= 25 and 0 <= ay <= 14 and (ax, ay) not in occ:
                            free_adj = True
                if free_adj:
                    off["REPOSITION"] += 1
        else:
            off["REPOSITION"] += 1
    return off


def classify_played(events, my_ids, carrier_id):
    """Z eventů kola: co identifikovatelného se zahrálo. UNKNOWN = HAND_OFF
    (artefakt bb_module.cpp:325, stráž typeIdx<21 na 22prvkovém poli)."""
    played = Counter()
    moved = set()          # hráči, kteří se v kole už POHNULI (MOVE/GFI/DODGE)
    handoff_recv = {}      # receiver -> True
    pass_recv = {}
    blitzers = set()
    for e in events:
        et = e["type"]
        pid = e["player_id"]
        if et in ("MOVE", "GFI", "DODGE") and pid in my_ids:
            moved.add(pid)
        elif et == "BLOCK" and pid in my_ids:
            if pid in moved:
                played["BLITZ_blocks"] += 1
                blitzers.add(pid)
            else:
                played["BLOCK"] += 1
        elif et == "FOUL" and pid in my_ids:
            played["FOUL"] += 1
        elif et == "PICKUP" and pid in my_ids:
            played["PICKUP"] += 1
        elif et == "PASS" and pid in my_ids:
            played["PASS_throw"] += 1
            pass_recv[e["target_id"]] = True
        elif et == "UNKNOWN" and pid in my_ids:
            played["HAND_OFF_ev"] += 1
            handoff_recv[e["target_id"]] = True
        elif et == "TOUCHDOWN" and pid in my_ids:
            if handoff_recv.get(pid) and pass_recv:
                played["TD_chain"] += 1
            elif handoff_recv.get(pid):
                played["TD_handoff"] += 1
            elif pass_recv.get(pid):
                played["TD_pass"] += 1
            elif pid in blitzers:
                played["TD_blitz_and_score"] += 1
            else:
                played["TD_walk"] += 1
    return played


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    offer_turns = Counter()      # typ -> počet kol, kde je >=1 kandidát
    offer_cands = Counter()      # typ -> součet kandidátů
    played = Counter()
    our_turns = 0
    games = 0
    turn_no_max = 0
    handoff_detail = Counter()   # kontrola UNKNOWN eventů
    opp_handoffs = 0

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
        opp_race = g[f"{them}_race"]

        for t in g["turn_logs"]:
            # kontrola UNKNOWN eventů v KAŽDÉM kole (obě strany)
            for e in t["events"]:
                if e["type"] == "UNKNOWN":
                    adj = cheb_xy(e["from_x"], e["from_y"], e["to_x"], e["to_y"]) <= 1
                    handoff_detail["adjacent" if adj else "NOT_adjacent"] += 1

            if t["active_team"] != us:
                for e in t["events"]:
                    if e["type"] == "UNKNOWN":
                        opp_handoffs += 1
                continue
            our_turns += 1
            turn_no_max = max(turn_no_max, t["turn"])

            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            ball = {"x": t["ball_x"], "y": t["ball_y"], "held": t["ball_held"]}
            # míč drží NÁŠ hráč? has_ball je per-player v snímku
            off = analyze_turn_offers(mine, theirs, "dwarf", opp_race, ez,
                                      t["turn"], t["weather"], ball)
            for k, v in off.items():
                if v > 0:
                    if not k.startswith("_"):
                        offer_turns[k] += 1
                    offer_cands[k] += v

            my_ids = {p["id"] for p in mine}
            carrier_id = next((p["id"] for p in mine if p["has_ball"]), -1)
            pl = classify_played(t["events"], my_ids, carrier_id)
            played.update(pl)

    print(f"korpus: {games} her · našich (trpasličích) kol: {our_turns}")
    print(f"UNKNOWN eventy (obě strany): {sum(handoff_detail.values())} "
          f"({dict(handoff_detail)}) · z toho v kolech soupeře: {opp_handoffs}")

    print("\n=== NABÍDNUTO (rekonstrukce brány, snímek začátku kola = PODLAHA) ===")
    print(f"{'makro':>17s} {'kol s nabídkou':>14s} {'% kol':>7s} "
          f"{'kandidátů celkem':>17s} {'kand./kolo':>10s}")
    for m in MACROS:
        ot, oc = offer_turns[m], offer_cands[m]
        print(f"{m:>17s} {ot:>14d} {100.0 * ot / our_turns:>6.2f}% "
              f"{oc:>17d} {oc / our_turns:>10.3f}")
    print(f"  z PASS_ACTION: hand-off kandidátů "
          f"{offer_cands['_PASS_ACTION_handoff']}, "
          f"throw kandidátů {offer_cands['_PASS_ACTION_throw']}")

    print("\n=== ZAHRÁNO (z eventů; jen identifikovatelné) ===")
    for k in ["BLOCK", "BLITZ_blocks", "FOUL", "PICKUP", "PASS_throw",
              "HAND_OFF_ev", "TD_walk", "TD_blitz_and_score", "TD_handoff",
              "TD_pass", "TD_chain"]:
        v = played[k]
        print(f"  {k:>20s} {v:>7d}  ({v / games:.3f}/zápas, {games} her; "
              f"{100.0 * v / our_turns:.2f} % našich kol)")


if __name__ == "__main__":
    main()
