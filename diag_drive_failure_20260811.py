#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
diag_drive_failure_20260811.py — rozklad trpasličích PŘIJÍMACÍCH drivů na příčiny.

Kategorie (vzájemně výlučné, v tomto pořadí):
  A  SKÓROVALI          — v drivu je naše TOUCHDOWN událost
  B  MÍČ NIKDY NEZÍSKÁN — v celém drivu jsme míč ani jednou nedrželi
  C  ZTRATILI JSME HO   — drželi jsme ho a na konci drivu ho drží soupeř / leží volný
  D  DOŠLA KOLA         — na konci půle míč držíme, ale nejsme v endzone
     D1 POZDNÍ START    — zbývalo_kol × 2.61 <  zbývalo_polí   (předregistrovaný test)
     D2 POMALÁ KLEC     — zbývalo_kol × 2.61 >= zbývalo_polí a přesto TD nepadl
     překryv            — D1 drivy, kde bylo NAVÍC dosažené tempo < 2.61 pole/kolo

Dělení na drivy (spolehlivěji než reset čísla kola):
  hranice drivu = (i) první turn_log hry, (ii) změna half,
  (iii) turn_log následující po logu s touchdown=True (mezi nimi proběhl kickoff
  a přestavění hřiště — snímek nové drivu je už PO výkopu).

Definice měřicích bodů (zapsáno předem, ne zpětně):
  * "první držení" = první start-of-turn snímek v drivu, kde míč drží trpaslík
    (snímky jsou pořizovány na začátku každého tahu = hned po konci předchozího).
  * zbývalo_kol   = počet trpasličích tahů, jejichž startovní snímek je >= snímek
    prvního držení (tj. tahy, které měl nosič celé k dispozici; tah, v němž míč
    sebral, se už odehrál). Odpovídá 9−turn, když držení začne na startu našeho tahu.
  * zbývalo_polí  = vzdálenost míče od soupeřovy endzone na snímku prvního držení
    (HOME útočí na x=25, AWAY na x=0).
  * dosažené tempo = (zbývalo_polí − vzdálenost na konci drivu) / zbývalo_kol;
    koncová pozice míče se u posledního tahu půle dopočítá z posledního MOVE/GFI
    nosiče v událostech (koncový snímek půle v datech není).
  * odpor v koridoru = počet STOJÍCÍCH soupeřů (state 0) v obdélníku mezi nosičem
    a endzone (x ostře mezi), y v pásu nosič±2; průměr přes naše tahy s držením.

