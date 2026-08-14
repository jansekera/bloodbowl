"""Safe-first counterfactual reconstruction (2026-07-23).

Follow-up to diag_safefirst_scan_20260722.py, which flagged 58/348 DODGE
events that moved a player into MORE tackle zones than its origin square
(56.9% of those failed vs 37.6% baseline). That scan used the TURN-START
snapshot as an approximation and explicitly could not answer whether a
genuinely safer destination existed for the same tactical purpose.

This script answers that question properly:
  1. Replays events WITHIN each turn (MOVE/DODGE/GFI position updates,
     PUSH displacement, KNOCKED_DOWN -> prone, INJURY/CASUALTY removal)
     so board state at each dodge decision is exact, not turn-start.
  2. At each DODGE, enumerates every legal single-step alternative
     (8 neighbours of origin, on-pitch, unoccupied) with its exact
     tackle-zone count and dodge target number. The DODGE event's `roll`
     field stores the TARGET number (move_handler.cpp:140), and the
     target is 7-AG + destTZ + mods (helpers.cpp:33), so an alternative
     square's target = actual_target - tz(actual_dest) + tz(alt). No
     player stats needed.
  3. Applies a same-tactical-purpose filter:
       - player continued moving afterwards -> alternative must make
         equal-or-better Chebyshev progress toward the player's actual
         final square that turn;
       - dodge was final step and player is ball carrier -> alternative
         must make equal-or-better progress toward the endzone;
       - dodge was final step, non-carrier -> alternative must land
         within Chebyshev 1 of the actual destination (same local spot).
  4. For flagged cases with a safer same-purpose alternative, replays
     scoreMoveAction (macro_actions.cpp:35) arithmetic with the
     purpose-proxy goal to classify WHY the engine picked the worse
     square (distance win / sideline penalty / tie order / unclear).

Read-only over diag_replay_mine_20260721_data. No simulation.
"""
import glob
import gzip
import json
import os

DATA = os.path.join(os.path.dirname(__file__), "..", "..",
                    "diag_replay_mine_20260721_data")

ADJ = [(-1, -1), (-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0), (1, 1)]
PITCH_W, PITCH_H = 26, 15


def on_pitch(x, y):
    return 0 <= x < PITCH_W and 0 <= y < PITCH_H


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


class Board:
    """Live board reconstructed from turn snapshot + intra-turn events."""

    def __init__(self, turn):
        self.pos = {}    # id -> (x, y)
        self.state = {}  # id -> int (0 standing, 1 prone/stunned-ish, None off)
        self.side = {}   # id -> 'home' | 'away'
        for key, side in (("home_players", "home"), ("away_players", "away")):
            for p in turn[key]:
                self.pos[p["id"]] = (p["x"], p["y"])
                self.state[p["id"]] = p["state"]
                self.side[p["id"]] = side

    def apply(self, ev):
        t, pid = ev["type"], ev["player_id"]
        if t in ("MOVE", "PUSH"):
            to = (ev["to_x"], ev["to_y"])
            if on_pitch(*to):
                self.pos[pid] = to
            else:  # crowd push
                self.pos.pop(pid, None)
                self.state[pid] = None
        elif t == "DODGE" or t == "GFI":
            # success: following MOVE event applies position (idempotent to
            # apply here too); failure: player falls PRONE at destination,
            # no MOVE event follows.
            to = (ev["to_x"], ev["to_y"])
            if on_pitch(*to):
                self.pos[pid] = to
            if not ev["success"]:
                self.state[pid] = 1  # prone
        elif t == "KNOCKED_DOWN":
            self.state[pid] = 1  # prone/stunned: exerts no TZ, occupies square
        elif t == "INJURY":
            # 2d6 injury: >=8 KO'd or worse -> off pitch; else stunned (stays)
            if ev["roll"] >= 8:
                self.pos.pop(pid, None)
                self.state[pid] = None
            else:
                self.state[pid] = 1
        elif t == "CASUALTY":
            self.pos.pop(pid, None)
            self.state[pid] = None

    def occupied(self, sq):
        return any(p == sq for p in self.pos.values())

    def tz(self, sq, my_side):
        n = 0
        for pid, p in self.pos.items():
            if self.side[pid] == my_side or self.state.get(pid) != 0:
                continue
            if cheb(p, sq) == 1:
                n += 1
        return n


