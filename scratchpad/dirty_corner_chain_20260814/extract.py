#!/usr/bin/env python3
"""Extrakce řetězu „špinavý roh → zámek → roh/tempo v N+1" (14.08.2026).

Jeden řádek = jedno naše kolo N (kolo, po kterém následuje soupeřovo kolo
a pak naše kolo N+1). Definice rohů/špinavosti/zámků/Δx se NEVYMÝŠLEJÍ:
importují se z diag_rules_checks_20260812 (adj, threatens, players, load)
a diag_exposure_scan_20260812 (Board, predictors) — jediná definice, dvě
použití, přesně jak si vynutil audit měřicího aparátu 13.08.

Snímky: S = logs[i] (začátek N), E = logs[i+1] (konec N = začátek soupeřova
kola), S2 = logs[i+2] (začátek N+1), E2 = logs[i+3] (konec N+1).

Výstup: JSONL.gz, jeden objekt na kolo. Analýza je zvlášť (analyze.py),
aby se dala iterovat bez přepočtu korpusu.
"""
import glob, gzip, json, math, sys
sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING
from diag_exposure_scan_20260812 import Board, predictors

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"
OUT = sys.argv[2] if len(sys.argv) > 2 else \
    "/home/jan/claude/bloodbowl/scratchpad/dirty_corner_chain_20260814/rows.jsonl.gz"
LIMIT = int(sys.argv[3]) if len(sys.argv) > 3 else 0


def corners(car, us, them):
    """Rohy klece PŘESNĚ podle diag_rules_checks_20260812 (K29).

    Vrací (filled_pozice, dirty_pozice, occ_map, polluter_hráči).
    Polluter = soupeř, který `threatens` a sousedí s obsazeným rohem.
    """
    diag = [(car["x"] + dx, car["y"] + dy) for dx in (-1, 1) for dy in (-1, 1)]
    occ = {(p["x"], p["y"]): p for p in us}
    filled = [d for d in diag if d in occ]
    threat = [p for p in them if threatens(p)]
    dirty = [d for d in filled
             if any(adj(d, (t["x"], t["y"])) for t in threat)]
    polluters = [t for t in threat
                 if any(adj(d, (t["x"], t["y"])) for d in filled)]
    return filled, dirty, occ, polluters


