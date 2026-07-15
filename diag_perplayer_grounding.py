#!/usr/bin/env python3
"""Replay grounding for the per-player feature candidate list (Okruh 1-4).

WHY (2026-07-15): team1_brief_per_player.md derives its candidate features
(cage-corner eff_ST/Guard, carrier_blitzable BFS-vs-Chebyshev, is_free_receiver,
net_st_for_block, adjacent_to_sideline) from tactical THEORY; the "Okruh 5 /
replay analysis" checklist item that was supposed to ground them in observed
AI mistakes was never done. This script generates a small fresh batch of
production-config mirror games WITH full turn logs (positions per player per
turn -- the aggregate game_*.jsonl feature logs cannot answer per-player
questions) and mines them for exactly those failure modes.

CONFIG matches the production gate mirror (run_iteration.py _gate_game):
macro_mcts vs macro_mcts, weights_best.json both sides, MCTS=100, epsilon=0,
vf_blend=0.0, TV=1200 (developed rosters WITH Guard), dirichlet_alpha=0.0,
exploration_c=1.0, races cycling _RACES[i] vs _RACES[i+1]. Policy file loaded
(weights_policy.json) to match the self-play prior-floor regime, same as
diag_first_possession.py.

PLAYER ID -> ROSTER TEMPLATE mapping replicates buildTeam()
(engine/src/game_simulator.cpp:161-215): specialists (template 1..n-1, in
template order) fill slots 10 downward, linemen (template 0) fill the
remaining low slots; player id = baseId + slot (home baseId=1, away 12).
The mapping is verified at runtime against bb_engine.setup_half() stats.

USAGE (repo root, venv, engine/build on path; do not run during training):
    python3 diag_perplayer_grounding.py run <label> [N]     # generate games
    python3 diag_perplayer_grounding.py analyze <label>     # mine + report
    python3 diag_perplayer_grounding.py show <label> <game> <snap> [note]
        # role-labelled ASCII pitch of one snapshot

Data: diag_perplayer_grounding_data/<label>/g****.json.gz (full turn logs).
"""
from __future__ import annotations

import gzip
import json
import sys
import time
from collections import Counter, defaultdict
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
BASE_SEED = 20260715
DEFAULT_N = 100
DATA_ROOT = Path("diag_perplayer_grounding_data")

# ---------------------------------------------------------------------------
# TV1200 roster tables (engine/src/roster.cpp:500-589), template order as in
# the C++ initializer. Tuple: (name, role_letter, MA, ST, AG, AV, skills, qty)
# Template 0 is the lineman "fill" type.
# ---------------------------------------------------------------------------
ROSTERS = {
    "human": [
        ("Lineman", "L", 6, 3, 3, 8, set(), 11),
        ("Blitzer+Guard", "B", 7, 3, 3, 8, {"Block", "Guard"}, 2),
        ("Blitzer+MB", "B", 7, 3, 3, 8, {"Block", "MightyBlow"}, 1),
        ("Blitzer+StripBall", "B", 7, 3, 3, 8, {"Block", "StripBall", "Tackle"}, 1),
        ("Thrower", "T", 6, 3, 3, 8, {"SureHands", "Pass", "Block"}, 1),
        ("Catcher", "C", 8, 2, 3, 7, {"Catch", "Dodge", "Block"}, 2),
        ("Ogre", "O", 5, 5, 2, 9, {"Loner", "BoneHead", "MightyBlow", "ThickSkull", "ThrowTeamMate", "Block"}, 1),
    ],
    "orc": [
        ("Lineman", "L", 5, 3, 3, 9, set(), 11),
        ("Blitzer+Guard", "B", 6, 3, 3, 9, {"Block", "Guard"}, 2),
        ("Blitzer+MB", "B", 6, 3, 3, 9, {"Block", "MightyBlow"}, 1),
        ("Blitzer+StripBall", "B", 6, 3, 3, 9, {"Block", "StripBall", "Tackle"}, 1),
        ("BlackOrc+Guard", "K", 4, 4, 2, 9, {"Guard"}, 4),
        ("Thrower", "T", 5, 3, 3, 8, {"SureHands", "Pass", "Block"}, 1),
    ],
    "skaven": [
        ("Lineman", "L", 7, 3, 3, 7, set(), 11),
        ("GutterRunner", "R", 9, 2, 4, 7, {"Dodge", "SureFeet"}, 4),
        ("Blitzer+Guard", "B", 7, 3, 3, 8, {"Block", "Guard"}, 1),
        ("Blitzer+StripBall", "B", 7, 3, 3, 8, {"Block", "StripBall", "Tackle"}, 1),
        ("Thrower", "T", 7, 3, 3, 7, {"SureHands", "Pass", "Block"}, 1),
        ("Lineman+Wrestle", "W", 7, 3, 3, 7, {"Wrestle"}, 2),
    ],
    "dwarf": [
        ("Longbeard", "L", 4, 3, 2, 9, {"Block", "Tackle", "ThickSkull"}, 11),
        ("Longbeard+Guard", "G", 4, 3, 2, 9, {"Block", "Tackle", "ThickSkull", "Guard"}, 4),
        ("Blitzer+Guard", "B", 5, 3, 3, 9, {"Block", "ThickSkull", "Guard"}, 1),
        ("Blitzer+StripBall", "B", 5, 3, 3, 9, {"Block", "ThickSkull", "StripBall"}, 1),
        ("TrollSlayer", "S", 5, 3, 2, 8, {"Block", "Frenzy", "ThickSkull", "Dauntless", "Guard"}, 2),
        ("Runner", "T", 6, 3, 3, 8, {"SureHands", "ThickSkull", "Block"}, 2),
    ],
    "wood-elf": [
        ("Lineman", "L", 7, 3, 4, 7, set(), 11),
        ("Wardancer+StripBall", "B", 8, 3, 4, 7, {"Block", "Dodge", "Leap", "StripBall"}, 1),
        ("Wardancer+SideStep", "B", 8, 3, 4, 7, {"Block", "Dodge", "Leap", "SideStep"}, 1),
        ("Catcher", "C", 8, 2, 4, 7, {"Catch", "Dodge", "Sprint", "Block"}, 2),
        ("Thrower", "T", 7, 3, 4, 7, {"Pass", "Block"}, 1),
        ("Treeman", "O", 2, 6, 1, 10, {"Loner", "TakeRoot", "StandFirm", "MightyBlow", "ThickSkull", "Guard"}, 1),
    ],
}


def slot_templates(race: str) -> list[dict]:
    """Slot 0..10 -> player template dict, replicating buildTeam()."""
    tpls = ROSTERS[race]
    slots: list[dict | None] = [None] * 11
    spec_slot = 10
    for t in range(1, len(tpls)):
        name, letter, ma, st, ag, av, skills, qty = tpls[t]
        for _ in range(qty):
            if spec_slot < 0:
                break
            slots[spec_slot] = dict(name=name, letter=letter, ma=ma, st=st,
                                    ag=ag, av=av, skills=skills)
            spec_slot -= 1
    name, letter, ma, st, ag, av, skills, qty = tpls[0]
    for i in range(spec_slot + 1):
        slots[i] = dict(name=name, letter=letter, ma=ma, st=st, ag=ag, av=av,
                        skills=skills)
    return slots  # type: ignore


