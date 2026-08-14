"""Corrected blitz free-vs-covered target analysis (2026-07-23).

Redoes the 22.07 freeness scan (diag_blitz_usage_scan_20260722.py) with the
reachability confound controlled. The 22.07 scan compared the chosen
target's free/covered rate (48.9% free) against ALL standing opponents on
the board (77.3% free) -- but far-away opponents are counted in that
baseline while the blitzer could never have targeted them, so the
comparison is confounded by reachability.

This script:
 1. Identifies BLITZ decisions in the 24-game 21.07 replay corpus.
    A blitz = BLOCK event whose attacker has >=1 MOVE/GFI/DODGE event
    earlier in the same turn (Move-then-Block is only legal as a blitz;
    engine logs blitz movement as ordinary move events, see
    action_resolver.cpp case ActionType::BLITZ). BLOCKs with no prior
    movement are ambiguous (plain block OR adjacent-declared blitz) and
    are analyzed separately.
 2. Reconstructs the DECISION-TIME board (positions/states of all players
    just before the blitzer's first movement step) by replaying the
    turn's event stream from the turn-start snapshot.
 3. Computes the blitzer's REACHABLE TARGET SET, replicating
    canReachAdjacentTo (pathfinder.cpp:20): BFS through empty on-pitch
    squares, 8-neighborhood, range = MA + 2 GFI, cannot enter occupied
    squares, target reachable if a visited square is chebyshev-adjacent
    to it; already-adjacent targets are reachable. MA comes from
    bb_engine.setup_half with the same get_developed_roster(race, 1200)
    calls the corpus used (player id -> stats mapping is deterministic).
 4. Compares the chosen target's free/covered status against the OTHER
    REACHABLE targets at the same decision point (paired, within-decision)
    with a uniform-random-choice permutation null. "Free" = zero standing
    attacker-side teammates (excluding the blitzer) adjacent to the
    target -- the 22.07 definition, kept for comparability.
 5. Also replicates the macro scorer's dice estimate
    (macro_actions.cpp:103 getBlockDiceCount, incl. its quirk of counting
    defensive assists around the blitzer's PRE-MOVE position) to test the
    mechanism hypothesis: covered targets carry attacker assists -> more
    dice -> dice*2 dominates the score -> covered targets get chosen.

KNOWN LIMITATIONS (all conservative, none should flip the sign):
 - Skills are not queryable through the python bindings (SkillName enum
   not bound), so Guard assists, Sprint (3 GFI) and JumpUp are ignored.
   Dice estimates are ST+positional-assist approximations.
 - Decision-time board is exact for positions of movers and pushes, but
   stand-ups are not logged; a prone-at-turn-start teammate that stood up
   before the blitz is still counted as non-assisting (matches the
   turn-start approximation of the 22.07 scan).
 - KO/CASUALTY removals mid-turn are tracked via KNOCKED_DOWN/CASUALTY
   events (player marked non-standing either way).

Validation gate: the chosen target must fall inside the computed
reachable set. The observed agreement rate is printed; if it were low the
reachability model would be wrong and the analysis void.

Usage:  venv/bin/python scratchpad/blitz_reachability_freeness_20260723.py
Read-only: touches no production files, launches no games.
"""
import glob
import gzip
import json
import random
import sys
from collections import Counter

sys.path.insert(0, "engine/build")

PITCH_W, PITCH_H = 26, 15
MOVE_EVENTS = ("MOVE", "GFI", "DODGE")  # events that place player at (to_x,to_y)
DATA_GLOB = "diag_replay_mine_20260721_data/g*.json.gz"


# ---------------------------------------------------------------- rosters
def load_roster_stats():
    """player id -> (MA, ST) per (home_race, away_race), via the engine's
    deterministic setup path (same rosters the corpus was generated with)."""
    import bb_engine as bb

    cache = {}

    def get(home_race, away_race):
        key = (home_race, away_race)
        if key not in cache:
            gs = bb.GameState()
            hr = bb.get_developed_roster(home_race, 1200)
            ar = bb.get_developed_roster(away_race, 1200)
            bb.setup_half(gs, hr, ar)
            stats = {}
            for pid in range(1, 23):
                p = gs.get_player(pid)
                stats[pid] = (p.stats.movement, p.stats.strength)
            cache[key] = stats
        return cache[key]

    return get