def success_prob(target):
    t = min(max(target, 2), 6)  # nat 1 always fails, nat 6 always succeeds
    return (7 - t) / 6.0


def endzone_x(team_side):
    return 25 if team_side == "home" else 0


def analyze():
    files = sorted(glob.glob(os.path.join(DATA, "g*.json.gz")))
    cases = []
    total_dodges = 0

    for fp in files:
        rec = json.load(gzip.open(fp, "rt"))
        for t in rec["turn_logs"]:
            active = t["active_team"]
            events = t.get("events", [])
            board = Board(t)

            # Pre-compute each player's final position within this turn
            final_pos = {}
            fboard = Board(t)
            for ev in events:
                fboard.apply(ev)
            for pid, p in fboard.pos.items():
                final_pos[pid] = p

            # carrier at turn start
            carrier_id = t.get("ball_carrier_id", -1)

            for i, ev in enumerate(events):
                if ev["type"] == "PICKUP" and ev["success"]:
                    carrier_id = ev["player_id"]
                if ev["type"] != "DODGE":
                    board.apply(ev)
                    continue

                total_dodges += 1
                pid = ev["player_id"]
                F = (ev["from_x"], ev["from_y"])
                T = (ev["to_x"], ev["to_y"])
                tgt_num = ev["roll"]

                tz_from = board.tz(F, active)
                tz_to = board.tz(T, active)

                # candidates: legal single-step alternatives from F
                cands = []
                for dx, dy in ADJ:
                    c = (F[0] + dx, F[1] + dy)
                    if not on_pitch(*c):
                        continue
                    if c != T and board.occupied(c):
                        continue
                    tz_c = board.tz(c, active)
                    cands.append({
                        "sq": c, "tz": tz_c,
                        "target": tgt_num - tz_to + tz_c,
                    })

                # tactical-purpose goal
                fp_pos = final_pos.get(pid, T)
                if fp_pos != T:
                    goal, purpose = fp_pos, "continued_to_final"
                elif pid == carrier_id:
                    ez = endzone_x(active)
                    goal, purpose = (ez, T[1]), "carrier_endzone"
                else:
                    goal, purpose = T, "final_step_local"

                for c in cands:
                    if purpose == "final_step_local":
                        c["same_purpose"] = cheb(c["sq"], T) <= 1
                    else:
                        c["same_purpose"] = cheb(c["sq"], goal) <= cheb(T, goal)

                # enemy standing positions at this moment (for blitz-greedy
                # signature analysis) + whether this player blocks later
                enemies = [p for epid, p in board.pos.items()
                           if board.side[epid] != active
                           and board.state.get(epid) == 0]
                blocks_later = any(
                    e2["type"] in ("BLOCK",) and e2["player_id"] == pid
                    for e2 in events[i + 1:])

                cases.append({
                    "game": os.path.basename(fp), "half": t["half"],
                    "turn": t["turn"], "team": active, "player": pid,
                    "from": F, "to": T, "tz_from": tz_from, "tz_to": tz_to,
                    "target_num": tgt_num, "success": ev["success"],
                    "purpose": purpose, "goal": goal,
                    "is_carrier": pid == carrier_id,
                    "cands": cands, "enemies": enemies,
                    "blocks_later": blocks_later,
                })
                board.apply(ev)

    return cases, total_dodges


def score_move(sq, tz, goal, currently_in_tz=True):
    """Replay of scoreMoveAction arithmetic (macro_actions.cpp:35-68).
    GFI status identical across single-step candidates -> omitted."""
    s = cheb(sq, goal) * 10
    if tz > 0:
        s += (12 if currently_in_tz else 20) * tz
    if sq[1] <= 1 or sq[1] >= 13:
        s += 6
    return s