def player_table(race_home: str, race_away: str) -> dict[int, dict]:
    """player id (1..22) -> template dict."""
    out = {}
    for slot, tpl in enumerate(slot_templates(race_home)):
        out[1 + slot] = tpl
    for slot, tpl in enumerate(slot_templates(race_away)):
        out[12 + slot] = tpl
    return out


def verify_mapping() -> None:
    """Cross-check slot mapping stats against the engine's own setup."""
    import bb_engine
    for ra in RACES:
        rb = RACES[(RACES.index(ra) + 1) % len(RACES)]
        hr = bb_engine.get_developed_roster(ra, TV)
        ar = bb_engine.get_developed_roster(rb, TV)
        gs = bb_engine.GameState()
        bb_engine.setup_half(gs, hr, ar)
        table = player_table(ra, rb)
        for pid in range(1, 23):
            p = gs.get_player(pid)
            t = table[pid]
            got = (p.stats.movement, p.stats.strength, p.stats.agility, p.stats.armour)
            want = (t["ma"], t["st"], t["ag"], t["av"])
            assert got == want, f"{ra}/{rb} id {pid}: engine {got} != table {want} ({t['name']})"
    print("slot-mapping verification vs bb_engine.setup_half: OK (5 race pairs, 22 ids)")


# ---------------------------------------------------------------------------
# Game generation
# ---------------------------------------------------------------------------

def _game_worker(args: tuple) -> dict:
    seed, race_idx, out_path = args
    import bb_engine
    ra = RACES[race_idx % len(RACES)]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
        dirichlet_alpha=0.0, exploration_c=1.0,
    )
    turns = lgr.get_turn_logs()
    rec = {
        "seed": seed, "home_race": ra, "away_race": rb,
        "home_score": lgr.result.home_score, "away_score": lgr.result.away_score,
        "turns": turns,
    }
    with gzip.open(out_path, "wt") as f:
        json.dump(rec, f)
    return {"seed": seed, "hs": lgr.result.home_score, "as": lgr.result.away_score,
            "races": f"{ra}/{rb}", "n_turns": len(turns)}


def cmd_run(label: str, n: int) -> None:
    verify_mapping()
    out_dir = DATA_ROOT / label
    out_dir.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i % len(RACES), str(out_dir / f"g{i:04d}.json.gz"))
             for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(10) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            print(f"[{done}/{n}] seed={r['seed']} {r['races']} "
                  f"{r['hs']}-{r['as']} turns={r['n_turns']} "
                  f"({time.time()-t0:.0f}s)", flush=True)
    print(f"DONE {n} games in {time.time()-t0:.0f}s -> {out_dir}")


# ---------------------------------------------------------------------------
# Shared geometry helpers
# ---------------------------------------------------------------------------
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def cheb(a, b) -> int:
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def adj8(pos):
    x, y = pos
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            if dx == 0 and dy == 0:
                continue
            nx, ny = x + dx, y + dy
            if 0 <= nx <= 25 and 0 <= ny <= 14:
                yield (nx, ny)


class Board:
    """Positions/states of all players at (or during) one snapshot turn."""

    def __init__(self, turn: dict):
        self.pos: dict[int, tuple] = {}
        self.state: dict[int, int] = {}
        self.side: dict[int, str] = {}
        for key, side in (("home_players", "home"), ("away_players", "away")):
            for p in turn[key]:
                self.pos[p["id"]] = (p["x"], p["y"])
                self.state[p["id"]] = p["state"]
                self.side[p["id"]] = side
        self.carrier = turn["ball_carrier_id"] if turn["ball_held"] else -1

    def standing(self, side: str):
        return [pid for pid, s in self.side.items()
                if s == side and self.state[pid] == 0]

    def occupied(self):
        return set(self.pos.values())

    def tz_squares(self, side: str) -> set:
        """Squares covered by tackle zones of SIDE's standing players."""
        out = set()
        for pid in self.standing(side):
            out.update(adj8(self.pos[pid]))
        return out

    def apply_event(self, ev: dict) -> None:
        """Track mid-turn movement/knockdowns (approximate)."""
        et = ev["type"]
        if et == "MOVE" and ev["player_id"] in self.pos:
            self.pos[ev["player_id"]] = (ev["to_x"], ev["to_y"])
            if self.state.get(ev["player_id"]) == 1:
                self.state[ev["player_id"]] = 0  # moved => stood up
        elif et == "PUSH" and ev["player_id"] in self.pos:
            if 0 <= ev["to_x"] <= 25 and 0 <= ev["to_y"] <= 14:
                self.pos[ev["player_id"]] = (ev["to_x"], ev["to_y"])
            else:
                self.state[ev["player_id"]] = 3  # surfed off pitch
        elif et == "KNOCKED_DOWN" and ev["player_id"] in self.state:
            self.state[ev["player_id"]] = 1


def load_games(label: str):
    out_dir = DATA_ROOT / label
    for f in sorted(out_dir.glob("g*.json.gz")):
        with gzip.open(f, "rt") as fh:
            rec = json.load(fh)
        rec["_file"] = f.name
        yield rec


# ---------------------------------------------------------------------------
# Candidate 1: cage corner quality
# ---------------------------------------------------------------------------

def analyze_cages(games: list) -> dict:
    """At the start of each DEFENDER turn (cage as the attacker left it):
    is the cage there, who sits in the corners, was a stronger/Guard player
    idle nearby, and did the cage break this defender turn?"""
    st = Counter()
    corner_types = Counter()
    noncorner_types = Counter()
    weak_break, strong_break = Counter(), Counter()
    tier_break: dict[int, Counter] = defaultdict(Counter)
    target_weakest = Counter()
    examples = []
    for g in games:
        table = player_table(g["home_race"], g["away_race"])
        turns = g["turns"]
        for k, turn in enumerate(turns):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att_side = b.side.get(b.carrier)
            if att_side is None or turn["active_team"] == att_side:
                continue  # want defender-to-move snapshots
            cpos = b.pos[b.carrier]
            occ = {v: k2 for k2, v in b.pos.items()}
            corners = []
            for dx, dy in DIAG:
                sq = (cpos[0] + dx, cpos[1] + dy)
                pid = occ.get(sq)
                if pid is not None and b.side[pid] == att_side and b.state[pid] == 0:
                    corners.append(pid)
            st["def_turns_vs_carrier"] += 1
            if len(corners) < 3:
                continue
            st["cages"] += 1
            for pid in corners:
                corner_types[table[pid]["name"]] += 1
            # tier of the WORST corner: 2 = severe (ST<=2), 1 = soft (ST3,
            # no Guard, no Block), 0 = solid
            def corner_tier(pid):
                t = table[pid]
                if t["st"] <= 2:
                    return 2
                if "Guard" not in t["skills"] and "Block" not in t["skills"]:
                    return 1
                return 0
            worst_tier = max(corner_tier(pid) for pid in corners)
            st[f"cages_worst_tier_{worst_tier}"] += 1
            # does the defender target the weakest corner when hitting a corner?
            corner_strength = {pid: (table[pid]["st"]
                                     + (0.5 if "Guard" in table[pid]["skills"] else 0))
                               for pid in corners}
            min_strength = min(corner_strength.values())
            hit_corners = {ev["target_id"] for ev in turn["events"]
                           if ev["type"] == "BLOCK" and ev["target_id"] in corner_strength}
            for pid in hit_corners:
                target_weakest[corner_strength[pid] == min_strength] += 1
            # idle better teammate nearby? (standing, not carrier/corner,
            # within 3 of carrier, strictly better: higher ST or has Guard
            # when some corner lacks Guard and has <= its ST)
            weak_corners = [pid for pid in corners
                            if table[pid]["st"] <= 2
                            or ("Guard" not in table[pid]["skills"] and table[pid]["st"] <= 3)]
            better_avail = False
            for pid, s in b.side.items():
                if s != att_side or pid == b.carrier or pid in corners:
                    continue
                if b.state[pid] != 0 or cheb(b.pos[pid], cpos) > 3:
                    continue
                noncorner_types[table[pid]["name"]] += 1
                for wc in weak_corners:
                    t_wc, t_p = table[wc], table[pid]
                    if (t_p["st"] > t_wc["st"]
                            or ("Guard" in t_p["skills"] and "Guard" not in t_wc["skills"]
                                and t_p["st"] >= t_wc["st"])):
                        better_avail = True
            if weak_corners:
                st["cages_with_weak_corner"] += 1
            if weak_corners and better_avail:
                st["cages_weak_corner_with_better_idle_nearby"] += 1
            # outcome: cage broken during this defender turn = carrier prone
            # or ball loose at next snapshot
            broken = False
            if k + 1 < len(turns):
                nxt = turns[k + 1]
                nb = Board(nxt)
                if not nxt["ball_held"] or nb.state.get(b.carrier, 0) != 0 \
                        or nxt["ball_carrier_id"] != b.carrier:
                    broken = True
            (weak_break if weak_corners else strong_break)[broken] += 1
            tier_break[worst_tier][broken] += 1
            if worst_tier == 2 and better_avail and len(examples) < 20:
                examples.append((g["_file"], k, sorted(weak_corners), broken))
    return {"stats": st, "corner_types": corner_types,
            "weak_break": weak_break, "strong_break": strong_break,
            "tier_break": dict(tier_break), "target_weakest": target_weakest,
            "examples": examples}