def main():
    paths = sorted(glob.glob(DATA + "/*.json.gz"))
    if LIMIT:
        paths = paths[:LIMIT]
    n_rows = 0
    stats = {"games": 0, "turns": 0}
    with gzip.open(OUT, "wt") as fout:
        for gi, path in enumerate(paths):
            r = load(path)
            if r["home_race"] == "dwarf":
                ours, opp_race = "home", r["away_race"]
            elif r["away_race"] == "dwarf":
                ours, opp_race = "away", r["home_race"]
            else:
                continue
            stats["games"] += 1
            theirs = "away" if ours == "home" else "home"
            fwd = 1 if ours == "home" else -1
            logs = r["turn_logs"]
            our_ids = {p["id"] for p in logs[0][f"{ours}_players"]}

            for i, S in enumerate(logs):
                if S["active_team"] != ours or i + 1 >= len(logs):
                    continue
                E = logs[i + 1]
                if S.get("touchdown") or E["half"] != S["half"]:
                    continue  # stejné vyřazení jako diag_rules_checks
                stats["turns"] += 1
                row = {"g": gi, "i": i, "half": S["half"], "turn": S["turn"],
                       "opp": opp_race}

                # ── ČÁST C: rozhodovací bod = ZAČÁTEK kola N ─────────────
                us_S = players(S, ours)
                them_S = players(S, theirs)
                bS = Board(S, ours)
                carS = next((p for p in us_S if p["has_ball"]), None)
                pollS, filledS_set = [], set()
                if carS is not None:
                    fS, dS, occS, pollS = corners(carS, us_S, them_S)
                    row["filled_S"], row["dirty_S"] = len(fS), len(dS)
                    filledS_set = set(fS)
                    row["opp3_S"] = sum(
                        1 for p in bS.th_st
                        if max(abs(p["x"] - carS["x"]),
                               abs(p["y"] - carS["y"])) <= 3)
                    # C1: očistitelnost BEZ blitzu — volný stojící soused
                    # pollutera (mimo nosiče a mimo těla na rozích)
                    hitters1 = hitters2 = 0
                    for t in pollS:
                        elig = [p for p in bS.us_st
                                if max(abs(p["x"] - t["x"]),
                                       abs(p["y"] - t["y"])) == 1
                                and not p["has_ball"]
                                and (p["x"], p["y"]) not in filledS_set]
                        if elig:
                            hitters1 += 1
                            if max(bS.dice(p, t, bS.us_st, bS.th_st)
                                   for p in elig) >= 2:
                                hitters2 += 1
                    row["pollS_n"] = len(pollS)
                    row["pollS_hitter"] = hitters1
                    row["pollS_hitter2d"] = hitters2

                # klasifikace blitzu z eventů kola N (chronologické pořadí):
                # blitz = BLOCK hráče, který PŘEDTÍM v kole měl MOVE/GFI
                moved_before = set()
                blitz_cls, blitz_tid = None, None
                poll_ids_S = {t["id"] for t in pollS}
                pos_S = {p["id"]: p for p in us_S + them_S}
                free_block_tids = set()
                for e in S["events"]:
                    if e["type"] in ("MOVE", "GFI") and e["player_id"] in our_ids:
                        moved_before.add(e["player_id"])
                    elif e["type"] == "BLOCK" and e["player_id"] in our_ids:
                        if e["player_id"] in moved_before and blitz_cls is None:
                            blitz_tid = e["target_id"]
                            t = pos_S.get(blitz_tid)
                            if carS is None:
                                blitz_cls = "no_ball"
                            elif t is None:
                                blitz_cls = "other"
                            elif max(abs(t["x"] - carS["x"]),
                                     abs(t["y"] - carS["y"])) == 1:
                                blitz_cls = "carrier_mark"
                            elif blitz_tid in poll_ids_S:
                                blitz_cls = "corner"
                            elif (t["x"] - carS["x"]) * fwd > 0:
                                blitz_cls = "wall_fwd"
                            else:
                                blitz_cls = "other"
                        else:
                            free_block_tids.add(e["target_id"])
                row["blitz_cls"] = blitz_cls or "none"
                if pollS:
                    row["pollS_freeblocked"] = sum(
                        1 for t in pollS if t["id"] in free_block_tids)
                    row["pollS_blitzed"] = 1 if blitz_cls == "corner" else 0

                # ── konec kola N ──────────────────────────────────────────
                us_E = players(E, ours)
                them_E = players(E, theirs)
                bE = Board(E, ours)
                row["blocks_N"] = sum(
                    1 for e in S["events"]
                    if e["type"] == "BLOCK" and e["player_id"] in our_ids)
                row["locked_N"] = sum(
                    1 for p in bE.us_st if bE.th_tz[(p["x"], p["y"])] > 0)

                car = next((p for p in us_E if p["has_ball"]), None)
                if car is not None:
                    filled, dirty, occ, poll = corners(car, us_E, them_E)
                    row["filled_N"] = len(filled)
                    row["dirty_N"] = len(dirty)
                    row["corner_ids"] = [occ[d]["id"] for d in filled]
                    row["dirty_ids"] = [occ[d]["id"] for d in dirty]
                    row["poll_N"] = len(poll)
                    # hustota: soupeři (stojící) do Čebyšev 3 od nosiče
                    row["opp3_N"] = sum(
                        1 for p in bE.th_st
                        if max(abs(p["x"] - car["x"]),
                               abs(p["y"] - car["y"])) <= 3)
                    # z toho zamčené rohy = tautologická část locked_N
                    dirty_set = set(dirty)
                    row["locked_corner_N"] = sum(
                        1 for p in bE.us_st
                        if (p["x"], p["y"]) in dirty_set)
                    P = predictors(bE)
                    row["reach0_N"] = P.get("REACH0")
                    row["fb2_N"] = P["FB2"]

                # Δx nosiče v TÉMŽ kole (S→E, týž nosič — vzor K9a/K36)
                if carS is not None and car is not None \
                        and carS["id"] == car["id"]:
                    row["dx_N"] = (car["x"] - carS["x"]) * fwd

                # ── C3: idle těla (K31 definice) a dosah na pollutery ────
                # idle: stojí na konci kola, nemá event, nenese míč, není
                # rohem klece, nesousedí se soupeřem (PŘESNĚ diag_rules K31)
                moved = {e["player_id"] for e in S["events"]}
                diagE = [(car["x"] + dx, car["y"] + dy)
                         for dx in (-1, 1) for dy in (-1, 1)] if car else []
                idles = []
                for p in us_E:
                    if p["state"] != STANDING or p["id"] in moved:
                        continue
                    if p["has_ball"] or (p["x"], p["y"]) in diagE:
                        continue
                    if any(adj((p["x"], p["y"]), (o["x"], o["y"]))
                           for o in them_E):
                        continue
                    idles.append(p)
                row["idle_n"] = len(idles)
                if pollS and idles:
                    ring = set()
                    for t in pollS:
                        for dx in (-1, 0, 1):
                            for dy in (-1, 0, 1):
                                if dx or dy:
                                    s = (t["x"] + dx, t["y"] + dy)
                                    if s not in bS.occ:
                                        ring.add(s)
                    row["idle_reach"] = sum(
                        1 for p in idles
                        if bS.bfs((p["x"], p["y"]), ring, p["ma"] + 2)
                        is not None)

                # ── soupeřovo kolo: okamžitá splatnost ────────────────────
                if i + 2 >= len(logs) or logs[i + 2]["half"] != S["half"]:
                    fout.write(json.dumps(row) + "\n"); n_rows += 1
                    continue
                S2 = logs[i + 2]
                opp_td = bool(E.get("touchdown"))
                if car is not None:
                    if opp_td:
                        row["ball_lost"] = 1   # ukradeno A proměněno
                    else:
                        row["ball_lost"] = 0 if any(
                            p["has_ball"] for p in players(S2, ours)) else 1
                # naši sražení během soupeřova kola (stáli na konci N,
                # nestojí na začátku N+1) — splátka „ztráta těl"
                if not opp_td:
                    st2 = {p["id"]: p["state"]
                           for p in S2[f"{ours}_players"]}
                    row["downed_opp_turn"] = sum(
                        1 for p in bE.us_st
                        if st2.get(p["id"], 3) != STANDING)

                # ── naše kolo N+1 ────────────────────────────────────────
                if opp_td or S2["active_team"] != ours:
                    fout.write(json.dumps(row) + "\n"); n_rows += 1
                    continue
                us_S2 = players(S2, ours)
                them_S2 = players(S2, theirs)
                b2 = Board(S2, ours)
                row["locked_S2"] = sum(
                    1 for p in b2.us_st if b2.th_tz[(p["x"], p["y"])] > 0)
                car2s = next((p for p in us_S2 if p["has_ball"]), None)
                # volná těla na začátku N+1: stojící, bez míče, mimo cizí TZ
                row["free_S2"] = sum(
                    1 for p in b2.us_st
                    if not p["has_ball"] and b2.th_tz[(p["x"], p["y"])] == 0)
                # osud těl ze špinavých rohů konce N
                if car is not None and row.get("dirty_ids"):
                    stmap = {p["id"]: p for p in S2[f"{ours}_players"]}
                    fates = []
                    for pid in row["dirty_ids"]:
                        q = stmap.get(pid)
                        if q is None or q["state"] == 3:
                            fates.append("out")
                        elif q["state"] != STANDING:
                            fates.append("down")
                        elif b2.th_tz[(q["x"], q["y"])] > 0:
                            fates.append("locked")
                        else:
                            fates.append("free")
                    row["dirty_fates"] = fates

                # polluteři na ZAČÁTKU N+1 (rozhodovací bod) + zásah blokem
                if car2s is not None:
                    f2, d2, occ2, poll2 = corners(car2s, us_S2, them_S2)
                    row["filled_S2"] = len(f2)
                    row["dirty_S2"] = len(d2)
                    row["poll_ids_S2"] = [p["id"] for p in poll2]
                    row["opp3_S2"] = sum(
                        1 for p in b2.th_st
                        if max(abs(p["x"] - car2s["x"]),
                               abs(p["y"] - car2s["y"])) <= 3)
                blk_targets = {e["target_id"] for e in S2["events"]
                               if e["type"] == "BLOCK"
                               and e["player_id"] in our_ids}
                row["blocks_N1"] = sum(
                    1 for e in S2["events"]
                    if e["type"] == "BLOCK" and e["player_id"] in our_ids)
                if row.get("poll_ids_S2"):
                    row["poll_hit"] = sum(1 for pid in row["poll_ids_S2"]
                                          if pid in blk_targets)

                row["td_N1"] = 1 if S2.get("touchdown") else 0
                if i + 3 >= len(logs) or logs[i + 3]["half"] != S["half"] \
                        or S2.get("touchdown"):
                    fout.write(json.dumps(row) + "\n"); n_rows += 1
                    continue
                E2 = logs[i + 3]
                us_E2 = players(E2, ours)
                them_E2 = players(E2, theirs)
                car2e = next((p for p in us_E2 if p["has_ball"]), None)
                if car2e is not None:
                    f3, d3, occ3, poll3 = corners(car2e, us_E2, them_E2)
                    row["filled_N1"] = len(f3)
                    row["dirty_N1"] = len(d3)
                    row["clean_N1"] = len(f3) - len(d3)
                    # Δx nosiče v N+1 — týž nosič, jinak degenerované (K36)
                    if car2s is not None and car2s["id"] == car2e["id"]:
                        row["dx_N1"] = (car2e["x"] - car2s["x"]) * fwd
                    # osud pollutera ze začátku N+1: stojí a špiní i na konci?
                    if row.get("poll_ids_S2"):
                        alive3 = {p["id"] for p in poll3}
                        st3 = {p["id"]: p["state"] for p in E2[f"{theirs}_players"]}
                        row["poll_still"] = sum(
                            1 for pid in row["poll_ids_S2"] if pid in alive3)
                        row["poll_down"] = sum(
                            1 for pid in row["poll_ids_S2"]
                            if st3.get(pid, 3) != STANDING)
                    # tělo ze špinavého rohu konce N slouží jako ČISTÝ roh N+1?
                    if row.get("dirty_ids"):
                        clean3_ids = {occ3[d]["id"] for d in f3
                                      if d not in set(d3)}
                        row["dirty_to_clean"] = sum(
                            1 for pid in row["dirty_ids"] if pid in clean3_ids)
                fout.write(json.dumps(row) + "\n"); n_rows += 1
            if gi % 200 == 0:
                print(f"  {gi}/{len(paths)} her, {n_rows} řádků", flush=True)
    print(f"HOTOVO: {stats['games']} her, {stats['turns']} kol, "
          f"{n_rows} řádků → {OUT}")


if __name__ == "__main__":
    main()