# ---------------------------------------------------------- board tracking
def turn_start_board(t):
    """pos[pid]=(x,y), standing[pid]=bool, side[pid]='home'|'away'."""
    pos, standing, side = {}, {}, {}
    for sname, plist in (("home", t["home_players"]), ("away", t["away_players"])):
        for p in plist:
            pos[p["id"]] = (p["x"], p["y"])
            standing[p["id"]] = (p["state"] == 0)  # board_snapshot.cpp: 0=STANDING
            side[p["id"]] = sname
    return pos, standing, side


def apply_event(ev, pos, standing):
    et = ev["type"]
    pid = ev["player_id"]
    if et in MOVE_EVENTS or et == "PUSH":
        if pid in pos:
            pos[pid] = (ev["to_x"], ev["to_y"])
            if et in MOVE_EVENTS and ev.get("success", True):
                standing[pid] = True  # moving implies standing (stand-up not logged)
            if et in ("GFI", "DODGE") and not ev.get("success", True):
                standing[pid] = False  # fell at destination
    elif et in ("KNOCKED_DOWN", "CASUALTY"):
        if pid in standing:
            standing[pid] = False


# ------------------------------------------------------------ reachability
def reachable_targets(blitzer_id, pos, standing, side, ma):
    """Replicates canReachAdjacentTo (pathfinder.cpp:20) for a standing
    blitzer with full MA: returns set of enemy ids reachable for a blitz."""
    bx, by = pos[blitzer_id]
    my_side = side[blitzer_id]
    max_range = ma + 2  # movementRemaining + 2 GFI (Sprint ignored)

    occupied = {pos[pid] for pid in pos
                if standing[pid] is not None and _on_board(pos[pid])}
    # occupied = every on-pitch player square (any state blocks movement,
    # matching state.getPlayerAtPosition in the BFS)
    occupied = set()
    for pid, xy in pos.items():
        if _on_board(xy):
            occupied.add(xy)

    enemies = [pid for pid in pos
               if side[pid] != my_side and standing[pid] and _on_board(pos[pid])]

    # BFS from blitzer over empty squares
    from collections import deque
    dist = {(bx, by): 0}
    q = deque([(bx, by)])
    while q:
        cx, cy = q.popleft()
        d = dist[(cx, cy)]
        if d >= max_range:
            continue
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                nx, ny = cx + dx, cy + dy
                if not (0 <= nx < PITCH_W and 0 <= ny < PITCH_H):
                    continue
                if (nx, ny) in dist:
                    continue
                if (nx, ny) in occupied:
                    continue
                dist[(nx, ny)] = d + 1
                q.append((nx, ny))

    out = set()
    for eid in enemies:
        ex, ey = pos[eid]
        if max(abs(ex - bx), abs(ey - by)) == 1:
            out.add(eid)  # already adjacent
            continue
        # any visited square adjacent to enemy (start square excluded, as in
        # pathfinder.cpp curPos != player.position -- irrelevant when dist>1)
        for sq, d in dist.items():
            if sq == (bx, by):
                continue
            if max(abs(sq[0] - ex), abs(sq[1] - ey)) == 1:
                out.add(eid)
                break
    return out


def _on_board(xy):
    return 0 <= xy[0] < PITCH_W and 0 <= xy[1] < PITCH_H


# ------------------------------------------------------------- freeness
def adjacent_ids(xy, pos, standing, exclude=()):
    x, y = xy
    out = []
    for pid, (px, py) in pos.items():
        if pid in exclude or not standing[pid]:
            continue
        if max(abs(px - x), abs(py - y)) == 1:
            out.append(pid)
    return out


def is_free(target_id, blitzer_id, pos, standing, side):
    """22.07 definition: zero standing attacker-side teammates (excluding
    the blitzer) adjacent to the target."""
    att_side = side[blitzer_id]
    adj = adjacent_ids(pos[target_id], pos, standing, exclude=(blitzer_id,))
    return not any(side[a] == att_side for a in adj)


def count_assists(target_pos, assisting_side, pos, standing, side,
                  exclude1, exclude2, tz_exclude):
    """Replicates helpers.cpp countAssists WITHOUT Guard (skills not
    queryable): adjacent standing teammate assists iff it is in zero enemy
    TZ (excluding the player being blocked)."""
    n = 0
    for pid in adjacent_ids(target_pos, pos, standing, exclude=(exclude1, exclude2)):
        if side[pid] != assisting_side:
            continue
        enemy_tz = sum(
            1 for e in adjacent_ids(pos[pid], pos, standing, exclude=(tz_exclude,))
            if side[e] != assisting_side)
        if enemy_tz == 0:
            n += 1
    return n