# ---------------------------------------------------------------------------
# Follow-up (2026-07-15): Dodge (corner) x Tackle (attacker) hypothesis for
# the counterintuitive tier2-breaks-less-than-tier0 result above. For every
# BLOCK event this data-collection run recorded where the target is a corner
# occupant of a formed cage (>=3 diagonals), cross-tab: corner's own ST/Guard
# tier x corner-has-Dodge x attacker-has-Tackle -> immediate knockdown rate
# and same-turn cage-broken rate (same "broken" definition as analyze_cages,
# i.e. carrier exposed by the NEXT snapshot -- turn-level, so with >1 corner
# block in a turn this is a shared outcome across those blocks).
# ---------------------------------------------------------------------------

def analyze_corner_dodge_tackle(games: list) -> dict:
    def corner_tier(t):
        if t["st"] <= 2:
            return 2
        if "Guard" not in t["skills"] and "Block" not in t["skills"]:
            return 1
        return 0

    cross: dict[tuple, list] = defaultdict(lambda: [0, 0, 0])  # (tier, has_dodge, has_tackle) -> [n, knocked_down, cage_broken]
    examples = defaultdict(list)
    for g in games:
        table = player_table(g["home_race"], g["away_race"])
        turns = g["turns"]
        for k, turn in enumerate(turns):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att_side = b.side.get(b.carrier)
            def_side = turn["active_team"]
            if att_side is None or def_side == att_side:
                continue
            cpos = b.pos[b.carrier]
            occ = {v: k2 for k2, v in b.pos.items()}
            corners = []
            for dx, dy in DIAG:
                sq = (cpos[0] + dx, cpos[1] + dy)
                pid = occ.get(sq)
                if pid is not None and b.side[pid] == att_side and b.state[pid] == 0:
                    corners.append(pid)
            if len(corners) < 3:
                continue
            corner_set = set(corners)
            # same-turn cage-broken outcome, as in analyze_cages
            broken = False
            if k + 1 < len(turns):
                nxt = turns[k + 1]
                nb = Board(nxt)
                if not nxt["ball_held"] or nb.state.get(b.carrier, 0) != 0 \
                        or nxt["ball_carrier_id"] != b.carrier:
                    broken = True
            for i, ev in enumerate(turn["events"]):
                if ev["type"] != "BLOCK" or ev["target_id"] not in corner_set:
                    continue
                a, d = ev["player_id"], ev["target_id"]
                if a not in table or d not in table:
                    continue
                td = table[d]
                tier = corner_tier(td)
                has_dodge = "Dodge" in td["skills"]
                has_tackle = "Tackle" in table[a]["skills"]
                knocked = any(e2["type"] == "KNOCKED_DOWN" and e2["player_id"] == d
                             for e2 in turn["events"][i:i + 4])
                key = (tier, has_dodge, has_tackle)
                cross[key][0] += 1
                cross[key][1] += 1 if knocked else 0
                cross[key][2] += 1 if broken else 0
                if len(examples[key]) < 4:
                    examples[key].append((g["_file"], k, a, d))
    return {"cross": cross, "examples": dict(examples)}


# ---------------------------------------------------------------------------
# Candidate 2: carrier_blitzable Chebyshev vs BFS
# ---------------------------------------------------------------------------

def bfs_can_blitz(b: Board, pid: int, cpos, ma: int) -> bool:
    """BFS from defender pid: reach a square ADJACENT to carrier within ma
    steps, path squares outside attacker TZ (brief Okruh 5 Q2 spec: blocked =
    opponent TZ; blitz exception: final square may be in TZ) and unoccupied."""
    att_side = "home" if b.side[pid] == "away" else "away"
    tz = b.tz_squares(att_side)
    occ = b.occupied() - {b.pos[pid]}
    targets = set(adj8(cpos))
    start = b.pos[pid]
    if start in targets:
        return True
    seen = {start}
    frontier = [start]
    for _ in range(ma):
        nxt = []
        for sq in frontier:
            for n in adj8(sq):
                if n in seen or n in occ:
                    continue
                if n in targets:
                    return True  # final step may be in TZ
                if n in tz:
                    continue
                seen.add(n)
                nxt.append(n)
        frontier = nxt
        if not frontier:
            break
    return False


def analyze_blitzable(games: list) -> dict:
    """Confusion of f63-style Chebyshev flag vs safe-path BFS flag, scored
    against what actually happened (carrier attacked this defender turn)."""
    cells = Counter()  # (cheb, bfs) -> [n, attacked]
    attacked_by = Counter()
    examples = {"false_danger_attacked": [], "false_danger_safe": []}
    for g in games:
        table = player_table(g["home_race"], g["away_race"])
        for k, turn in enumerate(g["turns"]):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att_side = b.side.get(b.carrier)
            def_side = turn["active_team"]
            if att_side is None or def_side == att_side:
                continue
            cpos = b.pos[b.carrier]
            chebf = any(cheb(b.pos[pid], cpos) <= table[pid]["ma"]
                        for pid in b.standing(def_side))
            bfsf = any(bfs_can_blitz(b, pid, cpos, table[pid]["ma"])
                       for pid in b.standing(def_side))
            # f63 blind spot: prone defender close enough to stand (3 MA) + reach
            pronef = any(b.state[pid] == 1
                         and cheb(b.pos[pid], cpos) <= max(table[pid]["ma"] - 3, 0) + 1
                         for pid, s in b.side.items() if s == def_side)
            attacked = any(
                (ev["type"] == "BLOCK" and ev["target_id"] == b.carrier)
                or (ev["type"] == "KNOCKED_DOWN" and ev["player_id"] == b.carrier)
                for ev in turn["events"])
            cells[(chebf, bfsf, attacked)] += 1
            if not chebf and pronef:
                cells[("prone_only", attacked)] += 1
            if chebf and not bfsf:
                key = "false_danger_attacked" if attacked else "false_danger_safe"
                if len(examples[key]) < 8:
                    examples[key].append((g["_file"], k))
            if attacked:
                attacked_by[(chebf, bfsf)] += 1
    return {"cells": cells, "examples": examples}


