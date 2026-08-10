"""Rozhodčí zvenčí: kolik trpasličích kol má SPLNĚNÝ PLÁN? (10.08.2026)

Zadání uživatele: "kolik kol má splněný plán". Klíčové rozlišení:
engine cíl tahu VYSLOVUJE (turn_planner.cpp:18 classifyTurnGoal), ale
skutečný plán staví jen pro PICKUP_BALL (:141) -- zbytek padá na
per-macro search(). Nemůžeme se tedy ptát enginu "splnils, co sis
určil"; musí to posoudit rozhodčí zvenčí ze záznamu.

Čte HOTOVÉ korpusy (diag_replay_mine_*_data), nespouští žádné zápasy
-> nesoutěží s běžící érou D-vlny 1 ani s M1.

=========================  PŘEDREGISTRACE  =========================
Zapsáno PŘED pohledem na výsledky.

Cíl tahu klasifikuji ZE SNÍMKU NA ZAČÁTKU TAHU, přesně podle
classifyTurnGoal (HOME útočí na x=25, AWAY na x=0; turnsLeft = 9 - turn):
  PICKUP_BALL  : míč není držen a je na hřišti
  SCORE_BALL   : držíme ho a dist(endzone) <= MA+2
  ADVANCE_BALL : držíme ho, ale endzone je dál
  DENY         : drží ho SOUPEŘ  (engine tu vrací NONE = žádný plán;
                 rozhodčí sem doplňuje povinnost, protože "míč první"
                 platí i v obraně)

SPLNĚNO (rung 1 = MÍČ):
  PICKUP_BALL  : v tahu je PICKUP událost NAŠEHO hráče (POKUS -- úspěch
                 je kostka, ne rozhodnutí). Zvlášť se vykáže i úspěch.
  ADVANCE_BALL : na konci tahu držíme míč A nosič dodržel POVINNÉ TEMPO
                 ROZVRHU. Uživatel 10.08.: "měřitelný cíl je TD v osmém
                 kole" -> tempo tohoto kola = ceil(zbývá polí / zbývá kol),
                 zbývá kol = 9 - turn. (Zpřísněno z "aspoň o 1 pole" ještě
                 PŘED prvním během -- výsledky jsem neviděl.)
  SCORE_BALL   : v tahu je naše TOUCHDOWN událost.
  DENY         : blokujeme nosiče, NEBO na konci tahu u něj stojí aspoň
                 jeden náš hráč (markování).

SPLNĚNO (rung 2 = BLITZ, univerzální, nezávisle na rung 1):
  某 náš hráč má v tomtéž tahu aspoň jeden MOVE a aspoň jeden BLOCK.
  ÚČEL BLITZU (uživatel 10.08.: "posun klece - blitz na proražení zdi"):
  blitz míří na tělo V CESTĚ -- cíl je blíž k naší endzone než nosič
  a do 4 polí od něj. Vykazuje se zvlášť, jen pro kola s naším míčem.
  ⚠️ DOLNÍ ODHAD: blitz z místa (deklaruji blitz, nehnu se, udeřím) je
  v záznamu k nerozeznání od běžného bloku -> podhodnocuje.

Hlavní číslo: podíl trpasličích kol se splněným rung 1, a totéž
přepočtené na "kolik z 8 kol půle".
Kontrolní skupina: TÁŽ metrika pro ostatní rasy (týž rozhodčí, tytéž
prahy) -- bez ní nevíme, jestli je číslo trpasličí, nebo enginové.
====================================================================
"""
import gzip
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path

# Korpus 20260721 vypadá, ale NEJDE použít: jeho turn_logs nemají u hráčů
# ma/st/ag/av (známý nález "replay log missing fields", 29.-30.07.) a bez MA
# nelze klasifikovat SCORE vs ADVANCE. Zůstává tedy jen korpus z 30.07.
CORPORA = ["diag_replay_mine_20260730_data"]