def rationalizability(c):
    """Purpose-agnostic test on the TRUE reconstructed board.

    For every possible goal square G on the pitch, replay scoreMoveAction
    arithmetic over all legal single-step candidates. Returns:
      rational_goals: # of G where the actual destination is (weakly) the
        best-scoring candidate -> the scorer COULD have picked it if the
        macro target were G.
      replan_always_safer: True if for EVERY G, every best-scoring
        candidate has tz < tz(actual dest) -> re-planning on the true
        board would have produced a strictly safer square no matter what
        the tactical goal was.
    """
    T = c["to"]
    tz_T = c["tz_to"]
    rational_goals = 0
    replan_always_safer = True
    for gx in range(PITCH_W):
        for gy in range(PITCH_H):
            G = (gx, gy)
            scores = [(score_move(a["sq"], a["tz"], G), a) for a in c["cands"]]
            best = min(s for s, _ in scores)
            s_T = next(s for s, a in scores if a["sq"] == T)
            if s_T <= best:
                rational_goals += 1
            argmin_tz_max = max(a["tz"] for s, a in scores if s == best)
            if argmin_tz_max >= tz_T:
                replan_always_safer = False
    return rational_goals, replan_always_safer


def blitz_greedy_signature(c):
    """Does this dodge match the greedy BLITZ approach-step selection in
    action_resolver.cpp:86-96 (argmin Chebyshev distance to some enemy,
    no TZ consideration)? Returns (matches, tie_break_would_fix, best_tz_in_argmin)
    maximally favourable reading: any standing enemy at distance >= 2 from
    the origin counts as a possible blitz target.
    """
    F, T = c["from"], c["to"]
    for E in c["enemies"]:
        if cheb(F, E) < 2:
            continue
        dmin = min(cheb(a["sq"], E) for a in c["cands"])
        argmin = [a for a in c["cands"] if cheb(a["sq"], E) == dmin]
        if not any(a["sq"] == T for a in argmin):
            continue
        best_tz = min(a["tz"] for a in argmin)
        if best_tz < c["tz_to"]:
            return True, True, best_tz
        # matches signature but tie-break alone wouldn't help for this E;
        # keep looking for another E where it would
    for E in c["enemies"]:
        if cheb(F, E) < 2:
            continue
        dmin = min(cheb(a["sq"], E) for a in c["cands"])
        argmin = [a for a in c["cands"] if cheb(a["sq"], E) == dmin]
        if any(a["sq"] == T for a in argmin):
            return True, False, min(a["tz"] for a in argmin)
    return False, False, None