# ---------------------------------------------------------------------------
# Candidate 3: free receiver near endzone
# ---------------------------------------------------------------------------

def analyze_receivers(games: list) -> dict:
    st = Counter()
    pass_when = Counter()
    examples = []
    nilnil_open = Counter()
    for g in games:
        nil_nil = g["home_score"] == 0 and g["away_score"] == 0
        for k, turn in enumerate(g["turns"]):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att = b.side.get(b.carrier)
            if att is None or turn["active_team"] != att:
                continue  # attacker-to-move snapshots
            st["att_turns"] += 1
            cpos = b.pos[b.carrier]
            ez_x = 25 if att == "home" else 0
            def_side = "away" if att == "home" else "home"
            opp_tz = b.tz_squares(def_side)
            open_recv = []
            best_open_dist = None
            for pid in b.standing(att):
                if pid == b.carrier:
                    continue
                p = b.pos[pid]
                if p in opp_tz or abs(p[0] - ez_x) >= abs(cpos[0] - ez_x):
                    continue
                d = abs(p[0] - ez_x)
                if best_open_dist is None or d < best_open_dist:
                    best_open_dist = d
                if d <= 6:
                    open_recv.append(pid)
            if best_open_dist is not None:
                st[f"open_ahead_dist_bucket_{min(best_open_dist // 4, 4)}"] += 1
            else:
                st["no_open_teammate_ahead"] += 1
            has_open = bool(open_recv)
            threw = any(ev["type"] in ("PASS",) for ev in turn["events"])
            if has_open:
                st["att_turns_open_receiver"] += 1
                if nil_nil:
                    nilnil_open["open"] += 1
            elif nil_nil:
                nilnil_open["closed"] += 1
            pass_when[(has_open, threw)] += 1
            if has_open and not threw and abs(cpos[0] - ez_x) >= 12 \
                    and len(examples) < 12:
                examples.append((g["_file"], k, sorted(open_recv)))
        # game-level pass counts
        n_pass = sum(1 for t in g["turns"] for ev in t["events"]
                     if ev["type"] == "PASS")
        st["total_passes"] += n_pass
        st["total_catches_ok"] += sum(1 for t in g["turns"] for ev in t["events"]
                                      if ev["type"] == "CATCH" and ev["success"])
        if nil_nil:
            st["nilnil_games"] += 1
            st["nilnil_passes"] += n_pass
        st["games"] += 1
    return {"stats": st, "pass_when": pass_when, "nilnil_open": nilnil_open,
            "examples": examples}


# ---------------------------------------------------------------------------
# Candidate 3b (2026-07-15 correction): is_free_receiver reframed as RELATIVE
# MOBILITY advantage, not "ahead of the carrier toward the endzone". A
# teammate is a useful option if they have a materially better safe (0-dodge,
# TZ-respecting) path to advance than the carrier currently has -- regardless
# of whether that teammate is geometrically ahead of or behind the carrier.
# Reuses the same BFS/TZ machinery as carrier_blitzable (bfs_can_blitz above)
# extended to every standing teammate, per Opus Q2's "one flood-fill per
# player, shared across features" framing in team1_results_opus.md.
# ---------------------------------------------------------------------------

def bfs_safe_reachable(start: tuple, ma: int, blocked: set, occ: set) -> set:
    """All squares reachable from START within MA steps, 0-dodge (never
    entering a blocked/TZ square) and never landing on an occupied square.
    Mirrors bfs_can_blitz's blocked-by-TZ semantics but returns the full
    reachable set instead of testing a single target (general-purpose
    version of the same flood-fill, per Opus's shared-flood-fill framing)."""
    seen = {start}
    frontier = [start]
    for _ in range(ma):
        nxt = []
        for sq in frontier:
            for n in adj8(sq):
                if n in seen or n in occ or n in blocked:
                    continue
                seen.add(n)
                nxt.append(n)
        frontier = nxt
        if not frontier:
            break
    return seen


MOBILITY_ADVANTAGE_THRESHOLD = 3  # squares of extra safe progress toward the endzone


def analyze_mobility_advantage(games: list) -> dict:
    st = Counter()
    by_race = defaultdict(Counter)          # attacking-side race -> Counter
    capitalized = Counter()                 # (has_advantage) -> [n, capitalized]
    capitalized_by_race = defaultdict(Counter)
    nilnil_rate = Counter()                 # (nil_nil) -> [n, advantage]
    examples = []
    for g in games:
        table = player_table(g["home_race"], g["away_race"])
        nil_nil = g["home_score"] == 0 and g["away_score"] == 0
        for k, turn in enumerate(g["turns"]):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att = b.side.get(b.carrier)
            if att is None or turn["active_team"] != att:
                continue  # attacker-to-move snapshots (same 1399-snapshot set as original)
            defn = "away" if att == "home" else "home"
            ez = 25 if att == "home" else 0
            opp_tz = b.tz_squares(defn)
            occ_all = b.occupied()
            cpos = b.pos[b.carrier]
            c_ma = table[b.carrier]["ma"]
            c_reach = bfs_safe_reachable(cpos, c_ma, opp_tz, occ_all - {cpos})
            c_progress = abs(cpos[0] - ez) - min(abs(s[0] - ez) for s in c_reach)

            best_teammate_progress = -999
            best_teammate = None
            for pid in b.standing(att):
                if pid == b.carrier:
                    continue
                pos = b.pos[pid]
                ma = table[pid]["ma"]
                reach = bfs_safe_reachable(pos, ma, opp_tz, occ_all - {pos})
                progress = abs(pos[0] - ez) - min(abs(s[0] - ez) for s in reach)
                if progress > best_teammate_progress:
                    best_teammate_progress = progress
                    best_teammate = pid

            st["att_turns"] += 1
            att_race = g["home_race"] if att == "home" else g["away_race"]
            by_race[att_race]["att_turns"] += 1

            advantage = (best_teammate is not None
                        and best_teammate_progress >= c_progress + MOBILITY_ADVANTAGE_THRESHOLD)
            if advantage:
                st["mobility_advantage"] += 1
                by_race[att_race]["mobility_advantage"] += 1
                acted = any(ev["type"] in ("PASS",) for ev in turn["events"]) or \
                    any(ev["type"] == "CATCH" and ev["player_id"] == best_teammate
                        for ev in turn["events"])
                capitalized[acted] += 1
                capitalized_by_race[att_race][acted] += 1
                if nil_nil:
                    nilnil_rate["nilnil_and_advantage"] += 1
                if len(examples) < 15:
                    examples.append((g["_file"], k, att_race, b.carrier, c_progress,
                                     best_teammate, best_teammate_progress, acted))
            if nil_nil:
                nilnil_rate["nilnil_turns"] += 1
    return {"stats": st, "by_race": dict(by_race), "capitalized": capitalized,
            "capitalized_by_race": dict(capitalized_by_race),
            "nilnil_rate": nilnil_rate, "examples": examples}


