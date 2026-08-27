#!/usr/bin/env python3
"""
FABLE 27.08. — AUDIT NABÍDEK MAKER PROTI DOSAHU

⚑ PROČ. Dnes se dvakrát nezávisle ukázal týž tvar vady (BLITZ_AND_SCORE
s `+3`, LEAP z TZ): makro se NABÍDNE do stavu, ve kterém nemůže splnit, co
slibuje. Otázka: kolikrát se ten tvar opakuje u ostatních maker.

⭐ CO SE POČÍTÁ. Pro každé naše kolo (snímek ZAČÁTKU kola) se z rekonstrukce
brány `analyze_turn_offers` (import z diag_fable_offered_played_20260817, dnes
srovnaná s enginem) vezme, CO se nabídlo, a pro každého kandidáta se
EMULUJE, co by expanze (`expand*` v macro_actions.cpp) skutečně ušla:

  - `walk()` je přepis `movePlayerToward` + `findMoveToward` + `scoreMoveAction`
    (macro_actions.cpp:42-178, 1369-1411): greedy krok za krokem, stejné
    skóre (10/pole, +20/+12 za TZ, +8 GFI, +6 sideline, manhattan tiebreak),
    stejné pořadí sousedů (position.cpp:5-16), stejná pravidla „max 1 pole
    zajížďky" a „smyčka", stejný rozpočet MOVE akcí (MA + 2 GFI,
    rules_engine.cpp:36-37).
  - `bfs()` je ideální cesta přes neobsazená pole (stejná jako pathfinder.cpp
    `canReachAdjacentTo`): říká, jestli cíl VŮBEC JDE dojít v rozpočtu.

⛔ KOSTKY SE NEHÁZÍ: dodge, GFI, hand-off, pass, blok -- všechno se počítá
jako úspěšné. Měří se jen GEOMETRIE a ROZPOČET POHYBU, tj. přesně to, co
brána slibuje a expanze vyžaduje. „Nedojde" tady znamená „nedojde ani když
padne všechno".

⚠️ OMEZENÍ (dědí se z rekonstrukce brány):
  - snímek je ZAČÁTEK kola: hasMoved/blitzUsed/... = false, MA plná;
    makra vzniklá až během kola nevidíme -> všechna N jsou PODLAHA,
  - naše strana = trpaslík (u dw-dw jen `home`),
  - ramena P38/P35 se berou VYPNUTÁ (baseline, stav korpusu 25.08.),
  - std::sort u BLITZ kandidátů není stabilní: při shodě skóre může engine
    vybrat jiný cíl než my (tisknou se remízy).

Použití: nice -n 19 python3 diag_offer_reach_audit_20260827.py [korpus] [N_her]
"""
import glob
import gzip
import json
import math
import sys
import time
from collections import Counter, deque

sys.path.insert(0, ".")
_m = __import__("diag_fable_offered_played_20260817")
analyze_turn_offers = _m.analyze_turn_offers
SKILLS = _m.SKILLS
pass_range = _m.pass_range
L, B = _m.L, _m.B

DATA = sys.argv[1] if len(sys.argv) > 1 else "blitzlanding_replic_20260825_corpus_data"
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 0

W, H = 26, 15
# position.cpp:5-16 -- pořadí sousedů rozhoduje remízy v findMoveToward
ADJ = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]


def on_pitch(x, y):
    return 0 <= x < W and 0 <= y < H


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