Pouze čte data. Nespouští zápasy. Pouštět pod nice -19.
"""
import argparse
import glob
import gzip
import json
import os
import sys
from collections import Counter, defaultdict

DOCTRINAL_PACE = 2.61  # pole/kolo (pochod 20.9 / 8 kol) — předregistrováno v zadání
POSITIONS = ["Troll Slayer", "Longbeard", "Runner", "Blitzer", "Lineman",
             "Catcher", "Thrower", "Black Orc", "Ogre", "Wardancer", "Gutter Runner",
             "Storm Vermin", "Rat Ogre", "Treeman"]


def position_of(name):
    for p in POSITIONS:
        if name.startswith(p):
            return p
    return name.split(" +")[0]


def load_game(path):
    with gzip.open(path, "rt") as f:
        return json.load(f)


def build_id_map(game):
    """id -> ('home'|'away', name). Sjednoceno přes všechny snímky (soupisky se mění)."""
    m = {}
    for tl in game["turn_logs"]:
        for side in ("home", "away"):
            for p in tl[side + "_players"]:
                m[p["id"]] = (side, p["name"])
    return m


def split_drives(turn_logs):
    drives, cur = [], []
    for tl in turn_logs:
        if cur and (tl["half"] != cur[0]["half"] or cur[-1]["touchdown"]):
            drives.append(cur)
            cur = []
        cur.append(tl)
    if cur:
        drives.append(cur)
    return drives


def holder_side(tl, id_map):
    """Kdo drží míč na snímku: 'home'/'away'/None."""
    if tl.get("ball_held") and tl.get("ball_carrier_id", -1) >= 0:
        ent = id_map.get(tl["ball_carrier_id"])
        return ent[0] if ent else None
    return None


def td_scorer_side(tl, id_map):
    """Kdo dal touchdown na snímku: 'home'/'away'/None.

    ⚠️ NENÍ to `active_team`. CRP („Scoring in the opponent's turn"):
    *„a player holding the ball could be pushed into the End Zone by a block …
    then your team scores a touchdown immediately"* — tedy nosič sražený
    odsunem do endzony skóruje i v soupeřově kole. Změřeno 14.08. na korpusu
    3000 her: **15 z 2183 touchdownů (0,7 %) padne v kole druhého týmu**,
    takže atribuce podle `active_team` je u nich obrácená.
    Autoritou je hráč z TOUCHDOWN události, ne kdo je zrovna na tahu.
    """
    for e in tl.get("events", []):
        if str(e.get("type", "")).upper() == "TOUCHDOWN":
            ent = id_map.get(e["player_id"])
            if ent:
                return ent[0]
    return tl["active_team"] if tl.get("touchdown") else None


def receiving_side(drive, id_map, anomalies, ctx):
    s0 = drive[0]
    h = holder_side(s0, id_map)
    if h:
        return h  # touchback / míč rovnou v držení
    if s0["ball_x"] < 0:
        # míč mimo hřiště na startu drivu -> přijímá aktivní tým (přijímající hraje první)
        return s0["active_team"]
    side = "home" if s0["ball_x"] <= 12 else "away"
    if s0["active_team"] != side:
        anomalies.append("%s: přijímací strana (%s, míč x=%d) != aktivní tým na startu drivu (%s)"
                         % (ctx, side, s0["ball_x"], s0["active_team"]))
    return side


def is_bug_drive(drive):
    """Míč je celý drive mimo hřiště a nikdo ho nedrží — hra bez míče (bug enginu)."""
    return len(drive) > 1 and all(tl["ball_x"] < 0 and not tl["ball_held"] for tl in drive)


def dist_to_endzone(side, x):
    return (25 - x) if side == "home" else x


def track_final_turn_possession(last_tl, id_map, dwarf_side, start_holder):
    """Odhad držení po posledním tahu půle (koncový snímek v datech není).
    Vrací ('dwarf'|'opp'|'loose', poslední známá pozice nosiče-trpaslíka nebo None)."""
    carrier = last_tl["ball_carrier_id"] if last_tl.get("ball_held") else -1
    state = start_holder  # 'dwarf'|'opp'|None(loose)
    pos = None
    for e in last_tl["events"]:
        t = e["type"]
        pid = e["player_id"]
        ent = id_map.get(pid)
        pside = ent[0] if ent else None
        if t in ("PICKUP", "CATCH") and e["success"]:
            carrier = pid
            state = "dwarf" if pside == dwarf_side else "opp"
        elif t in ("PICKUP", "CATCH") and not e["success"]:
            if pid == carrier or state is not None:
                pass
            state = state if pid != carrier else None
        elif t == "KNOCKED_DOWN" and pid == carrier:
            carrier, state = -1, None
        elif t in ("DODGE", "GFI") and pid == carrier and not e["success"]:
            carrier, state = -1, None
        elif t == "PASS" and pid == carrier:
            carrier, state = -1, None  # míč ve vzduchu; případný CATCH ho zase chytí
        elif t in ("MOVE", "GFI") and pid == carrier and e["success"]:
            pos = (e["to_x"], e["to_y"])
    return (state if state is not None else "loose"), pos


def loss_cause(drive, k_last, id_map, dwarf_side):
    """Čím jsme míč definitivně ztratili. Skenuje události tahu k_last (poslední
    snímek s naším držením) a případně dalších tahů."""
    carrier = drive[k_last]["ball_carrier_id"]
    for k in range(k_last, len(drive)):
        tl = drive[k]
        own_turn = tl["active_team"] == dwarf_side
        for e in tl["events"]:
            t, pid = e["type"], e["player_id"]
            ent = id_map.get(pid)
            pside = ent[0] if ent else None
            if t == "KNOCKED_DOWN" and pid == carrier:
                return ("sražen blokem v našem tahu (both down apod.)" if own_turn
                        else "soupeřův blitz/blok srazil nosiče")
            if t == "DODGE" and pid == carrier and not e["success"]:
                return "neúspěšný dodge nosiče"
            if t == "GFI" and pid == carrier and not e["success"]:
                return "neúspěšné GFI nosiče"
            if t == "PUSH" and e["target_id"] == carrier and not (0 <= e["to_x"] <= 25 and 0 <= e["to_y"] <= 14):
                return "nosič vytlačen do davu (surf)"
            if t == "PASS" and pid == carrier and not e["success"]:
                return "neúspěšná přihrávka"
            if t == "CATCH" and pside == dwarf_side and not e["success"]:
                return "nechycená přihrávka/handoff"
            if t == "PICKUP" and pside == dwarf_side and not e["success"]:
                return "fumble při (znovu)sebrání"
            if t == "CATCH" and e["success"]:
                carrier = pid
                if pside != dwarf_side:
                    return "interception/chycení soupeřem"
            if t == "PICKUP" and e["success"] and pside != dwarf_side:
                return "volný míč sebral soupeř"
        if k + 1 < len(drive) and holder_side(drive[k + 1], id_map) == dwarf_side:
            carrier = drive[k + 1]["ball_carrier_id"]
    return "jiné/nejasné"


def corridor_resistance(tl, carrier_id, id_map, dwarf_side):
    ent = None
    for side in ("home", "away"):
        for p in tl[side + "_players"]:
            if p["id"] == carrier_id:
                ent = p
    if ent is None:
        return None
    cx, cy = ent["x"], ent["y"]
    opp = "away_players" if dwarf_side == "home" else "home_players"
    lo, hi = (cx, 25) if dwarf_side == "home" else (0, cx)
    n = 0
    for p in tl[opp]:
        if p["state"] == 0 and lo < p["x"] < hi and abs(p["y"] - cy) <= 2:
            n += 1
    return n


def analyze_receiving_drive(drive, next_start, id_map, dwarf_side, ctx, anomalies):
    """Analýza jednoho trpasličího přijímacího drivu. Vrací dict."""
    res = {"ctx": ctx, "half": drive[0]["half"], "start_turn": drive[0]["turn"],
           "n_our_turns": sum(1 for tl in drive if tl["active_team"] == dwarf_side)}
    res["kickoff_dist"] = dist_to_endzone(dwarf_side, drive[0]["ball_x"])
    # --- A: náš touchdown --- (atribuce podle STŘELCE, ne podle active_team)
    scorers = [td_scorer_side(tl, id_map) for tl in drive if tl["touchdown"]]
    our_td = any(s == dwarf_side for s in scorers)
    opp_td = any(s is not None and s != dwarf_side for s in scorers)
    # křížová kontrola přes skóre
    #   ⚠️ `next_start is None` u POSLEDNÍHO drivu hry není chyba: skóre na snímku
    #   je stav na ZAČÁTKU kola, takže TD z posledního kola se do žádného snímku
    #   nedostane (změřeno 14.08.: 124 z 2183 TD, 5,7 %). Výsledek hry je přitom
    #   správný — nese ho `game["home_score"]/["away_score"]`. Kontrola se proto
    #   u posledního drivu přeskakuje, ne aby se hlásila jako nesrovnalost.
    if next_start is not None:
        ds_home = next_start["home_score"] - drive[0]["home_score"]
        ds_away = next_start["away_score"] - drive[0]["away_score"]
        d_our = ds_home if dwarf_side == "home" else ds_away
        if (d_our > 0) != our_td:
            anomalies.append("%s: TD flag (%s) nesedí se skóre delta (%d)" % (ctx, our_td, d_our))

    # --- držení míče v drivu ---
    held_snaps = [k for k, tl in enumerate(drive) if holder_side(tl, id_map) == dwarf_side]
    held_by_event = any(e["type"] in ("PICKUP", "CATCH") and e["success"]
                        and id_map.get(e["player_id"], (None,))[0] == dwarf_side
                        for tl in drive for e in tl["events"])
    ever_held = bool(held_snaps) or held_by_event

    # --- první držení (snímkově) ---
    j = held_snaps[0] if held_snaps else None
    # neúspěšné trpasličí pickupy před prvním držením (fumble na volném míči)
    limit = j if j is not None else len(drive)
    res["failed_pickups_before_hold"] = sum(
        1 for tl in drive[:limit] for e in tl["events"]
        if e["type"] == "PICKUP" and not e["success"]
        and id_map.get(e["player_id"], (None,))[0] == dwarf_side)
    if j is not None:
        tlj = drive[j]
        res["first_hold_turn"] = tlj["turn"]
        res["first_hold_dist"] = dist_to_endzone(dwarf_side, tlj["ball_x"])
        res["turns_left"] = sum(1 for tl in drive[j:] if tl["active_team"] == dwarf_side)
        cid = tlj["ball_carrier_id"]
        res["first_carrier"] = position_of(id_map[cid][1]) if cid in id_map else "?"

    # --- kdo nesl míč (přes naše tahy) ---
    carriers = Counter()
    resist = []
    tempo = []
    for k, tl in enumerate(drive):
        if tl["active_team"] == dwarf_side and holder_side(tl, id_map) == dwarf_side:
            cid = tl["ball_carrier_id"]
            carriers[position_of(id_map[cid][1])] += 1
            r = corridor_resistance(tl, cid, id_map, dwarf_side)
            if r is not None:
                resist.append(r)
            # tempo přes náš tah: snímek k (start našeho tahu) -> snímek k+1
            if k + 1 < len(drive) and holder_side(drive[k + 1], id_map) == dwarf_side:
                tempo.append(dist_to_endzone(dwarf_side, tl["ball_x"])
                             - dist_to_endzone(dwarf_side, drive[k + 1]["ball_x"]))
    res["carrier_turns"] = dict(carriers)
    res["avg_resistance"] = (sum(resist) / len(resist)) if resist else None
    res["avg_tempo"] = (sum(tempo) / len(tempo)) if tempo else None

    # --- kategorie ---
    if our_td:
        res["cat"] = "A"
        return res
    if not ever_held:
        res["cat"] = "B"
        res["opp_td"] = opp_td
        res["opp_ever_held"] = any(holder_side(tl, id_map) not in (None, dwarf_side) for tl in drive)
        return res
    if opp_td:
        res["cat"] = "C"
        if held_snaps:
            res["loss_cause"] = loss_cause(drive, held_snaps[-1], id_map, dwarf_side)
            res["loss_turn"] = drive[held_snaps[-1]]["turn"]
            res["loss_dist"] = dist_to_endzone(dwarf_side, drive[held_snaps[-1]]["ball_x"])
        else:
            res["loss_cause"] = "drženo jen uvnitř tahu (event), snímkově nikdy"
        return res
    # drive skončil koncem půle -> kdo drží po posledním tahu?
    last = drive[-1]
    start_holder = {"home": "dwarf" if dwarf_side == "home" else "opp",
                    "away": "dwarf" if dwarf_side == "away" else "opp"}.get(
                        holder_side(last, id_map))
    end_state, end_pos = track_final_turn_possession(last, id_map, dwarf_side, start_holder)
    if end_state != "dwarf":
        res["cat"] = "C"
        if held_snaps:
            res["loss_cause"] = loss_cause(drive, held_snaps[-1], id_map, dwarf_side)
            res["loss_turn"] = drive[held_snaps[-1]]["turn"]
            res["loss_dist"] = dist_to_endzone(dwarf_side, drive[held_snaps[-1]]["ball_x"])
        else:
            res["loss_cause"] = "drženo jen uvnitř tahu (event), snímkově nikdy"
        return res
    # --- D: došla kola ---
    res["cat"] = "D"
    # koncová vzdálenost: poslední známá pozice míče/nosiče
    if end_pos is not None:
        end_dist = dist_to_endzone(dwarf_side, end_pos[0])
    else:
        end_dist = dist_to_endzone(dwarf_side, last["ball_x"])
    res["end_dist"] = end_dist
    if j is None:
        # míč sebrán až BĚHEM posledního tahu půle (žádný pozdější snímek):
        # zbývalo_kol = 0 -> extrémní pozdní start
        res["first_hold_turn"] = last["turn"]
        res["first_hold_dist"] = end_dist
        res["turns_left"] = 0
        res["hold_only_final_turn"] = True
        res["subcat"] = "D1" if end_dist > 0 else "D2"
        res["short_drive"] = res["n_our_turns"] * DOCTRINAL_PACE < res["kickoff_dist"]
        return res
    need = res["turns_left"] * DOCTRINAL_PACE
    res["subcat"] = "D1" if need < res["first_hold_dist"] else "D2"
    ach = (res["first_hold_dist"] - end_dist) / res["turns_left"] if res["turns_left"] else 0.0
    res["achieved_pace"] = ach
    res["also_slow"] = ach < DOCTRINAL_PACE
    # byl by drive stihnutelný i při okamžitém sebrání míče na výkopu?
    res["short_drive"] = res["n_our_turns"] * DOCTRINAL_PACE < res["kickoff_dist"]
    return res


def analyze_defensive_drive(drive, id_map, dwarf_side, ctx):
    """Doplněk: obranný drive — získali jsme míč? skórovali jsme?"""
    scorers = [td_scorer_side(tl, id_map) for tl in drive if tl["touchdown"]]
    our_td = any(s == dwarf_side for s in scorers)
    opp_td = any(s is not None and s != dwarf_side for s in scorers)
    held = any(holder_side(tl, id_map) == dwarf_side for tl in drive) or any(
        e["type"] in ("PICKUP", "CATCH") and e["success"]
        and id_map.get(e["player_id"], (None,))[0] == dwarf_side
        for tl in drive for e in tl["events"])
    if our_td:
        out = "STEAL+TD"
    elif opp_td:
        out = "soupeř TD"
    elif held:
        out = "míč získán, bez TD"
    else:
        out = "bez zisku míče, bez TD soupeře"
    return {"ctx": ctx, "outcome": out, "held": held, "our_td": our_td}


def fmt_avg(vals):
    vals = [v for v in vals if v is not None]
    return "%.2f" % (sum(vals) / len(vals)) if vals else "—"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data_dir")
    ap.add_argument("--details", action="store_true", help="vypiš řádek na každý drive")
    ap.add_argument("--dump", metavar="CTX", help="ASCII výpis drivu (ctx = soubor:díl)")
    args = ap.parse_args()

    files = sorted(glob.glob(os.path.join(args.data_dir, "g*.json.gz")))
    anomalies = []
    recv = []   # přijímací drivy trpaslíka
    defs = []   # obranné drivy trpaslíka
    bug_drives = []
    n_games = 0
    for f in files:
        try:
            game = load_game(f)
        except Exception as e:
            anomalies.append("%s: nečitelný (%s)" % (f, e))
            continue
        if "dwarf" not in (game["home_race"], game["away_race"]):
            continue
        n_games += 1
        dwarf_side = "home" if game["home_race"] == "dwarf" else "away"
        opp_race = game["away_race"] if dwarf_side == "home" else game["home_race"]
        id_map = build_id_map(game)
        drives = split_drives(game["turn_logs"])
        base = os.path.basename(f).replace(".json.gz", "")
        for di, drive in enumerate(drives):
            ctx = "%s:%d" % (base, di)
            next_start = drives[di + 1][0] if di + 1 < len(drives) else None
            if args.dump == ctx:
                dump_drive(drive, id_map)
                return
            if is_bug_drive(drive):
                intended = drive[0]["active_team"]  # přijímající hraje první
                bug_drives.append({"ctx": ctx, "opp": opp_race,
                                   "intended_recv_dwarf": intended == dwarf_side,
                                   "len": len(drive)})
                anomalies.append("%s: BUG — míč celý drive mimo hřiště (%d snímků), zamýšlený příjem: %s"
                                 % (ctx, len(drive), "trpaslík" if intended == dwarf_side else "soupeř"))
                continue
            rside = receiving_side(drive, id_map, anomalies, ctx)
            if rside == dwarf_side:
                r = analyze_receiving_drive(drive, next_start, id_map, dwarf_side, ctx, anomalies)
                r["opp"] = opp_race
                recv.append(r)
            else:
                d = analyze_defensive_drive(drive, id_map, dwarf_side, ctx)
                d["opp"] = opp_race
                defs.append(d)

    print("Soubory: %d | trpasličích her: %d | přijímacích drivů: %d | obranných: %d | BUG drivů (míč mimo hru): %d"
          % (len(files), n_games, len(recv), len(defs), len(bug_drives)))
    print()

    opps = sorted(set(r["opp"] for r in recv))
    cats = ["A", "B", "C", "D1", "D2"]

    def catkey(r):
        return r.get("subcat", r["cat"]) if r["cat"] == "D" else r["cat"]

    # --- hlavní tabulka ---
    print("=== PŘIJÍMACÍ DRIVY TRPASLÍKA: A/B/C/D1/D2 ===")
    hdr = "%-10s" + "%8s" * len(cats) + "%8s" % "celkem"
    print("%-10s" % "soupeř" + "".join("%8s" % c for c in cats) + "%8s" % "celkem")
    for opp in opps + ["VŠE"]:
        rs = recv if opp == "VŠE" else [r for r in recv if r["opp"] == opp]
        cc = Counter(catkey(r) for r in rs)
        row = "%-10s" % opp + "".join("%8s" % ("%d (%d%%)" % (cc.get(c, 0), round(100 * cc.get(c, 0) / len(rs))) if rs else "0")
                                      for c in cats) + "%8d" % len(rs)
        print(row)
    print()
    print("=== TOTÉŽ, JEN PLNÉ DRIVY (>=7 našich kol; odpovídá půlím bez předchozího TD) ===")
    full = [r for r in recv if r["n_our_turns"] >= 7]
    for opp in opps + ["VŠE"]:
        rs = full if opp == "VŠE" else [r for r in full if r["opp"] == opp]
        cc = Counter(catkey(r) for r in rs)
        print("%-10s" % opp + "".join("%8s" % ("%d (%d%%)" % (cc.get(c, 0), round(100 * cc.get(c, 0) / len(rs))) if rs else "0")
                                      for c in cats) + "%8d" % len(rs))
    print()
    # překryv D1 & pomalé tempo
    d1 = [r for r in recv if r.get("subcat") == "D1"]
    d2 = [r for r in recv if r.get("subcat") == "D2"]
    print("D1 s dosaženým tempem < %.2f (překryv 'pozdě I pomalu'): %d z %d"
          % (DOCTRINAL_PACE, sum(1 for r in d1 if r.get("also_slow")), len(d1)))
    print("D2 dosažené tempo: " + fmt_avg([r.get("achieved_pace") for r in d2])
          + " | D1 dosažené tempo: " + fmt_avg([r.get("achieved_pace") for r in d1]))
    print("D1 'krátký drive už od výkopu' (n_kol×%.2f < vzdál. na výkopu): %d z %d"
          % (DOCTRINAL_PACE, sum(1 for r in d1 if r.get("short_drive")), len(d1)))
    print("D1 fumble pickupy před 1. držením: " + fmt_avg([r.get("failed_pickups_before_hold") for r in d1])
          + " | zpoždění 1. držení vs start drivu (kola): "
          + fmt_avg([r["first_hold_turn"] - r["start_turn"] for r in d1 if "first_hold_turn" in r]))
    print("D1 s prvním držením až v posledním tahu půle: %d z %d"
          % (sum(1 for r in d1 if r.get("hold_only_final_turn")), len(d1)))
    print()

    # --- doplňkové rozpady ---
    print("=== DOPLŇKY PER KATEGORIE (vše-soupeři) ===")
    for c in cats:
        rs = [r for r in recv if catkey(r) == c]
        if not rs:
            continue
        car = Counter()
        for r in rs:
            car.update(r.get("carrier_turns", {}))
        fh = [r["first_hold_turn"] for r in rs if "first_hold_turn" in r]
        fd = [r["first_hold_dist"] for r in rs if "first_hold_dist" in r]
        print("[%s] n=%d | 1. držení: kolo %s, vzdál. %s polí | tempo %s polí/kolo | odpor %s soupeřů"
              % (c, len(rs),
                 fmt_avg(fh), fmt_avg(fd),
                 fmt_avg([r.get("avg_tempo") for r in rs]),
                 fmt_avg([r.get("avg_resistance") for r in rs])))
        if car:
            tot = sum(car.values())
            print("    nosič (kola s míčem): " + ", ".join("%s %d%%" % (k, round(100 * v / tot))
                                                           for k, v in car.most_common()))
        if c == "C":
            causes = Counter(r.get("loss_cause") for r in rs)
            print("    příčiny ztráty: " + ", ".join("%s %d×" % (k, v) for k, v in causes.most_common()))
            print("    ztraceno v kole: " + fmt_avg([r.get("loss_turn") for r in rs])
                  + " | vzdálenost od endzone při ztrátě: " + fmt_avg([r.get("loss_dist") for r in rs]) + " polí")
        if c == "A":
            fc = Counter(r.get("first_carrier") for r in rs)
            print("    první nosič: " + ", ".join("%s %d×" % (k, v) for k, v in fc.most_common()))
    print()

    # --- B detail: skončil drive TD soupeře? ---
    bs = [r for r in recv if r["cat"] == "B"]
    if bs:
        print("[B] z toho soupeř v drivu skóroval: %d z %d | soupeř míč držel: %d z %d | naše fumble pickupy/drive: %s"
              % (sum(1 for r in bs if r.get("opp_td")), len(bs),
                 sum(1 for r in bs if r.get("opp_ever_held")), len(bs),
                 fmt_avg([r.get("failed_pickups_before_hold") for r in bs])))
        print()

    # --- obranné drivy (doplněk: 'k míči jsme se dostali jinak') ---
    print("=== OBRANNÉ DRIVY TRPASLÍKA (doplněk) ===")
    oc = Counter(d["outcome"] for d in defs)
    for k, v in oc.most_common():
        print("  %-35s %d (%d%%)" % (k, v, round(100 * v / len(defs)) if defs else 0))
    print()

    if args.details:
        print("=== DETAIL DRIVŮ (přijímací) ===")
        for r in recv:
            print("%s opp=%-8s H%d start=T%d kol=%d cat=%-2s 1.drž=kolo %s vzdál=%s zbývalo_kol=%s fumbly=%d tempo=%s end=%s %s%s" % (
                r["ctx"], r["opp"], r["half"], r["start_turn"], r["n_our_turns"], catkey(r),
                r.get("first_hold_turn", "—"), r.get("first_hold_dist", "—"),
                r.get("turns_left", "—"), r.get("failed_pickups_before_hold", 0),
                ("%.2f" % r["avg_tempo"]) if r.get("avg_tempo") is not None else "—",
                r.get("end_dist", "—"),
                "KRÁTKÝ-DRIVE " if r.get("short_drive") else "",
                r.get("loss_cause", "")))
        print()

    if anomalies:
        print("=== ANOMÁLIE / NESROVNALOSTI (nezametat, hlásit) ===")
        for a in anomalies:
            print("  " + a)


def dump_drive(drive, id_map):
    """ASCII výpis snímků drivu pro ruční prohlídku situací."""
    for k, tl in enumerate(drive):
        grid = [["." for _ in range(26)] for _ in range(15)]
        for side, ch in (("home", "H"), ("away", "a")):
            for p in tl[side + "_players"]:
                if p["state"] == 3 or not (0 <= p["x"] <= 25 and 0 <= p["y"] <= 14):
                    continue
                c = ch if p["state"] == 0 else ch.lower() if side == "home" else "x"
                grid[p["y"]][p["x"]] = c
        bx, by = tl["ball_x"], tl["ball_y"]
        if 0 <= bx <= 25 and 0 <= by <= 14:
            grid[by][bx] = "*" if not tl["ball_held"] else "@"
        print("--- snímek %d: H%d T%d %s%s%s míč(%d,%d) drží=%s" % (
            k, tl["half"], tl["turn"], tl["active_team"],
            " TO" if tl["turnover"] else "", " TD" if tl["touchdown"] else "",
            bx, by, tl["ball_carrier_id"]))
        for row in grid:
            print("".join(row))


if __name__ == "__main__":
    main()