# ---------------------------------------------------------------------------
# Candidate 4: net_st_for_block (assist-aware dice on chosen blocks)
# ---------------------------------------------------------------------------

def block_dice(b: Board, table: dict, att_id: int, def_id: int) -> int:
    """Net dice class for att->def per the brief's assist algorithm:
    +2 => 3 att-choice, +1 => 2, 0 => 1, -1 => -2 (def choice), <=-2 => -3."""
    apos, dpos = b.pos[att_id], b.pos[def_id]
    aside, dside = b.side[att_id], b.side[def_id]
    off = 0
    for pid in b.standing(aside):
        if pid == att_id or cheb(b.pos[pid], dpos) != 1:
            continue
        guard = "Guard" in table[pid]["skills"]
        in_other_tz = any(cheb(b.pos[q], b.pos[pid]) == 1
                          for q in b.standing(dside) if q != def_id)
        if guard or not in_other_tz:
            off += 1
    deff = 0
    for pid in b.standing(dside):
        if pid == def_id or cheb(b.pos[pid], apos) != 1:
            continue
        guard = "Guard" in table[pid]["skills"]
        in_other_tz = any(cheb(b.pos[q], b.pos[pid]) == 1
                          for q in b.standing(aside) if q != att_id)
        if guard or not in_other_tz:
            deff += 1
    net = (table[att_id]["st"] + off) - (table[def_id]["st"] + deff)
    if net >= 2:
        return 3
    if net == 1:
        return 2
    if net == 0:
        return 1
    if net == -1:
        return -2
    return -3


def analyze_blocks(games: list) -> dict:
    dice_dist = Counter()
    bad_with_better = 0
    bad_total = 0
    examples = []
    attacker_down_after_bad = Counter()
    # Wrestle correction (2026-07-15, coordinator task 2): a Wrestle attacker
    # blitzing the ball carrier deliberately accepts net-negative dice --
    # Wrestle forces Both Down on ANY block result with no reroll needed, no
    # turnover risk, so knocking the carrier down (and possibly stripping the
    # ball, if StripBall is also present) is the actual tactical goal, not a
    # mistake. Track these as a separate bucket rather than folding them into
    # "bad_total" uncritically.
    bad_wrestle_any = 0            # net<0 blocks where attacker has Wrestle
    bad_wrestle_on_carrier = 0     # ...and target is the ball carrier
    bad_wrestle_stripball_on_carrier = 0  # ...and attacker also has StripBall
    bad_non_wrestle = 0            # net<0 blocks with a non-Wrestle attacker
    bad_non_wrestle_with_better = 0
    for g in games:
        table = player_table(g["home_race"], g["away_race"])
        for k, turn in enumerate(g["turns"]):
            b = Board(turn)
            act = turn["active_team"]
            for i, ev in enumerate(turn["events"]):
                if ev["type"] == "BLOCK":
                    a, d = ev["player_id"], ev["target_id"]
                    if a not in b.pos or d not in b.pos or b.side.get(a) != act:
                        b.apply_event(ev)
                        continue
                    dc = block_dice(b, table, a, d)
                    dice_dist[dc] += 1
                    if dc < 0:
                        bad_total += 1
                        has_wrestle = "Wrestle" in table[a]["skills"]
                        on_carrier = (d == b.carrier)
                        has_stripball = "StripBall" in table[a]["skills"]
                        if has_wrestle:
                            bad_wrestle_any += 1
                            if on_carrier:
                                bad_wrestle_on_carrier += 1
                                if has_stripball:
                                    bad_wrestle_stripball_on_carrier += 1
                        else:
                            bad_non_wrestle += 1
                        # any better block available to the acting side now?
                        best_alt = -3
                        for pid in b.standing(act):
                            for opp in b.standing("away" if act == "home" else "home"):
                                if cheb(b.pos[pid], b.pos[opp]) == 1:
                                    best_alt = max(best_alt,
                                                   block_dice(b, table, pid, opp))
                        if best_alt >= 1:
                            bad_with_better += 1
                            if not has_wrestle:
                                bad_non_wrestle_with_better += 1
                            if len(examples) < 10:
                                examples.append((g["_file"], k, a, d, dc, best_alt))
                        # attacker knocked down among following events?
                        down = any(e2["type"] == "KNOCKED_DOWN" and e2["player_id"] == a
                                   for e2 in turn["events"][i:i + 6])
                        attacker_down_after_bad[down] += 1
                b.apply_event(ev)
    return {"dice_dist": dice_dist, "bad_total": bad_total,
            "bad_with_better": bad_with_better,
            "attacker_down_after_bad": attacker_down_after_bad,
            "examples": examples,
            "bad_wrestle_any": bad_wrestle_any,
            "bad_wrestle_on_carrier": bad_wrestle_on_carrier,
            "bad_wrestle_stripball_on_carrier": bad_wrestle_stripball_on_carrier,
            "bad_non_wrestle": bad_non_wrestle,
            "bad_non_wrestle_with_better": bad_non_wrestle_with_better}


# ---------------------------------------------------------------------------
# Candidate 5: adjacent_to_sideline / crowd surf
# ---------------------------------------------------------------------------

def analyze_sideline(games: list) -> dict:
    st = Counter()
    examples = []
    for g in games:
        for k, turn in enumerate(g["turns"]):
            b = Board(turn)
            act = turn["active_team"]
            opp = "away" if act == "home" else "home"
            # exposure at the start of ACT's turn: opp players on sideline
            # adjacent to one of act's standing players (surf candidates)
            surfable = [pid for pid in b.standing(opp)
                        if b.pos[pid][1] in (0, 14)
                        and any(b.side.get(q) == act and b.state[q] == 0
                                and cheb(b.pos[q], b.pos[pid]) == 1
                                for q in b.pos)]
            if surfable:
                st["turns_with_surfable_opp"] += 1
            surfed = [ev for ev in turn["events"]
                      if ev["type"] == "PUSH"
                      and not (0 <= ev["to_x"] <= 25 and 0 <= ev["to_y"] <= 14)]
            if surfed:
                st["turns_with_surf"] += 1
                st["surfs"] += len(surfed)
                if surfable and len(examples) < 8:
                    examples.append((g["_file"], k, [e["player_id"] for e in surfed]))
            if surfable and not surfed:
                st["surfable_not_surfed"] += 1
            # own players left standing on sideline adjacent to opp at start
            # of OPP's turn is the same statistic from the other perspective.
        st["games"] += 1
    return {"stats": st, "examples": examples}


# ---------------------------------------------------------------------------
# Role-labelled ASCII pitch
# ---------------------------------------------------------------------------