def main():
    cases, total = analyze()
    flagged = [c for c in cases if c["tz_to"] > c["tz_from"]]
    print(f"total DODGE events reconstructed: {total}")
    print(f"flagged (tz_to > tz_from, exact mid-turn board): {len(flagged)}")
    fail_rate_all = sum(1 for c in cases if not c["success"]) / len(cases)
    if flagged:
        fail_rate_fl = sum(1 for c in flagged if not c["success"]) / len(flagged)
        print(f"failure rate: flagged {100*fail_rate_fl:.1f}% vs all {100*fail_rate_all:.1f}%")
    print()

    # purpose-agnostic scorer test on true board
    n_irrational = 0
    n_replan_safer = 0
    for c in flagged:
        rg, ras = rationalizability(c)
        c["rational_goals"] = rg
        c["replan_always_safer"] = ras
        if rg == 0:
            n_irrational += 1
        if ras:
            n_replan_safer += 1
    print(f"scorer-rationalizability on TRUE board (any goal G on pitch):")
    print(f"  NO goal G rationalizes the chosen square: {n_irrational}/{len(flagged)}"
          f"  -> provably stale-plan (open-loop replay) artifacts")
    print(f"  re-planning would be strictly safer for EVERY goal G: "
          f"{n_replan_safer}/{len(flagged)}")
    print()

    # mechanism classification: greedy blitz-approach step vs scorer/plan
    n_sig = n_fixable = n_sig_block = 0
    for c in flagged:
        sig, fixable, best_tz = blitz_greedy_signature(c)
        c["blitz_sig"] = sig
        c["tie_break_fixes"] = fixable
        c["argmin_best_tz"] = best_tz
        if sig:
            n_sig += 1
            if c["blocks_later"]:
                n_sig_block += 1
            if fixable:
                n_fixable += 1
    print("mechanism: greedy BLITZ approach-step signature "
          "(action_resolver.cpp:86-96, argmin dist to an enemy):")
    print(f"  matches signature: {n_sig}/{len(flagged)}"
          f" (of which followed by own BLOCK event same turn: {n_sig_block})")
    print(f"  TZ tie-break at equal distance would give a strictly safer square:"
          f" {n_fixable}/{len(flagged)}")
    print()

    verdict = {"safer_same_purpose": [], "safer_any_only": [], "no_better": []}
    for c in flagged:
        alts = [a for a in c["cands"] if a["sq"] != c["to"]]
        safer_sp = [a for a in alts if a["same_purpose"] and a["tz"] < c["tz_to"]]
        safer_any = [a for a in alts if a["tz"] < c["tz_to"]]
        if safer_sp:
            best = min(safer_sp, key=lambda a: a["tz"])
            c["best_alt"] = best
            # why did the engine pick the worse square?
            goal = c["goal"]
            s_actual = score_move(c["to"], c["tz_to"], goal)
            s_alt = score_move(best["sq"], best["tz"], goal)
            if s_alt < s_actual:
                c["why"] = "alt_scores_better_under_own_scorer(goal-proxy)"
            elif s_alt == s_actual:
                c["why"] = "tie_under_own_scorer(order-dependent)"
            else:
                dist_gain = (cheb(best["sq"], goal) - cheb(c["to"], goal)) * 10
                sideline = 6 if (best["sq"][1] <= 1 or best["sq"][1] >= 13) \
                    and not (c["to"][1] <= 1 or c["to"][1] >= 13) else 0
                bits = []
                if dist_gain > 0:
                    bits.append(f"distance_win(+{dist_gain})")
                if sideline:
                    bits.append("sideline_penalty_on_safe_alt(+6)")
                c["why"] = "actual_scores_better: " + (",".join(bits) or "other")
            verdict["safer_same_purpose"].append(c)
        elif safer_any:
            verdict["safer_any_only"].append(c)
        else:
            verdict["no_better"].append(c)

    n = len(flagged)
    for k, v in verdict.items():
        print(f"{k}: {len(v)}/{n}")
    print()

    sp = verdict["safer_same_purpose"]
    if sp:
        from collections import Counter
        whys = Counter(c["why"] for c in sp)
        print("why engine picked the worse square (safer_same_purpose cases):")
        for w, k in whys.most_common():
            print(f"  {k:3d}  {w}")
        print()
        avg_actual_p = sum(success_prob(c["target_num"]) for c in sp) / len(sp)
        avg_alt_p = sum(success_prob(c["best_alt"]["target"]) for c in sp) / len(sp)
        print(f"avg dodge success prob: actual {100*avg_actual_p:.0f}% vs best same-purpose alt {100*avg_alt_p:.0f}%")
        fails = sum(1 for c in sp if not c["success"])
        print(f"of these {len(sp)} cases, actual dodge FAILED in {fails}")
        print()
        print("=== detailed walkthroughs (up to 10) ===")
        for c in sp[:10]:
            b = c["best_alt"]
            print(f"\n{c['game']} H{c['half']} T{c['turn']} {c['team']} player {c['player']}"
                  f" {'CARRIER' if c['is_carrier'] else ''}")
            print(f"  from {c['from']} (tz={c['tz_from']}) -> chose {c['to']}"
                  f" (tz={c['tz_to']}, target {c['target_num']}+,"
                  f" p={100*success_prob(c['target_num']):.0f}%,"
                  f" {'OK' if c['success'] else 'FAILED'})")
            print(f"  purpose={c['purpose']} goal={c['goal']}")
            print(f"  best same-purpose alt {b['sq']} (tz={b['tz']},"
                  f" target {b['target']}+, p={100*success_prob(b['target']):.0f}%)")
            print(f"  why: {c['why']}")
            avail = ", ".join(f"{a['sq']}tz{a['tz']}{'*' if a['same_purpose'] else ''}"
                              for a in c["cands"])
            print(f"  all legal steps (*=same purpose): {avail}")

    # dump full JSON for review
    out = os.path.join(os.path.dirname(__file__), "flagged_cases.json")
    with open(out, "w") as f:
        json.dump({"flagged": flagged, "verdict_counts":
                   {k: len(v) for k, v in verdict.items()}}, f, indent=1, default=str)
    print(f"\nfull dump: {out}")


if __name__ == "__main__":
    main()