def dice_estimate(blitzer_id, target_id, pos, standing, side, st_of):
    """Replicates macro_actions.cpp getBlockDiceCount, incl. defensive
    assists counted around the blitzer's CURRENT (pre-move) square."""
    att_st = st_of[blitzer_id]
    def_st = st_of[target_id]
    att_side = side[blitzer_id]
    def_side = side[target_id]
    att_assists = count_assists(pos[target_id], att_side, pos, standing, side,
                                blitzer_id, target_id, target_id)
    def_assists = count_assists(pos[blitzer_id], def_side, pos, standing, side,
                                target_id, blitzer_id, blitzer_id)
    a, d = att_st + att_assists, def_st + def_assists
    if a > 2 * d:
        return 3
    if a > d:
        return 2
    if a == d:
        return 1
    if d > 2 * a:
        return -3
    return -2


# ------------------------------------------------------------------ main
def main():
    get_roster = load_roster_stats()
    files = sorted(glob.glob(DATA_GLOB))
    rng = random.Random(20260723)

    decisions = []          # strict blitzes (moved-then-blocked)
    ambiguous_blocks = 0    # BLOCK with no prior movement (block OR adj-blitz)
    chosen_unreachable = 0
    naive_pop_free = naive_pop_total = 0

    for fp in files:
        rec = json.load(gzip.open(fp, "rt"))
        stats = get_roster(rec["home_race"], rec["away_race"])
        for t in rec["turn_logs"]:
            events = t.get("events", [])
            # group consecutive BLOCKs by same (attacker,target) = one decision
            block_idx = []
            last_pair = None
            for i, ev in enumerate(events):
                if ev["type"] == "BLOCK":
                    pair = (ev["player_id"], ev["target_id"])
                    if pair != last_pair:
                        block_idx.append(i)
                    last_pair = pair
                else:
                    last_pair = None

            for bi in block_idx:
                ev = events[bi]
                att, tgt = ev["player_id"], ev["target_id"]
                # attacker's first movement event this turn (blitz move steps)
                first_move = None
                for j in range(bi):
                    e2 = events[j]
                    if e2["player_id"] == att and e2["type"] in MOVE_EVENTS:
                        first_move = j
                        break
                if first_move is None:
                    ambiguous_blocks += 1
                    continue

                # decision-time board = replay events [0, first_move)
                pos, standing, side = turn_start_board(t)
                for j in range(first_move):
                    apply_event(events[j], pos, standing)

                if att not in pos or tgt not in pos or not standing.get(att):
                    continue

                ma_att = stats[att][0]
                st_of = {pid: stats[pid][1] for pid in pos}
                reach = reachable_targets(att, pos, standing, side, ma_att)

                if tgt not in reach:
                    chosen_unreachable += 1
                    continue

                # offense/defense at decision time (turn-start carrier approx)
                carrier = t.get("ball_carrier_id", -1)
                held = t.get("ball_held", False)
                if held and carrier in side:
                    on_off = (side[carrier] == side[att])
                else:
                    on_off = None  # loose ball

                cand = {}
                for eid in reach:
                    cand[eid] = {
                        "free": is_free(eid, att, pos, standing, side),
                        "dice": dice_estimate(att, eid, pos, standing, side, st_of),
                    }
                # naive population baseline (all standing enemies on board),
                # recomputed at decision time for the confound illustration
                for pid in pos:
                    if side[pid] != side[att] and standing[pid]:
                        naive_pop_total += 1
                        if is_free(pid, att, pos, standing, side):
                            naive_pop_free += 1

                decisions.append({
                    "chosen": tgt, "cand": cand, "on_off": on_off,
                    "n_reach": len(reach),
                })

    # ------------------------------------------------------------ report
    n = len(decisions)
    print(f"games: {len(files)}  strict blitz decisions: {n}  "
          f"ambiguous no-move BLOCKs (excluded): {ambiguous_blocks}")
    print(f"validation: chosen target OUTSIDE computed reachable set: "
          f"{chosen_unreachable} (model agreement "
          f"{100 * n / max(1, n + chosen_unreachable):.1f}%)")
    print()

    chosen_free = sum(1 for d in decisions if d["cand"][d["chosen"]]["free"])
    print(f"chosen target free: {chosen_free}/{n} ({100 * chosen_free / n:.1f}%)")
    print(f"naive whole-board baseline (decision-time): "
          f"{naive_pop_free}/{naive_pop_total} "
          f"({100 * naive_pop_free / naive_pop_total:.1f}%) free  <- 22.07-style")

    # corrected baseline: freeness among OTHER reachable targets, paired
    alt_free = alt_total = 0
    for d in decisions:
        for eid, c in d["cand"].items():
            if eid == d["chosen"]:
                continue
            alt_total += 1
            alt_free += c["free"]
    print(f"corrected baseline (other REACHABLE targets, pooled): "
          f"{alt_free}/{alt_total} ({100 * alt_free / max(1, alt_total):.1f}%) free")
    print()

    # informative decisions: reachable set contains both free and covered
    inf = [d for d in decisions
           if len({c["free"] for c in d["cand"].values()}) == 2]
    obs = sum(1 for d in inf if d["cand"][d["chosen"]]["free"])
    exp = sum(sum(c["free"] for c in d["cand"].values()) / len(d["cand"])
              for d in inf)
    print(f"informative decisions (mixed free/covered in reachable set): {len(inf)}")
    print(f"  chose FREE target: {obs}/{len(inf)} ({100 * obs / max(1, len(inf)):.1f}%)")
    print(f"  expected under uniform-random choice from reachable set: "
          f"{exp:.1f} ({100 * exp / max(1, len(inf)):.1f}%)")

    # permutation null
    sims = 100000
    ge = le = 0
    for _ in range(sims):
        s = 0
        for d in inf:
            cands = list(d["cand"].values())
            s += rng.choice(cands)["free"]
        if s >= obs:
            ge += 1
        if s <= obs:
            le += 1
    p_two = 2 * min(ge / sims, le / sims)
    print(f"  permutation p (two-sided, uniform-choice null): {p_two:.4f}")
    print()

    # dice-mechanism check: does the scorer's dice*2 term explain it?
    print("mechanism check -- dice estimates (macro scorer replica, no skills):")
    ch_dice = [d["cand"][d["chosen"]]["dice"] for d in decisions]
    alt_dice = [c["dice"] for d in decisions
                for eid, c in d["cand"].items() if eid != d["chosen"]]
    fr_dice = [c["dice"] for d in decisions for c in d["cand"].values() if c["free"]]
    cv_dice = [c["dice"] for d in decisions for c in d["cand"].values() if not c["free"]]
    _mean = lambda v: sum(v) / len(v) if v else float("nan")
    print(f"  chosen targets mean dice:      {_mean(ch_dice):+.2f}  (n={len(ch_dice)})")
    print(f"  reachable alternatives:        {_mean(alt_dice):+.2f}  (n={len(alt_dice)})")
    print(f"  free reachable candidates:     {_mean(fr_dice):+.2f}  (n={len(fr_dice)})")
    print(f"  covered reachable candidates:  {_mean(cv_dice):+.2f}  (n={len(cv_dice)})")

    # within-dice-stratum freeness choice (controls dice AND reachability)
    print()
    print("free-choice rate within decisions where chosen dice == alternative dice")
    same = [d for d in inf
            if len({c['dice'] for c in d['cand'].values()}) == 1]
    if same:
        o2 = sum(1 for d in same if d["cand"][d["chosen"]]["free"])
        e2 = sum(sum(c["free"] for c in d["cand"].values()) / len(d["cand"])
                 for d in same)
        print(f"  (mixed-freeness, uniform-dice decisions): n={len(same)}, "
              f"chose free {o2} vs expected {e2:.1f}")
    else:
        print("  no uniform-dice mixed-freeness decisions")

    # offense/defense split (the +2 free bonus exists ONLY on defense,
    # macro_actions.cpp:338-341)
    print()
    for label, want in (("DEFENSE (has +2 free bonus)", False),
                        ("OFFENSE (no free bonus)", True)):
        sub = [d for d in inf if d["on_off"] is (want)]
        if not sub:
            print(f"{label}: no informative decisions")
            continue
        o = sum(1 for d in sub if d["cand"][d["chosen"]]["free"])
        e = sum(sum(c["free"] for c in d["cand"].values()) / len(d["cand"])
                for d in sub)
        print(f"{label}: n={len(sub)}, chose free {o} "
              f"({100 * o / len(sub):.1f}%) vs expected {e:.1f} "
              f"({100 * e / len(sub):.1f}%)")

    # reachable-set size distribution, for context
    print()
    sizes = Counter(d["n_reach"] for d in decisions)
    print("reachable-set size distribution:",
          dict(sorted(sizes.items())))


if __name__ == "__main__":
    main()