def render_roles(g: dict, snap: int, highlight: set | None = None) -> str:
    """render_pitch() variant with role letters (home upper, away lower).
    Carrier shown as '@'; prone/stunned as '+'/'_' with roles in the caption."""
    turn = g["turns"][snap]
    table = player_table(g["home_race"], g["away_race"])
    grid = [["." for _ in range(26)] for _ in range(15)]
    caption = []
    for key, is_home in (("home_players", True), ("away_players", False)):
        for p in turn[key]:
            x, y = int(p["x"]), int(p["y"])
            if not (0 <= x <= 25 and 0 <= y <= 14):
                continue
            t = table[p["id"]]
            ch = t["letter"] if is_home else t["letter"].lower()
            if p["state"] == 1:
                ch = "+"
            elif p["state"] == 2:
                ch = "_"
            if p["has_ball"]:
                ch = "@"
            grid[y][x] = ch
            if highlight and p["id"] in highlight:
                caption.append(
                    f"  id{p['id']} {'H' if is_home else 'A'} {t['name']} "
                    f"ST{t['st']} MA{t['ma']}{' Guard' if 'Guard' in t['skills'] else ''} "
                    f"at ({x},{y}){' PRONE' if p['state']==1 else ''}")
    if not turn["ball_held"] and 0 <= turn["ball_x"] <= 25:
        if grid[turn["ball_y"]][turn["ball_x"]] == ".":
            grid[turn["ball_y"]][turn["ball_x"]] = "*"  # loose ball (distinct from 'O'=Ogre/Treeman)
    lines = [f"Half {turn['half']} turn {turn['turn']} ({turn['active_team']} to move) "
             f"score {turn['home_score']}-{turn['away_score']} "
             f"[{g['home_race']} H vs {g['away_race']} A]"]
    lines.append("   " + "".join(str(x % 10) for x in range(26)))
    lines.append("   " + "-" * 26)
    for y in range(15):
        lines.append(f"{y:2d}|{''.join(grid[y])}|")
    lines.append("   " + "-" * 26)
    lines.append("   HOME=UPPER away=lower  @=ball carrier  *=loose ball  "
                  "+=prone  _=stunned  L/l=Lineman B/b=Blitzer G/g=Guard-corner "
                  "K/k=BlackOrc C/c=Catcher T/t=Thrower R/r=GutterRunner "
                  "W/w=Wrestle-lineman S/s=TrollSlayer O/o=Ogre/Treeman")
    lines.extend(caption)
    # events of this turn (viewer-style, MOVEs collapsed)
    moves = defaultdict(list)
    for ev in turn["events"]:
        if ev["type"] == "MOVE":
            moves[ev["player_id"]].append((ev["to_x"], ev["to_y"]))
    for pid, path in moves.items():
        lines.append(f"  -> id{pid} moves to {path[-1]} ({len(path)} steps)")
    for ev in turn["events"]:
        et = ev["type"]
        if et == "MOVE":
            continue
        pid, tid = ev["player_id"], ev["target_id"]
        ok = "OK" if ev["success"] else "FAIL"
        if et in ("DODGE", "GFI", "PICKUP", "CATCH"):
            lines.append(f"  {et} id{pid} roll {ev['roll']} {ok}")
        elif et == "BLOCK":
            lines.append(f"  BLOCK id{pid} -> id{tid} {ok}")
        elif et == "PUSH":
            lines.append(f"  PUSH id{pid} ({ev['from_x']},{ev['from_y']})->({ev['to_x']},{ev['to_y']})")
        elif et == "PASS":
            lines.append(f"  PASS id{pid} -> id{tid} roll {ev['roll']} {ok}")
        else:
            lines.append(f"  {et} id{pid}" + (f" -> id{tid}" if tid not in (-1, None) else ""))
    return "\n".join(lines)


def load_game(label: str, fname: str) -> dict:
    with gzip.open(DATA_ROOT / label / fname, "rt") as fh:
        rec = json.load(fh)
    rec["_file"] = fname
    return rec


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

def cmd_analyze(label: str) -> None:
    games = list(load_games(label))
    print(f"loaded {len(games)} games from {DATA_ROOT/label}")
    res = Counter()
    for g in games:
        if g["home_score"] == g["away_score"]:
            res["draw"] += 1
            if g["home_score"] == 0:
                res["nil_nil"] += 1
        else:
            res["decisive"] += 1
    print(f"results: {dict(res)}\n")

    print("=" * 70)
    print("1) CAGE CORNER QUALITY")
    c = analyze_cages(games)
    print(dict(c["stats"]))
    print("corner occupants by template:", dict(c["corner_types"].most_common()))
    for tier in sorted(c["tier_break"]):
        cc = c["tier_break"][tier]
        n = cc[True] + cc[False]
        if n:
            print(f"worst-corner tier {tier} (0=solid,1=soft,2=ST<=2): "
                  f"broken {cc[True]}/{n} = {cc[True]/n:.1%}")
    tw = c["target_weakest"]
    ntw = tw[True] + tw[False]
    if ntw:
        print(f"defender corner-blocks aimed at weakest corner: {tw[True]}/{ntw} = {tw[True]/ntw:.1%}")
    print("examples (severe corner + better idle nearby; last flag=broken):",
          c["examples"][:8])

    print("=" * 70)
    print("2) CARRIER_BLITZABLE cheb vs BFS (defender-to-move snapshots)")
    bl = analyze_blitzable(games)
    agg = Counter()
    prone_only = Counter()
    for key, n in bl["cells"].items():
        if key[0] == "prone_only":
            prone_only[key[1]] += n
            continue
        chf, bff, att = key
        agg[(chf, bff)] += n
        agg[(chf, bff, "att")] += n if att else 0
    tot = sum(v for k, v in bl["cells"].items() if k[0] != "prone_only")
    for key in [(True, True), (True, False), (False, False), (False, True)]:
        n = agg[key]
        a = agg[(key[0], key[1], "att")]
        if n:
            print(f"cheb={key[0]} bfs={key[1]}: {n} ({n/tot:.1%}), "
                  f"carrier actually attacked {a}/{n} = {a/n:.1%}")
    if prone_only:
        po = prone_only[True] + prone_only[False]
        print(f"cheb=0 BUT prone defender in stand+reach range: {po}, "
              f"attacked {prone_only[True]}/{po}")
    print("examples false-danger (cheb=1,bfs=0) attacked:",
          bl["examples"]["false_danger_attacked"][:4])
    print("examples false-danger safe:", bl["examples"]["false_danger_safe"][:4])

    print("=" * 70)
    print("3) FREE RECEIVER")
    r = analyze_receivers(games)
    print(dict(r["stats"]))
    print("(open_receiver, threw_pass):", dict(r["pass_when"]))
    print("nil-nil attacker turns with open receiver:", dict(r["nilnil_open"]))
    print("examples (open recv, carrier deep, no pass):", r["examples"][:6])

    print("=" * 70)
    print("4) NET_ST_FOR_BLOCK (chosen blocks, assist-aware)")
    bk = analyze_blocks(games)
    print("dice class distribution (3=3d att, 2=2d, 1=1d, -2=2d def, -3=3d def):",
          dict(sorted(bk["dice_dist"].items())))
    print(f"negative-dice blocks: {bk['bad_total']}, of which better (>=1d) "
          f"alternative existed same moment: {bk['bad_with_better']}")
    print("attacker down within 6 events after bad block:",
          dict(bk["attacker_down_after_bad"]))
    print("examples (file, snap, att, def, dice, best_alt):", bk["examples"][:6])

    print("=" * 70)
    print("5) SIDELINE / CROWD SURF")
    s = analyze_sideline(games)
    print(dict(s["stats"]))
    print("examples:", s["examples"][:6])