def last_positions(events, ids):
    """Kde hráč skončil: poslední to_x/to_y v událostech, jinak None."""
    pos = {}
    for ev in events:
        pid = ev.get("player_id")
        if pid in ids and ev.get("to_x", -1) >= 0:
            pos[pid] = (ev["to_x"], ev["to_y"])
    return pos


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def classify_and_judge(rec, ti):
    """Vrátí dict pro jeden tah, nebo None (tah bez plánu / nepoužitelný)."""
    turns = rec["turn_logs"]
    t = turns[ti]
    side = t["active_team"]                      # "home" / "away"
    race = rec["home_race"] if side == "home" else rec["away_race"]
    mine = t["home_players"] if side == "home" else t["away_players"]
    theirs = t["away_players"] if side == "home" else t["home_players"]
    my_ids = {p["id"] for p in mine}
    ez = 25 if side == "home" else 0
    events = t.get("events", [])

    # --- rung 2: blitz (nezávisle na cíli) --------------------------------
    moved, blocked = set(), set()
    for ev in events:
        pid = ev.get("player_id")
        if pid not in my_ids:
            continue
        if ev["type"] in ("MOVE", "GFI"):
            moved.add(pid)
        elif ev["type"] == "BLOCK":
            blocked.add(pid)
    blitz_ids = moved & blocked
    blitz = bool(blitz_ids)

    # --- rung 1: míč ------------------------------------------------------
    held = t["ball_held"]
    carrier_id = t["ball_carrier_id"]
    ball = (t["ball_x"], t["ball_y"])
    on_pitch = 0 <= ball[0] <= 25 and 0 <= ball[1] <= 14

    # Účel blitzu v režimu ROZVRH (uživatel 10.08.): "prorazit zeď" --
    # tělo V CESTĚ klece, ne libovolná šťavnatá oběť. Měřím: cíl blitzu je
    # blíž k naší cílové endzone než nosič a do 4 polí od něj.
    blitz_wall = None
    if blitz and held and carrier_id in my_ids:
        c = next(p for p in mine if p["id"] == carrier_id)
        cpos = (c["x"], c["y"])
        tpos = {p["id"]: (p["x"], p["y"]) for p in theirs}
        blitz_wall = False
        for ev in events:
            if ev["type"] == "BLOCK" and ev.get("player_id") in blitz_ids:
                tp = tpos.get(ev.get("target_id"))
                if tp and abs(tp[0]-ez) <= abs(cpos[0]-ez) and cheb(tp, cpos) <= 4:
                    blitz_wall = True


    if not held:
        if not on_pitch:
            return None
        goal = "PICKUP"
        attempts = [ev for ev in events
                    if ev["type"] == "PICKUP" and ev.get("player_id") in my_ids]
        ok = bool(attempts)
        extra = any(ev.get("success") for ev in attempts)
    elif carrier_id in my_ids:
        carrier = next(p for p in mine if p["id"] == carrier_id)
        start = (carrier["x"], carrier["y"])
        dist = abs(start[0] - ez)
        goal = "SCORE" if dist <= carrier["ma"] + 2 else "ADVANCE"
        if goal == "SCORE":
            ok = any(ev["type"] == "TOUCHDOWN" and ev.get("player_id") in my_ids
                     for ev in events)
            extra = None
        else:
            end = last_positions(events, {carrier_id}).get(carrier_id, start)
            gained = abs(start[0] - ez) - abs(end[0] - ez)
            lost = any(ev["type"] == "TURNOVER" for ev in events)
            # ROZVRH (uživatel 10.08.): cíl je TD v 8. kole -> povinné tempo
            # tohoto kola = zbývá polí / zbývá kol (turnsLeft = 9 - turn).
            turns_left = max(1, 9 - t["turn"])
            need = -(-dist // turns_left)          # ceil
            ok = (gained >= need) and not lost
            extra = (gained, need)
    else:
        goal = "DENY"
        their_ids = {p["id"] for p in theirs}
        if carrier_id not in their_ids:
            return None
        cs = next(p for p in theirs if p["id"] == carrier_id)
        cpos = last_positions(events, {carrier_id}).get(carrier_id, (cs["x"], cs["y"]))
        hit = any(ev["type"] == "BLOCK" and ev.get("player_id") in my_ids
                  and ev.get("target_id") == carrier_id for ev in events)
        endpos = last_positions(events, my_ids)
        marked = any(cheb(endpos.get(p["id"], (p["x"], p["y"])), cpos) == 1
                     for p in mine if p["state"] == 0)
        ok = hit or marked
        extra = None

    return {"race": race, "side": side, "half": t["half"], "turn": t["turn"],
            "goal": goal, "ok": ok, "blitz": blitz,
            "blitz_wall": blitz_wall, "extra": extra}


def main():
    rows = []
    for corp in CORPORA:
        root = Path(corp)
        if not root.exists():
            continue
        for fp in sorted(root.glob("g*.json.gz")):
            rec = json.load(gzip.open(fp, "rt"))
            for ti in range(len(rec["turn_logs"])):
                r = classify_and_judge(rec, ti)
                if r:
                    r["game"] = f"{corp[-13:-5]}/{fp.stem}"
                    rows.append(r)

    by_race = defaultdict(list)
    for r in rows:
        by_race[r["race"]].append(r)

    print("=" * 72)
    print("MÍRA SPLNĚNÍ PLÁNU — rozhodčí zvenčí, %d kol" % len(rows))
    print("=" * 72)
    hdr = f"{'rasa':<10} {'kol':>5} {'rung1':>7} {'z 8':>6} {'blitz':>7} {'oba':>7}"
    print(hdr)
    print("-" * len(hdr))
    for race in sorted(by_race, key=lambda r: -len(by_race[r])):
        rs = by_race[race]
        n = len(rs)
        ok = sum(r["ok"] for r in rs)
        bl = sum(r["blitz"] for r in rs)
        both = sum(r["ok"] and r["blitz"] for r in rs)
        print(f"{race:<10} {n:>5} {ok/n:>6.1%} {8*ok/n:>6.1f} "
              f"{bl/n:>6.1%} {both/n:>6.1%}")

    print()
    print("ROZPAD PODLE CÍLE TAHU (jak ho klasifikuje classifyTurnGoal)")
    print(f"{'rasa':<10} {'cíl':<8} {'kol':>5} {'splněno':>8} {'blitz':>7}")
    print("-" * 42)
    for race in sorted(by_race, key=lambda r: -len(by_race[r])):
        for goal in ("PICKUP", "ADVANCE", "SCORE", "DENY"):
            rs = [r for r in by_race[race] if r["goal"] == goal]
            if not rs:
                continue
            n = len(rs)
            print(f"{race:<10} {goal:<8} {n:>5} {sum(r['ok'] for r in rs)/n:>7.1%}"
                  f" {sum(r['blitz'] for r in rs)/n:>6.1%}")

    dw = [r for r in by_race.get("dwarf", []) if r["goal"] == "PICKUP"]
    if dw:
        succ = sum(1 for r in dw if r["extra"])
        print(f"\nPICKUP trpaslík: pokus v {sum(r['ok'] for r in dw)}/{len(dw)} kol, "
              f"z toho úspěšný sběr {succ}")
    print("\nBLITZ NA PRORAŽENÍ ZDI (jen kola, kdy držíme míč a blitz padl)")
    print(f"{'rasa':<10} {'blitzů':>7} {'v cestě':>9}")
    print("-" * 30)
    for race in sorted(by_race, key=lambda r: -len(by_race[r])):
        rs = [r for r in by_race[race] if r["blitz_wall"] is not None]
        if not rs:
            continue
        print(f"{race:<10} {len(rs):>7} {sum(r['blitz_wall'] for r in rs)/len(rs):>8.1%}")

    print("\nADVANCE — TEMPO ROZVRHU (zisk polí vs. povinné zbývá/kola)")
    print(f"{'rasa':<10} {'kol':>5} {'zisk⌀':>7} {'nutno⌀':>7} {'tempo OK':>9}")
    print("-" * 42)
    for race in sorted(by_race, key=lambda r: -len(by_race[r])):
        rs = [r for r in by_race[race] if r["goal"] == "ADVANCE"]
        if not rs:
            continue
        n = len(rs)
        gm = sum(r["extra"][0] for r in rs) / n
        nm = sum(r["extra"][1] for r in rs) / n
        print(f"{race:<10} {n:>5} {gm:>7.2f} {nm:>7.2f} {sum(r['ok'] for r in rs)/n:>8.1%}")
    adv = [r for r in by_race.get("dwarf", []) if r["goal"] == "ADVANCE"]
    if adv:
        g = Counter(r["extra"][0] for r in adv)
        print("trpaslík rozdělení zisku: "
              + ", ".join(f"{k}:{v}" for k, v in sorted(g.items())))


if __name__ == "__main__":
    sys.exit(main())