class Board:
    """Statický snímek: obsazenost + stojící soupeři (pro TZ)."""

    def __init__(self, mine, theirs, race, opp_race):
        self.occ = {}
        for p in mine:
            self.occ[(p["x"], p["y"])] = ("mine", p)
        for p in theirs:
            self.occ[(p["x"], p["y"])] = ("theirs", p)
        self.stand_theirs = [p for p in theirs if p["state"] == 0]
        self.stand_mine = [p for p in mine if p["state"] == 0]
        self.race, self.opp_race = race, opp_race

    def tz(self, x, y):
        return sum(1 for e in self.stand_theirs if cheb(x, y, e["x"], e["y"]) == 1)

    def free(self, x, y):
        return on_pitch(x, y) and (x, y) not in self.occ

    def move_player(self, p, to):
        """Dočasně přesune hráče (kvůli obsazenosti během chůze)."""
        del self.occ[(p["x"], p["y"])]
        self.occ[to] = ("mine", p)

    def bfs(self, start, budget, goal, through_target=None):
        """Nejlevnější cesta přes NEOBSAZENÁ pole (pathfinder.cpp:71-102).
        goal(x, y) -> bool. Vrací cenu nebo None."""
        sx, sy = start
        dist = {start: 0}
        q = deque([start])
        while q:
            cx, cy = q.popleft()
            c = dist[(cx, cy)]
            if (cx, cy) != start and goal(cx, cy):
                return c
            if c >= budget:
                continue
            for dx, dy in ADJ:
                n = (cx + dx, cy + dy)
                if n in dist or not on_pitch(*n):
                    continue
                if n in self.occ:
                    continue
                dist[n] = c + 1
                q.append(n)
        return None

    def walk(self, p, target, ma, max_steps, avoid=None, stop_x=None, gfi=2):
        """Emulace movePlayerToward (macro_actions.cpp:1369-1411).
        Vrací (výsledek, koncová pozice, kroků, kroků-z-TZ)."""
        pos = (p["x"], p["y"])
        mv = ma
        last = None
        steps = dodges = 0
        self_prone = p["state"] == 1
        for _ in range(max_steps):
            if pos == target and not self_prone:
                return ("arrived", pos, steps, dodges)
            if stop_x is not None and pos[0] == stop_x:
                return ("arrived", pos, steps, dodges)
            cur_tz = self.tz(*pos)
            cur_d = cheb(*pos, *target)
            best, bests = None, 10 ** 9
            cands = []
            for dx, dy in ADJ:
                n = (pos[0] + dx, pos[1] + dy)
                if not on_pitch(*n) or n in self.occ:
                    continue
                if mv - 1 < -gfi:
                    continue
                cands.append(n)
            if self_prone:
                cands.append(pos)          # rules_engine.cpp:260 -- vstávací MOVE
            for n in cands:
                if avoid is not None and n == avoid:
                    continue
                d = cheb(*n, *target)
                needs_gfi = mv <= 0
                if n == target:
                    s = d * 10 + (8 if needs_gfi else 0)
                else:
                    s = d * 10
                    man = abs(n[0] - target[0]) + abs(n[1] - target[1])
                    s += min(man - d, 9)
                    dtz = self.tz(*n)
                    if dtz > 0 and cur_tz == 0:
                        s += 20 * dtz
                    elif dtz > 0:
                        s += 12 * dtz
                    if needs_gfi:
                        s += 8
                    if n[1] <= 1 or n[1] >= 13:
                        s += 6
                if s < bests:
                    bests, best = s, n
            if best is None:
                return ("no_move", pos, steps, dodges)
            md = cheb(*best, *target)
            if md > cur_d + 1:
                return ("detour", pos, steps, dodges)
            if md >= cur_d and best == last:
                return ("loop", pos, steps, dodges)
            if self_prone:
                self_prone = False
                mv -= 3
                if best == pos:
                    continue                # vstal na místě, cíl == vlastní pole
            if cur_tz > 0:
                dodges += 1
            last = pos
            self.move_player(p, best)
            p["x"], p["y"] = best
            pos = best
            mv -= 1
            steps += 1
        if pos == target:
            return ("arrived", pos, steps, dodges)
        return ("exhausted", pos, steps, dodges)


def clone(players):
    return [dict(p) for p in players]


def count_assists(board, target, att_id, side_players, tz_enemies, tz_exclude):
    """getBlockDiceCount -> countAssists (přepis z diag_fable_offered_played)."""
    cnt = 0
    for a in side_players:
        if a["id"] in (att_id, target["id"]) or a["state"] != 0:
            continue
        if cheb(a["x"], a["y"], target["x"], target["y"]) != 1:
            continue
        if "Guard" in SKILLS[(a["_race"], a["name"])]:
            cnt += 1
        else:
            etz = sum(1 for e in tz_enemies
                      if e["id"] != tz_exclude and e["state"] == 0
                      and cheb(a["x"], a["y"], e["x"], e["y"]) == 1)
            if etz == 0:
                cnt += 1
    return cnt


def block_dice(board, att, d, mine, theirs, blitz):
    """getBlockDiceCount(state, att, def, isBlitz, dauntlessInOffer=True)
    (macro_actions.cpp:341-379). Asistence obránce se počítají u pole, kde
    blitzer STOJÍ TEĎ (P35 vypnuto)."""
    att_st, def_st = att["st"], d["st"]
    if "Dauntless" in SKILLS[(att["_race"], att["name"])] and def_st > att_st:
        att_st = def_st
    aa = count_assists(board, d, att["id"], mine, theirs, d["id"])
    da = count_assists(board, att, d["id"], theirs, mine, att["id"])
    a_tot, d_tot = att_st + aa, def_st + da
    if a_tot > 2 * d_tot:
        return 3
    if a_tot > d_tot:
        return 2
    if a_tot == d_tot:
        return 1
    if d_tot > 2 * a_tot:
        return -3
    return -2


def can_blitz_reach(board, blitzer, target, gfi=2):
    """rules_engine.cpp:88-108: sousedí, nebo BFS s rezervou 1 MP na blok."""
    if cheb(blitzer["x"], blitzer["y"], target["x"], target["y"]) == 1:
        return 0
    budget = blitzer["ma"] + gfi - 1
    if budget <= 0:
        return None
    tx, ty = target["x"], target["y"]
    return board.bfs((blitzer["x"], blitzer["y"]), budget,
                     lambda x, y: cheb(x, y, tx, ty) == 1)


def score_route_y(board, c, ez, dx):
    """expandScore (macro_actions.cpp:1441-1470): nejméně TZ na trase."""
    best_y, best_tz = c["y"], 999
    for yoff in (-2, -1, 0, 1, 2):
        ty = c["y"] + yoff
        if ty < 1 or ty > 13:
            continue
        cx, cy, s = c["x"], c["y"], 0
        guard = 0
        while (cx != ez or cy != ty) and guard < W + H:
            guard += 1
            if cy < ty:
                cy += 1
            elif cy > ty:
                cy -= 1
            if cx != ez:
                cx += dx
            if on_pitch(cx, cy):
                s += board.tz(cx, cy)
        if s < best_tz:
            best_tz, best_y = s, ty
    return best_y