def cmd_scan_types(label: str) -> None:
    """Find pointers for diverse situation types (gallery selection)."""
    hits: dict[str, list] = defaultdict(list)
    for g in load_games(label):
        table = player_table(g["home_race"], g["away_race"])
        turns = g["turns"]
        ground_streak = 0
        for k, turn in enumerate(turns):
            b = Board(turn)
            act = turn["active_team"]
            evs = turn["events"]
            # (a) stall: carrier could score (dist <= MA, no GFI needed) but no TD
            if b.carrier >= 0 and b.side.get(b.carrier) == act:
                ez = 25 if act == "home" else 0
                dist = abs(b.pos[b.carrier][0] - ez)
                if dist <= table[b.carrier]["ma"] and not turn["touchdown"]:
                    hits["stall_could_score"].append((g["_file"], k, dist))
            # (b) FOUL while loose ball within 2 of a teammate of fouling side
            for ev in evs:
                if ev["type"] == "FOUL" and not turn["ball_held"]:
                    bp = (turn["ball_x"], turn["ball_y"])
                    near = any(b.side.get(pid) == act and b.state[pid] == 0
                               and cheb(b.pos[pid], bp) <= 2 for pid in b.pos)
                    if near:
                        hits["foul_near_loose_ball"].append((g["_file"], k))
            # (c) turnover cascade: turnover on failed dodge/GFI while carrying
            if turn.get("turnover"):
                for ev in evs:
                    if ev["type"] in ("DODGE", "GFI") and not ev["success"] \
                            and ev["player_id"] == b.carrier:
                        hits["carrier_risk_turnover"].append((g["_file"], k, ev["type"]))
                        break
            # (d) TD scored this turn
            if turn.get("touchdown"):
                hits["touchdown"].append((g["_file"], k))
            # (e) last-turn attacking chances (half 2, turn >= 8)
            if turn["half"] == 2 and turn["turn"] >= 8 and b.carrier >= 0 \
                    and b.side.get(b.carrier) == act:
                hits["endgame_attacker"].append((g["_file"], k))
            # (f) pickup neglect streak
            if not turn["ball_held"]:
                ground_streak += 1
                if ground_streak == 6:
                    hits["ground_streak6"].append((g["_file"], k))
            else:
                ground_streak = 0
            # (g) failed pickups repeated same square
            npk = sum(1 for ev in evs if ev["type"] == "PICKUP" and not ev["success"])
            if npk >= 2:
                hits["multi_failed_pickup_turn"].append((g["_file"], k))
    for k2 in sorted(hits):
        v = hits[k2]
        print(f"{k2}: {len(v)} hits; first 8: {v[:8]}")


# ---------------------------------------------------------------------------
# Broader survey (2026-07-15 follow-up): situation types beyond the 5
# original candidates, for a human-review gallery. Reuses the same saved
# games -- no new simulation. Covers kickoff/setup, cage advance, good
# blocks, pass/handoff attempts, turnover recovery, screens, KO mid-drive,
# one-turn TDs, stalling-while-leading, in addition to the scan_types above.
# ---------------------------------------------------------------------------

def cmd_mobility(label: str) -> None:
    games = list(load_games(label))
    res = analyze_mobility_advantage(games)
    st = res["stats"]
    tot = st["att_turns"]
    adv = st["mobility_advantage"]
    print(f"{len(games)} games, {tot} attacker-to-move snapshots (threshold="
          f"{MOBILITY_ADVANTAGE_THRESHOLD} squares safe progress)")
    print(f"OVERALL mobility-advantage prevalence: {adv}/{tot} = {adv/tot:.2%}  "
          f"(original ahead-of-carrier definition: 0.50%)")
    cap = res["capitalized"]
    ncap = cap[True] + cap[False]
    if ncap:
        print(f"OVERALL capitalized (PASS or CATCH-by-advantaged-teammate same turn): "
              f"{cap[True]}/{ncap} = {cap[True]/ncap:.1%}")
    print()
    print("By attacking-side race:")
    for race, c in sorted(res["by_race"].items()):
        n, a = c["att_turns"], c["mobility_advantage"]
        capc = res["capitalized_by_race"].get(race, Counter())
        ncapc = capc[True] + capc[False]
        cap_str = f", capitalized {capc[True]}/{ncapc}={capc[True]/ncapc:.1%}" if ncapc else ", capitalized 0/0"
        print(f"  {race:10s} n={n:4d}  advantage={a:4d} ({a/n:.2%}){cap_str}")
    nr = res["nilnil_rate"]
    if nr["nilnil_turns"]:
        print(f"\nnil-nil-game attacker turns: {nr['nilnil_turns']}, of which "
              f"mobility-advantage: {nr['nilnil_and_advantage']} = "
              f"{nr['nilnil_and_advantage']/nr['nilnil_turns']:.2%}")
    print("\nexamples (file, snap, race, carrier_id, carrier_progress, "
          "teammate_id, teammate_progress, capitalized):")
    for ex in res["examples"]:
        print(f"  {ex}")


def cmd_dodge_tackle(label: str) -> None:
    games = list(load_games(label))
    res = analyze_corner_dodge_tackle(games)
    cross = res["cross"]
    print(f"{len(games)} games. Cross-tab (tier, corner_has_dodge, attacker_has_tackle) "
          "-> n, knockdown%, cage_broken%:")
    for key in sorted(cross, key=lambda k: (k[0], not k[1], not k[2])):
        n, kd, br = cross[key]
        tier, dodge, tackle = key
        tname = {2: "severe(ST<=2)", 1: "soft(ST3,noG/B)", 0: "solid"}[tier]
        print(f"  tier={tname:15s} dodge={str(dodge):5s} tackle={str(tackle):5s} "
              f"n={n:4d}  knockdown={kd}/{n}={kd/n:.1%}  cage_broken={br}/{n}={br/n:.1%}")
    print("\nexamples per cell (file, snap, attacker_id, defender_id):")
    for key, ex in res["examples"].items():
        print(f"  {key}: {ex}")


