"""Supplementary scan (2026-07-23): how often does the BLITZ macro scorer
(macro_actions.cpp:294-372) pick a target that NO blitzer can actually
reach, producing a silently dead macro?

Mechanism under test:
 - the candidate scorer iterates ALL standing enemies x all free teammates
   with no reachability check (dice*2 + bonuses, computed at pre-move
   positions);
 - on OFFENSE only the top-1 candidate becomes a macro
   (macro_actions.cpp:366-371), on defense top-2;
 - expandBlitz (macro_actions.cpp:950-981) matches the macro against micro
   BLITZ actions, which ARE reachability-filtered (rules_engine.cpp:52-70);
   if none matches it returns an empty result -> the blitz silently
   evaporates for that MCTS branch.

Measured at TURN START of every turn in the 21.07 corpus (nobody has
acted yet, so free-to-act == standing). Skills (Guard/Sprint/Horns) not
replicable through the python bindings -- dice estimates approximate, so
treat the output as an estimate, not an exact count.

Usage:  venv/bin/python scratchpad/blitz_deadmacro_scan_20260723.py
"""
import glob
import gzip
import json
import sys

sys.path.insert(0, "engine/build")

from blitz_reachability_freeness_20260723 import (
    load_roster_stats, turn_start_board, reachable_targets, dice_estimate,
    PITCH_H,
)

DATA_GLOB = "diag_replay_mine_20260721_data/g*.json.gz"


def scorer_top_targets(pos, standing, side, stats, active_side, carrier_id,
                       n_top):
    """Replicates the macro blitz-candidate scorer (macro_actions.cpp:305-365)
    at turn start, WITHOUT reachability (as the engine does).
    Returns targets sorted by best score desc."""
    st_of = {pid: stats[pid][1] for pid in pos}
    my = [p for p in pos if side[p] == active_side and standing[p]]
    enemies = [p for p in pos if side[p] != active_side and standing[p]]

    i_have_ball = carrier_id in pos and side.get(carrier_id) == active_side
    on_def = (carrier_id in pos) and not i_have_ball

    cands = []
    for d in enemies:
        best = None
        dx_, dy_ = pos[d]
        for b in my:
            dice = dice_estimate(b, d, pos, standing, side, st_of)
            score = dice * 2
            if dy_ <= 2 or dy_ >= PITCH_H - 3:
                score += 3
            elif dy_ <= 4 or dy_ >= PITCH_H - 5:
                score += 1
            if on_def:
                if d == carrier_id:
                    score += 10
                # scoring-threat bonus (MA+2 >= dist to endzone)
                ez_x = 25 if side[d] == "home" else 0
                if stats[d][0] + 2 >= abs(dx_ - ez_x):
                    score += 4
                # free-opponent bonus: no active-side TZ on the defender
                my_tz = sum(1 for m in my
                            if max(abs(pos[m][0] - dx_), abs(pos[m][1] - dy_)) == 1)
                if my_tz == 0:
                    score += 2
            else:
                if i_have_ball and carrier_id in pos:
                    cx, cy = pos[carrier_id]
                    if max(abs(dx_ - cx), abs(dy_ - cy)) <= 2:
                        score += 2
            if best is None or score > best:
                best = score
        if best is not None:
            cands.append((best, d))
    cands.sort(key=lambda t: -t[0])
    return [d for _, d in cands[:n_top]]


def main():
    get_roster = load_roster_stats()
    files = sorted(glob.glob(DATA_GLOB))

    stats_by_ctx = {"offense": [0, 0], "defense": [0, 0]}  # [dead, total]

    for fp in files:
        rec = json.load(gzip.open(fp, "rt"))
        stats = get_roster(rec["home_race"], rec["away_race"])
        for t in rec["turn_logs"]:
            pos, standing, side = turn_start_board(t)
            active = t["active_team"]
            carrier = t.get("ball_carrier_id", -1) if t.get("ball_held") else -1
            if carrier not in side:
                carrier = -1
            i_have_ball = carrier != -1 and side[carrier] == active
            on_def = carrier != -1 and not i_have_ball
            if carrier == -1:
                continue  # loose ball: scorer runs but context ambiguous; skip
            ctx = "offense" if i_have_ball else "defense"
            n_top = 2 if on_def else 1

            top = scorer_top_targets(pos, standing, side, stats, active,
                                     carrier, n_top)
            if not top:
                continue

            # union of per-blitzer reachable sets over all standing teammates
            union = set()
            for b in pos:
                if side[b] == active and standing[b]:
                    union |= reachable_targets(b, pos, standing, side,
                                               stats[b][0])
            # macro is dead if NO emitted candidate is reachable by anyone
            dead = all(tgt not in union for tgt in top)
            stats_by_ctx[ctx][1] += 1
            if dead:
                stats_by_ctx[ctx][0] += 1

    for ctx, (dead, total) in stats_by_ctx.items():
        pct = 100 * dead / total if total else float("nan")
        print(f"{ctx}: ALL emitted BLITZ macro target(s) unreachable "
              f"(dead macro) in {dead}/{total} turns ({pct:.1f}%)")


if __name__ == "__main__":
    main()