def stall_steps(board, c, ez, turn):
    """carrierStallAwareSteps (macro_actions.cpp:1498-1518)."""
    dist = abs(c["x"] - ez)
    tr = max(1, 9 - turn)
    ideal = max(1, (dist + tr - 1) // tr)
    max_safe = max(1, c["ma"] // 2)
    steps = min(ideal, max_safe)
    blitzable = any(cheb(e["x"], e["y"], c["x"], c["y"]) <= e["ma"]
                    for e in board.stand_theirs)
    if tr <= 2 or blitzable:
        steps = min(ideal, c["ma"])
    return steps


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if LIMIT:
        files = files[:LIMIT]
    if not files:
        sys.exit(f"žádná data v {DATA}")

    R = Counter()          # všechny výsledky: (makro, kategorie) -> N
    mismatch = Counter()   # kontrola: moje kandidáty vs analyze_turn_offers
    games = our_turns = 0
    t0 = time.time()

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
        dx = 1 if us == "home" else -1
        my_ez = 0 if us == "home" else 25          # endzone, kterou bráníme
        opp_ez = my_ez
        opp_race = g[f"{them}_race"]

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            our_turns += 1
            mine0 = t[f"{us}_players"]
            theirs0 = t[f"{them}_players"]
            ball = {"x": t["ball_x"], "y": t["ball_y"], "held": t["ball_held"]}
            off = analyze_turn_offers(clone(mine0), clone(theirs0), "dwarf",
                                      opp_race, ez, t["turn"], t["weather"], ball)
            mine = clone(mine0)
            theirs = clone(theirs0)
            for p in mine:
                p["_race"] = "dwarf"
            for p in theirs:
                p["_race"] = opp_race
            board = Board(mine, theirs, "dwarf", opp_race)
            stand_mine = board.stand_mine
            stand_theirs = board.stand_theirs
            carrier = next((p for p in mine if p["has_ball"]), None)
            if carrier is not None and carrier["state"] != 0:
                carrier = None                      # canAct() vyžaduje STANDING
            i_have_ball = carrier is not None
            ball_on_ground = (not ball["held"]) and 0 <= ball["x"] <= 25
            weather = t["weather"]
            turn = t["turn"]
            # Pozorovatelný příznak „makro vrátilo prázdno -> END_TURN"
            # (macro_mcts.cpp:1180-1183): kolo bez jediné naší akce.
            my_ids = {p["id"] for p in mine}
            n_ev = sum(1 for e in t["events"] if e.get("player_id", -1) in my_ids)
            R[("KOLA", "našich kol")] += 1
            if n_ev == 0:
                R[("KOLA", "kolo BEZ JEDINÉ naší akce (hned END_TURN)")] += 1
                if i_have_ball:
                    R[("KOLA", "_…z toho s míčem v ruce")] += 1
                    if off.get("ADVANCE", 0):
                        R[("KOLA", "_…z toho s míčem a nabídnutým ADVANCE")] += 1
                elif ball_on_ground:
                    R[("KOLA", "_…z toho s míčem na zemi")] += 1
                else:
                    R[("KOLA", "_…z toho v obraně")] += 1

            def fresh():
                m = clone(mine)
                th = clone(theirs)
                return Board(m, th, "dwarf", opp_race), m, th

            # ---------------- SCORE ----------------
            if off.get("SCORE", 0):
                b, m, th = fresh()
                c = next(p for p in m if p["has_ball"])
                d = abs(c["x"] - ez)
                ideal = b.bfs((c["x"], c["y"]), c["ma"] + 2, lambda x, y: x == ez)
                by = score_route_y(b, c, ez, dx)
                res, pos, st, dg = b.walk(c, (ez, by), c["ma"], 14, stop_x=ez)
                R[("SCORE", "nabídnuto")] += 1
                if res == "arrived":
                    R[("SCORE", "dojde")] += 1
                elif ideal is None:
                    R[("SCORE", "NEDOJDE: ani ideální cesta (BFS) se nevejde")] += 1
                else:
                    R[("SCORE", f"NEDOJDE: greedy chůze selže ({res}), BFS by došel")] += 1
                if d == c["ma"] + 2:
                    R[("SCORE", "_dist == MA+2 (vyžaduje oba GFI)")] += 1

            # ---------------- ADVANCE ----------------
            if off.get("ADVANCE", 0):
                b, m, th = fresh()
                c = next(p for p in m if p["has_ball"])
                steps0 = stall_steps(b, c, ez, turn)
                steps = steps0
                tx = min(24, max(1, c["x"] + dx * steps))
                ty = c["y"] + (1 if c["y"] < 5 else -1 if c["y"] > 9 else 0)
                while steps > 0 and ((tx, ty) in b.occ or b.tz(tx, ty) > 0):
                    steps -= 1
                    tx = min(24, max(1, c["x"] + dx * steps))
                R[("ADVANCE", "nabídnuto")] += 1
                if steps <= 0:
                    R[("ADVANCE", "VZDÁ SE: záložní smyčka stáhla steps na 0")] += 1
                    side = any(b.free(c["x"] + ox, c["y"] + oy) and b.tz(c["x"] + ox, c["y"] + oy) == 0
                               for ox in range(-steps0, steps0 + 1)
                               for oy in range(-steps0, steps0 + 1)
                               if dx * ox >= 1)
                    if side:
                        R[("ADVANCE", "_…a přitom bylo volné TZ-free pole vpřed mimo přímku")] += 1
                else:
                    res, pos, st, dg = b.walk(c, (tx, ty), c["ma"], steps + 2)
                    if res == "arrived":
                        R[("ADVANCE", "dojde na zvolené pole")] += 1
                    else:
                        R[("ADVANCE", f"NEDOJDE: chůze selže ({res})")] += 1

            # ---------------- HAND_OFF_SCORE ----------------
            if off.get("HAND_OFF_SCORE", 0):
                n_c = 0
                for tm0 in stand_mine:
                    if tm0["id"] == carrier["id"]:
                        continue
                    if cheb(carrier["x"], carrier["y"], tm0["x"], tm0["y"]) > 2:
                        continue
                    rd = abs(tm0["x"] - ez)
                    if not (0 < rd <= tm0["ma"] + 2):
                        continue
                    n_c += 1
                    b, m, th = fresh()
                    c = next(p for p in m if p["has_ball"])
                    tm = next(p for p in m if p["id"] == tm0["id"])
                    R[("HAND_OFF_SCORE", "nabídnuto")] += 1
                    ok = True
                    if cheb(c["x"], c["y"], tm["x"], tm["y"]) > 1:
                        res, pos, st, dg = b.walk(c, (tm["x"], tm["y"]), c["ma"], c["ma"])
                        adj = cheb(pos[0], pos[1], tm["x"], tm["y"]) == 1
                        if not adj:
                            R[("HAND_OFF_SCORE", f"NEDOJDE: nosič neskončí vedle příjemce ({res})")] += 1
                            ok = False
                        else:
                            if st > 1:
                                R[("HAND_OFF_SCORE", "_nosič po dosažení sousedství dál PŘEŠLAPUJE (kroků > 1)")] += 1
                            if dg > 0:
                                R[("HAND_OFF_SCORE", "_nosič cestou opouští TZ (dodge)")] += 1
                    if ok:
                        ideal = b.bfs((tm["x"], tm["y"]), tm["ma"] + 2, lambda x, y: x == ez)
                        res, pos, st, dg = b.walk(tm, (ez, tm["y"]), tm["ma"], 14, stop_x=ez)
                        if res == "arrived":
                            R[("HAND_OFF_SCORE", "dojde (příjemce do endzóny)")] += 1
                        elif ideal is None:
                            R[("HAND_OFF_SCORE", "NEDOJDE: příjemce -- ani BFS")] += 1
                        else:
                            R[("HAND_OFF_SCORE", f"NEDOJDE: příjemce -- greedy selže ({res}), BFS by došel")] += 1
                if n_c != off["HAND_OFF_SCORE"]:
                    mismatch["HAND_OFF_SCORE"] += 1

            # ---------------- PASS_SCORE ----------------
            if off.get("PASS_SCORE", 0):
                best, best_s = None, -999
                for tm in stand_mine:
                    if tm["id"] == carrier["id"]:
                        continue
                    pd = cheb(carrier["x"], carrier["y"], tm["x"], tm["y"])
                    if pd < 2 or pd > 10:
                        continue
                    rd = abs(tm["x"] - ez)
                    if not (0 < rd <= tm["ma"] + 2):
                        continue
                    s = tm["ag"] * 5 - pd + (5 if "Catch" in SKILLS[("dwarf", tm["name"])] else 0)
                    if s > best_s:
                        best_s, best = s, tm
                if best is None:
                    mismatch["PASS_SCORE"] += 1
                else:
                    R[("PASS_SCORE", "nabídnuto")] += 1
                    rng = pass_range(best["x"] - carrier["x"], best["y"] - carrier["y"])
                    if rng is None:
                        R[("PASS_SCORE", "NEJDE: cíl mimo pravítko (žádná PASS akce)")] += 1
                    elif weather == "blizzard" and rng in (L, B):
                        R[("PASS_SCORE", "NEJDE: blizzard zakazuje long pass (žádná PASS akce)")] += 1
                    else:
                        b, m, th = fresh()
                        tm = next(p for p in m if p["id"] == best["id"])
                        ideal = b.bfs((tm["x"], tm["y"]), tm["ma"] + 2, lambda x, y: x == ez)
                        res, pos, st, dg = b.walk(tm, (ez, tm["y"]), tm["ma"], 14, stop_x=ez)
                        if res == "arrived":
                            R[("PASS_SCORE", "dojde (příjemce do endzóny)")] += 1
                        elif ideal is None:
                            R[("PASS_SCORE", "NEDOJDE: příjemce -- ani BFS")] += 1
                        else:
                            R[("PASS_SCORE", f"NEDOJDE: příjemce -- greedy selže ({res}), BFS by došel")] += 1

            # ---------------- CHAIN_SCORE ----------------
            if off.get("CHAIN_SCORE", 0):
                best, best_s = None, -999
                for relay in stand_mine:
                    if relay["id"] == carrier["id"]:
                        continue
                    pd = cheb(carrier["x"], carrier["y"], relay["x"], relay["y"])
                    if pd < 1 or pd > 10:
                        continue
                    for sc in stand_mine:
                        if sc["id"] in (carrier["id"], relay["id"]):
                            continue
                        if cheb(relay["x"], relay["y"], sc["x"], sc["y"]) > 2:
                            continue
                        sd = abs(sc["x"] - ez)
                        if not (0 < sd <= sc["ma"] + 2):
                            continue
                        s = relay["ag"] * 3 + sc["ag"] * 5 + sc["ma"] - pd * 2
                        if "Catch" in SKILLS[("dwarf", relay["name"])]:
                            s += 5
                        if "Catch" in SKILLS[("dwarf", sc["name"])]:
                            s += 3
                        if s > best_s:
                            best_s, best = s, (relay, sc)
                if best is None:
                    mismatch["CHAIN_SCORE"] += 1
                else:
                    relay0, sc0 = best
                    R[("CHAIN_SCORE", "nabídnuto")] += 1
                    rng = pass_range(relay0["x"] - carrier["x"], relay0["y"] - carrier["y"])
                    if rng is None:
                        R[("CHAIN_SCORE", "NEJDE: relay mimo pravítko (žádná PASS akce)")] += 1
                    elif weather == "blizzard" and rng in (L, B):
                        R[("CHAIN_SCORE", "NEJDE: blizzard zakazuje long pass")] += 1
                    else:
                        b, m, th = fresh()
                        relay = next(p for p in m if p["id"] == relay0["id"])
                        sc = next(p for p in m if p["id"] == sc0["id"])
                        ok = True
                        if cheb(relay["x"], relay["y"], sc["x"], sc["y"]) > 1:
                            res, pos, st, dg = b.walk(relay, (sc["x"], sc["y"]), relay["ma"], relay["ma"])
                            if cheb(pos[0], pos[1], sc["x"], sc["y"]) != 1:
                                R[("CHAIN_SCORE", f"NEDOJDE: relay neskončí vedle střelce ({res})")] += 1
                                ok = False
                        if ok:
                            ideal = b.bfs((sc["x"], sc["y"]), sc["ma"] + 2, lambda x, y: x == ez)
                            res, pos, st, dg = b.walk(sc, (ez, sc["y"]), sc["ma"], 14, stop_x=ez)
                            if res == "arrived":
                                R[("CHAIN_SCORE", "dojde (střelec do endzóny)")] += 1
                            elif ideal is None:
                                R[("CHAIN_SCORE", "NEDOJDE: střelec -- ani BFS")] += 1
                            else:
                                R[("CHAIN_SCORE", f"NEDOJDE: střelec -- greedy selže ({res}), BFS by došel")] += 1

            # ---------------- PICKUP ----------------
            if off.get("PICKUP", 0):
                cands = []
                for p in stand_mine:
                    dist = cheb(p["x"], p["y"], ball["x"], ball["y"])
                    if dist > p["ma"] + 2:
                        continue
                    target = 6 - p["ag"] + board.tz(ball["x"], ball["y"])
                    if weather == "pouring_rain":
                        target += 1
                    target = max(2, min(6, target))
                    chance = (7.0 - target) / 6.0
                    if "SureHands" in SKILLS[("dwarf", p["name"])]:
                        chance += (1.0 - chance) * chance
                    cands.append((round(chance * 100) - dist * 5, p))
                cands.sort(key=lambda c: -c[0])
                picks = cands[:1]
                if len(cands) > 1 and cands[0][0] - cands[1][0] <= 25:
                    picks = cands[:2]
                if len(picks) != off["PICKUP"]:
                    mismatch["PICKUP"] += 1
                for _, p0 in picks:
                    b, m, th = fresh()
                    p = next(q for q in m if q["id"] == p0["id"])
                    R[("PICKUP", "nabídnuto")] += 1
                    ideal = b.bfs((p["x"], p["y"]), p["ma"] + 2,
                                  lambda x, y: (x, y) == (ball["x"], ball["y"]))
                    res, pos, st, dg = b.walk(p, (ball["x"], ball["y"]), p["ma"], p["ma"] + 2)
                    if res == "arrived":
                        R[("PICKUP", "dojde na míč")] += 1
                    elif ideal is None:
                        R[("PICKUP", "NEDOJDE: ani BFS se nevejde do MA+2")] += 1
                    else:
                        R[("PICKUP", f"NEDOJDE: greedy selže ({res}), BFS by došel")] += 1

            # ---------------- CAGE ----------------
            if off.get("CAGE", 0):
                b, m, th = fresh()
                c = next(p for p in m if p["has_ball"])
                used = {c["id"]}
                R[("CAGE", "nabídnuto")] += 1
                open_c = filled = far = walkfail = ours = blocked = 0
                for sx, sy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
                    cp = (c["x"] + sx, c["y"] + sy)
                    if not on_pitch(*cp):
                        blocked += 1
                        continue
                    if cp in b.occ:
                        side_, q = b.occ[cp]
                        if side_ == "mine" and q["state"] == 0:
                            ours += 1           # roh už drží náš stojící
                        else:
                            blocked += 1        # soupeř nebo ležící: expanze roh přeskočí
                        continue
                    open_c += 1
                    mover, bd = None, 999
                    for p in m:
                        if p["id"] in used or p["state"] != 0:
                            continue
                        d = cheb(p["x"], p["y"], *cp)
                        if d < bd:
                            bd, mover = d, p
                    if mover is None:
                        continue
                    used.add(mover["id"])
                    res, pos, st, dg = b.walk(mover, cp, mover["ma"], 4)
                    if res == "arrived":
                        filled += 1
                    elif bd > 4:
                        far += 1
                    else:
                        walkfail += 1
                if open_c == 0 and ours == 4:
                    R[("CAGE", "klec už hotová (4 naše rohy) -> expanze nic nedělá")] += 1
                elif open_c == 0:
                    R[("CAGE", "žádný volný roh, klec NENÍ hotová (rohy drží soupeř/ležící/sideline) -> expanze nic nedělá")] += 1
                elif filled == 0:
                    R[("CAGE", "NIKDO nedojde na žádný volný roh")] += 1
                    if far and not walkfail:
                        R[("CAGE", "_…protože nejbližší volný je > 4 pole (limit 4 kroků)")] += 1
                else:
                    R[("CAGE", "aspoň 1 roh obsazen")] += 1
                R[("CAGE", "_rohů volných")] += open_c
                R[("CAGE", "_rohů dojito")] += filled
                R[("CAGE", "_rohů nedojito: nejbližší > 4 pole")] += far
                R[("CAGE", "_rohů nedojito: chůze selže do 4 kroků")] += walkfail

            # ---------------- BLITZ ----------------
            if off.get("BLITZ", 0):
                on_def = (not i_have_ball) and (not ball_on_ground)
                opp_carrier_id = next((p["id"] for p in theirs if p["has_ball"]), -1)
                cands = []
                for d in stand_theirs:
                    best_s = -999
                    for bl in stand_mine:
                        dice = block_dice(board, bl, d, mine, theirs, True)
                        s = dice * 2
                        if d["y"] <= 2 or d["y"] >= H - 3:
                            s += 3
                        elif d["y"] <= 4 or d["y"] >= H - 5:
                            s += 1
                        if on_def:
                            if d["id"] == opp_carrier_id:
                                s += 10
                            if d["ma"] + 2 >= abs(d["x"] - opp_ez):
                                s += 4
                            if sum(1 for q in stand_mine if cheb(q["x"], q["y"], d["x"], d["y"]) == 1) == 0:
                                s += 2
                        else:
                            if i_have_ball and cheb(d["x"], d["y"], carrier["x"], carrier["y"]) <= 2:
                                s += 2
                            if d["has_ball"]:
                                s += 5
                        best_s = max(best_s, s)
                    cands.append((best_s, d))
                cands.sort(key=lambda c: -c[0])
                k = min(2 if (on_def and len(cands) > 1) else 1, len(cands))
                if k != off["BLITZ"]:
                    mismatch["BLITZ"] += 1
                tie = len(cands) > k and cands[k - 1][0] == cands[k][0]
                any_reach = any(can_blitz_reach(board, bl, d) is not None
                                for _, d in cands for bl in stand_mine)
                R[("BLITZ", "_kol s nabídkou BLITZ")] += 1
                if not any_reach:
                    R[("BLITZ", "_kol, kde NIKDO nedosáhne na ŽÁDNÉHO soupeře (nezávisle na výběru cíle)")] += 1
                elif not any(can_blitz_reach(board, bl, cands[0][1]) is not None for bl in stand_mine):
                    R[("BLITZ", "_kol, kde top-1 cíl je nedosažitelný, ač JINÝ soupeř dosažitelný je")] += 1
                for s, d in cands[:k]:
                    R[("BLITZ", "nabídnuto")] += 1
                    if tie:
                        R[("BLITZ", "_remíza skóre na hraně výběru (cíl nejistý)")] += 1
                    reach = [bl for bl in stand_mine if can_blitz_reach(board, bl, d) is not None]
                    if not reach:
                        R[("BLITZ", "NEJDE: žádný náš hráč nedosáhne k cíli (žádná BLITZ akce)")] += 1
                        near = min(cheb(bl["x"], bl["y"], d["x"], d["y"]) for bl in stand_mine)
                        if near > 6 + 2:
                            R[("BLITZ", "_…cíl dál než MA6+2 od kohokoli")] += 1
                    else:
                        R[("BLITZ", "dosažitelný aspoň jedním blitzerem")] += 1

            # ---------------- BLITZ_AND_SCORE ----------------
            if off.get("BLITZ_AND_SCORE", 0):
                d = abs(carrier["x"] - ez)
                gfi = 2
                best, bd = None, 999
                for e in stand_theirs:
                    if abs(e["x"] - ez) >= d:
                        continue
                    yd = abs(e["y"] - carrier["y"])
                    if yd > 2:
                        continue
                    xd = abs(e["x"] - carrier["x"])
                    if xd <= 2 and xd + yd <= 3 and xd + yd < bd:
                        bd, best = xd + yd, e
                R[("BLITZ_AND_SCORE", "nabídnuto")] += 1
                reach = {}
                for bl in stand_mine:
                    c_ = can_blitz_reach(board, bl, best)
                    if c_ is not None:
                        reach[bl["id"]] = c_
                if not reach:
                    R[("BLITZ_AND_SCORE", "NEJDE: nikdo nedosáhne na blokujícího (žádná BLITZ akce)")] += 1
                elif set(reach) == {carrier["id"]}:
                    left = carrier["ma"] - reach[carrier["id"]] - 1
                    if d > left + gfi:
                        R[("BLITZ_AND_SCORE", "NEDOJDE: blitzovat může jen nosič a po bloku mu nezbývá dosah")] += 1
                    else:
                        R[("BLITZ_AND_SCORE", "jen nosič blitzuje, dosah po bloku stačí")] += 1
                else:
                    R[("BLITZ_AND_SCORE", "blitzovat může spoluhráč")] += 1
                    b, m, th = fresh()
                    c = next(p for p in m if p["has_ball"])
                    ideal = b.bfs((c["x"], c["y"]), c["ma"] + 2, lambda x, y: x == ez)
                    if ideal is None:
                        R[("BLITZ_AND_SCORE", "_…ale nosič nemá cestu ani BFS (blokující stojí v ní)")] += 1

            # ---------------- BLOCK (Dauntless jako předpoklad) ----------------
            if off.get("BLOCK", 0):
                R[("BLOCK", "nabídnuto")] += off["BLOCK"]
                for att in stand_mine:
                    if "Dauntless" not in SKILLS[("dwarf", att["name"])]:
                        continue
                    for dxx in (-1, 0, 1):
                        for dyy in (-1, 0, 1):
                            cell = board.occ.get((att["x"] + dxx, att["y"] + dyy))
                            if not cell or cell[0] != "theirs" or cell[1]["state"] != 0:
                                continue
                            d = cell[1]
                            if d["st"] <= att["st"]:
                                continue
                            with_d = block_dice(board, att, d, mine, theirs, False)
                            att2 = dict(att)
                            att2["name"] = "Longbeard"     # bez Dauntless
                            without = block_dice(board, att2, d, mine, theirs, False)
                            offered = with_d >= 2 or (with_d == 1 and not att["has_ball"])
                            offered_wo = without >= 2 or (without == 1 and not att["has_ball"])
                            if offered:
                                R[("BLOCK", "_nabídka Slayera proti silnějšímu (Dauntless vyrovnán)")] += 1
                                if not offered_wo:
                                    R[("BLOCK", "_…bez vyrovnání by se NENABÍDLA (hod Dauntless rozhodne)")] += 1

            # ---------------- REPOSITION ----------------
            if off.get("REPOSITION", 0):
                b, m, th = fresh()
                on_defense = (not i_have_ball) and (not ball_on_ground)
                c = next((p for p in m if p["has_ball"]), None)
                if c is not None and c["state"] != 0:
                    c = None
                opp_c = next((p for p in th if p["has_ball"] and p["state"] == 0), None) if on_defense else None
                placed = {"hunter": False, "receiver": False, "cagetag": False,
                          "intercept": False, "safety": False, "marker": False}
                turns_left = max(0, 9 - turn)
                ez_guard = 0
                screen_slot = 0
                opp_threats = 0
                if on_defense:
                    for op in th:
                        if op["state"] != 0:
                            continue
                        if op["ma"] + 2 >= abs(op["x"] - opp_ez) and \
                           sum(1 for q in m if q["state"] == 0 and cheb(q["x"], q["y"], op["x"], op["y"]) == 1) == 0:
                            opp_threats += 1
                n_c = 0
                for p in m:
                    if p["state"] != 0:
                        continue
                    if c is not None and p["id"] == c["id"]:
                        continue
                    if any(cheb(p["x"], p["y"], e["x"], e["y"]) == 1 for e in th if e["state"] == 0):
                        continue
                    strat = None
                    target = None
                    if ball_on_ground:
                        if cheb(p["x"], p["y"], ball["x"], ball["y"]) == 1:
                            target = (p["x"], p["y"]); strat = "míč: už vedle (stůj)"
                        else:
                            ba, bdd = None, 999
                            for dxx, dyy in ADJ:
                                a = (ball["x"] + dxx, ball["y"] + dyy)
                                if not on_pitch(*a) or a in b.occ:
                                    continue
                                dd = cheb(p["x"], p["y"], *a)
                                if dd < bdd:
                                    bdd, ba = dd, a
                            if ba is None:
                                continue
                            target = ba; strat = "míč: k volnému poli vedle míče"
                    elif c is not None:
                        cd = cheb(p["x"], p["y"], c["x"], c["y"])
                        if not placed["hunter"] and p["ma"] >= 7 and cd > 4:
                            target = (c["x"], c["y"]); strat = "útok: hunter (MA>=7)"
                            placed["hunter"] = True
                        elif not placed["receiver"] and turns_left <= 2 and p["ma"] >= 6 and cd > 3:
                            ry = min(12, max(2, c["y"] + (2 if p["y"] > c["y"] else -2)))
                            rx = min(24, max(1, ez - dx * 3))
                            target = (rx, ry); strat = "útok: receiver u endzóny"
                            placed["receiver"] = True
                        elif cd <= 3:
                            target = (c["x"] + dx * 2, c["y"]); strat = "útok: 2 pole před nosiče"
                        else:
                            target = (c["x"], c["y"]); strat = "útok: k nosiči (cíl = nosičovo pole)"
                    elif on_defense:
                        used_tag = False
                        if not placed["cagetag"] and opp_c is not None:
                            cage_n = sum(1 for o in th if o["id"] != opp_c["id"] and o["state"] == 0
                                         and cheb(opp_c["x"], opp_c["y"], o["x"], o["y"]) == 1)
                            if cage_n >= 2:
                                bc, mtz = (opp_c["x"], opp_c["y"]), 999
                                for dxx, dyy in ADJ:
                                    a = (opp_c["x"] + dxx, opp_c["y"] + dyy)
                                    if not on_pitch(*a) or a in b.occ:
                                        continue
                                    ftz = sum(1 for o in th if o["state"] == 0 and cheb(a[0], a[1], o["x"], o["y"]) == 1)
                                    if ftz < mtz:
                                        mtz, bc = ftz, a
                                if bc != (opp_c["x"], opp_c["y"]):
                                    target = bc; strat = "obrana: roh soupeřovy klece"
                                    placed["cagetag"] = True
                                    used_tag = True
                        if not used_tag:
                            used_int = False
                            if not placed["intercept"] and opp_c is not None:
                                dx_opp = -dx
                                lane = (min(24, max(1, (opp_c["x"] + my_ez) // 2)),
                                        min(13, max(1, opp_c["y"])))
                                goal_side = (p["x"] - opp_c["x"]) * dx_opp >= -2
                                if goal_side and cheb(p["x"], p["y"], *lane) <= p["ma"] * 2:
                                    target = lane; strat = "obrana: intercept lane"
                                    placed["intercept"] = True
                                    used_int = True
                            if not used_int:
                                if not placed["safety"] and p["ma"] >= 6:
                                    target = (my_ez, 7); strat = "obrana: safety (vlastní endzóna, y=7)"
                                    placed["safety"] = True
                                elif not placed["marker"] and opp_c is not None:
                                    target = (opp_c["x"], opp_c["y"]); strat = "obrana: marker (cíl = pole nosiče)"
                                    placed["marker"] = True
                                elif opp_threats > 0 and ez_guard < 2:
                                    gx = min(24, max(1, my_ez + dx * 4))
                                    target = (gx, 5 if ez_guard == 0 else 9); strat = "obrana: endzone guard"
                                    ez_guard += 1
                                else:
                                    bx = ball["x"] if 0 <= ball["x"] <= 25 else ez
                                    sx_ = min(24, max(1, (bx + my_ez) // 2))
                                    target = (sx_, (3, 5, 7, 9, 11)[screen_slot % 5]); strat = "obrana: screen"
                                    screen_slot += 1
                    else:
                        target = (p["x"] + dx * 3, 7); strat = "jinak: 3 vpřed, střed"
                    n_c += 1
                    R[("REPOSITION", "nabídnuto")] += 1
                    R[("REPOSITION", f"  [{strat}] nabídnuto")] += 1
                    if target == (p["x"], p["y"]):
                        R[("REPOSITION", f"  [{strat}] cíl = vlastní pole (stůj)")] += 1
                        continue
                    if not on_pitch(*target):
                        R[("REPOSITION", f"  [{strat}] CÍL MIMO HŘIŠTĚ")] += 1
                        continue
                    dist = cheb(p["x"], p["y"], *target)
                    if target in b.occ:
                        R[("REPOSITION", f"  [{strat}] cíl je OBSAZENÉ pole (jen přiblížení, dojít nelze)")] += 1
                        bb_, mm, tt = fresh()
                        pp = next(q for q in mm if q["id"] == p["id"])
                        avoid = None if ball["held"] else (ball["x"], ball["y"])
                        res, pos, st, dg = bb_.walk(pp, target, pp["ma"], pp["ma"], avoid=avoid)
                        fd = cheb(pos[0], pos[1], *target)
                        if dist <= p["ma"]:
                            if fd == 1:
                                R[("REPOSITION", f"  [{strat}] _cíl obsazený v dosahu MA: skončí VEDLE cíle")] += 1
                            else:
                                R[("REPOSITION", f"  [{strat}] _cíl obsazený v dosahu MA: skončí DÁL než vedle (d={fd})")] += 1
                        continue
                    if dist > p["ma"]:
                        R[("REPOSITION", f"  [{strat}] NEDOJDE: cíl dál než MA (bez GFI)")] += 1
                        continue
                    bb_, mm, tt = fresh()
                    pp = next(q for q in mm if q["id"] == p["id"])
                    avoid = None if ball["held"] else (ball["x"], ball["y"])
                    res, pos, st, dg = bb_.walk(pp, target, pp["ma"], pp["ma"], avoid=avoid)
                    if res == "arrived":
                        R[("REPOSITION", f"  [{strat}] dojde")] += 1
                    else:
                        R[("REPOSITION", f"  [{strat}] NEDOJDE: chůze selže ({res}) v dosahu MA")] += 1
                if n_c != off["REPOSITION"]:
                    mismatch["REPOSITION"] += 1

    el = time.time() - t0
    print(f"korpus {DATA}: {games} her, {our_turns} našich kol, {el:.1f} s "
          f"({el / max(1, games):.2f} s/hra -> 3 000 her ≈ {el / max(1, games) * 3000 / 60:.1f} min)")
    print(f"kontrola rekonstrukce kandidátů vs analyze_turn_offers (kol s neshodou): "
          f"{dict(mismatch) or 'žádná'}\n")
    order = ["KOLA", "SCORE", "ADVANCE", "HAND_OFF_SCORE", "PASS_SCORE", "CHAIN_SCORE",
             "PICKUP", "CAGE", "BLITZ", "BLITZ_AND_SCORE", "BLOCK", "REPOSITION"]
    for mac in order:
        rows = [(k[1], v) for k, v in R.items() if k[0] == mac]
        if not rows:
            print(f"=== {mac}: nic nenabídnuto ===\n")
            continue
        n = R.get((mac, "nabídnuto"), 0)
        print(f"=== {mac} (nabídnuto {n}, {n / our_turns:.3f}/kolo) ===")
        for k, v in sorted(rows, key=lambda r: (-r[1], r[0])):
            if k == "nabídnuto":
                continue
            pct = f"{100.0 * v / n:5.1f} %" if n else ""
            print(f"  {v:7d} {pct:>8}  {k}")
        print()


if __name__ == "__main__":
    main()