def cmd_survey(label: str) -> None:
    hits: dict[str, list] = defaultdict(list)
    for g in load_games(label):
        table = player_table(g["home_race"], g["away_race"])
        turns = g["turns"]

        # (a) good block: 2+ dice attacker-choice block actually chosen
        for k, turn in enumerate(turns):
            b = Board(turn)
            act = turn["active_team"]
            for ev in turn["events"]:
                if ev["type"] == "BLOCK":
                    a, d = ev["player_id"], ev["target_id"]
                    if a in b.pos and d in b.pos and b.side.get(a) == act:
                        dc = block_dice(b, table, a, d)
                        if dc >= 2:
                            hits["good_block"].append((g["_file"], k, a, d, dc))
                b.apply_event(ev)

        # (b) pass attempts (real PASS events, success or fail)
        for k, turn in enumerate(turns):
            for ev in turn["events"]:
                if ev["type"] == "PASS":
                    hits["pass_attempt"].append(
                        (g["_file"], k, ev["player_id"], ev["target_id"], ev["success"]))

        # (c) hand-off: CATCH event with roll-mod +1 signature and no PASS
        # event same turn (resolveHandOff emits only CATCH, pass_handler.cpp:355-390)
        for k, turn in enumerate(turns):
            has_pass = any(ev["type"] == "PASS" for ev in turn["events"])
            if has_pass:
                continue
            for ev in turn["events"]:
                if ev["type"] == "CATCH":
                    hits["handoff_candidate"].append(
                        (g["_file"], k, ev["player_id"], ev["success"]))

        # (d) turnover -> who holds the ball a few turn-boundaries later?
        for k, turn in enumerate(turns):
            if turn.get("turnover") and k + 1 < len(turns):
                losing_side = turn["active_team"]  # side whose turn just ended in turnover
                recovered_by = None
                for j in range(k + 1, min(k + 4, len(turns))):
                    bj = Board(turns[j])
                    if bj.carrier >= 0:
                        recovered_by = bj.side.get(bj.carrier)
                        break
                hits["turnover_then"].append((g["_file"], k, losing_side, recovered_by))

        # (e) KO / casualty mid-drive (not at kickoff)
        for k, turn in enumerate(turns):
            for ev in turn["events"]:
                if ev["type"] in ("CASUALTY", "INJURY") and turn["turn"] >= 2:
                    hits["ko_mid_drive"].append((g["_file"], k, ev["player_id"], ev["type"]))

        # (f) defensive screen present vs absent when opponent has a scoring
        # threat (carrier within MA+2 of endzone)
        for k, turn in enumerate(turns):
            b = Board(turn)
            if b.carrier < 0:
                continue
            att = b.side.get(b.carrier)
            defn = "away" if att == "home" else "home"
            if turn["active_team"] != att:
                continue
            ez = 25 if att == "home" else 0
            cpos = b.pos[b.carrier]
            dist = abs(cpos[0] - ez)
            if dist > table[b.carrier]["ma"] + 2:
                continue  # not yet a real threat
            # BUG FIX (2026-07-15, coordinator caught in review): the endzone
            # a defender screens is the SAME one the attacker is racing to
            # (home attacks x=25 -> away defends x=25, not x=0). Originally
            # used the attacker's own goal line here by mistake, which
            # measured screeners on the wrong side of the pitch entirely.
            screeners = [pid for pid in b.standing(defn)
                        if min(cpos[0], ez) <= b.pos[pid][0] <= max(cpos[0], ez)
                        and abs(b.pos[pid][0] - ez) < abs(cpos[0] - ez)]
            key = "screen_present" if len(screeners) >= 2 else "screen_absent"
            hits[key].append((g["_file"], k, len(screeners), dist))

        # (g) one-turn TD: touchdown this turn, carrier's position at the
        # START of this turn (board snapshot, before events) was already far
        # enough from the endzone that GFI/dodge was needed this same turn
        # (dist > MA), i.e. a scrappy dash rather than a walk-in.
        for k, turn in enumerate(turns):
            if not turn.get("touchdown"):
                continue
            b0 = Board(turn)
            cid = b0.carrier
            if cid >= 0 and cid in table and b0.side.get(cid) == turn["active_team"]:
                ez = 25 if turn["active_team"] == "home" else 0
                start_dist = abs(b0.pos[cid][0] - ez)
                if start_dist > table[cid]["ma"]:
                    hits["one_turn_dash_td"].append((g["_file"], k, start_dist))

        # (h) stalling while leading: half 2, active side leads by >=1,
        # holds the ball, but carrier does not move closer to the endzone
        # this turn (no MOVE events bringing it closer) and doesn't score
        for k, turn in enumerate(turns):
            if turn["half"] != 2 or turn["turn"] < 4:
                continue
            act = turn["active_team"]
            lead = (turn["home_score"] - turn["away_score"]) if act == "home" \
                else (turn["away_score"] - turn["home_score"])
            if lead < 1:
                continue
            b = Board(turn)
            if b.carrier < 0 or b.side.get(b.carrier) != act:
                continue
            ez = 25 if act == "home" else 0
            start_dist = abs(b.pos[b.carrier][0] - ez)
            moved_closer = False
            cur = b.pos[b.carrier]
            for ev in turn["events"]:
                if ev["type"] == "MOVE" and ev["player_id"] == b.carrier:
                    if abs(ev["to_x"] - ez) < abs(cur[0] - ez):
                        moved_closer = True
                    cur = (ev["to_x"], ev["to_y"])
            if not moved_closer and not turn["touchdown"]:
                hits["stall_while_leading"].append((g["_file"], k, lead, start_dist))

    for k2 in sorted(hits):
        v = hits[k2]
        print(f"{k2}: {len(v)} hits; first 10: {v[:10]}")


def cmd_summary(label: str) -> None:
    """One line per game for example-picking."""
    for g in load_games(label):
        ev_count = Counter()
        ground = 0
        for t in g["turns"]:
            if not t["ball_held"]:
                ground += 1
            for ev in t["events"]:
                if ev["type"] in ("PASS", "PICKUP", "TOUCHDOWN"):
                    ev_count[(ev["type"], ev["success"])] += 1
                elif ev["type"] == "PUSH" and not (0 <= ev["to_x"] <= 25 and 0 <= ev["to_y"] <= 14):
                    ev_count[("SURF", True)] += 1
        print(f"{g['_file']} {g['home_race'][:5]:>5}/{g['away_race'][:5]:<5} "
              f"{g['home_score']}-{g['away_score']} "
              f"ground={ground}/{len(g['turns'])} "
              f"pickup_ok={ev_count[('PICKUP', True)]} pickup_fail={ev_count[('PICKUP', False)]} "
              f"pass_ok={ev_count[('PASS', True)]} pass_fail={ev_count[('PASS', False)]} "
              f"surfs={ev_count[('SURF', True)]}")


def main() -> None:
    cmd = sys.argv[1]
    if cmd == "summary":
        cmd_summary(sys.argv[2])
        return
    if cmd == "scan":
        cmd_scan_types(sys.argv[2])
        return
    if cmd == "survey":
        cmd_survey(sys.argv[2])
        return
    if cmd == "dodge_tackle":
        cmd_dodge_tackle(sys.argv[2])
        return
    if cmd == "mobility":
        cmd_mobility(sys.argv[2])
        return
    if cmd == "blocks_wrestle":
        games = list(load_games(sys.argv[2]))
        r = analyze_blocks(games)
        n = r["bad_total"]
        print(f"total negative-dice blocks: {n}")
        print(f"  of which attacker has Wrestle: {r['bad_wrestle_any']} "
              f"({r['bad_wrestle_any']/n:.1%})")
        print(f"    ...and target IS the ball carrier: {r['bad_wrestle_on_carrier']} "
              f"({r['bad_wrestle_on_carrier']/n:.1%})")
        print(f"    ...and attacker ALSO has StripBall (intentional strip-on-carrier): "
              f"{r['bad_wrestle_stripball_on_carrier']} "
              f"({r['bad_wrestle_stripball_on_carrier']/n:.1%})")
        nn = r["bad_non_wrestle"]
        print(f"\ngenuinely-bad (non-Wrestle attacker) blocks: {nn} "
              f"({nn/n:.1%} of all negative-dice blocks, vs original unfiltered 100%)")
        print(f"  of those, a better (net>=0, attacker-choice) alternative existed: "
              f"{r['bad_non_wrestle_with_better']}/{nn} = "
              f"{r['bad_non_wrestle_with_better']/nn:.1%}")
        return
    if cmd == "run":
        cmd_run(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_N)
    elif cmd == "analyze":
        cmd_analyze(sys.argv[2])
    elif cmd == "show":
        g = load_game(sys.argv[2], sys.argv[3])
        hl = set(int(x) for x in sys.argv[5].split(",")) if len(sys.argv) > 5 else None
        print(render_roles(g, int(sys.argv[4]), hl))
    else:
        raise SystemExit(f"unknown command {cmd}")


if __name__ == "__main__":
    main()
