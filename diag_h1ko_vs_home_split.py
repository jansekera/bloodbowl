#!/usr/bin/env python3
"""H1-knockout vs home-slot split for the opening-drive conversion gap.

WHY (2026-07-15, follow-up to diag_first_possession postfix_20260714):
the postfix report CONFIRMED a +14.2pp home-slot win edge (p=0.0133) and an
opening-drive conversion gap: H1 opening (always received by HOME post
H2-kickoff-fix) converts 25.3% vs H2 opening (always AWAY) 17.3%. "Home" and
"H1" are perfectly confounded by the fixed kickoff schedule, so two hypotheses
remain:

  (a) home-slot bug -- something structurally favors the side coded "home"
      (formation/features/pathfinding), independent of half;
  (b) H2-return effect -- players knocked out (KO/casualty) during H1 fail to
      return correctly for the H2 opening, degrading the H2 receiver, which
      post-fix is always AWAY. Mechanism is half-specific, not side-specific.

CODE-READING PRELUDE (must be verified e2e by this script, never assumed --
see project_bloodbowl_h2_kickoff_bug class of bugs): game_simulator.cpp
setupHalfOrDrive() resets ALL players to OFF_PITCH and buildTeam() re-places
all 11 per side with state=STANDING at EVERY kickoff (post-TD included;
resetHalfState only gates rerolls/turn clock). If that is what actually runs,
KO/casualty persistence beyond the current drive is impossible and (b) cannot
operate at all. The decisive measurement is therefore:

  1. RETURN AUDIT: on-pitch player counts (len of home_players/away_players in
     the get_turn_logs() snapshots -- forEachOnPitch omits KO/INJURED/DEAD) at
     the first snapshot of every drive. Expectation under the code reading:
     11v11 at 100% of drive starts. Any deficit = KO persistence exists and
     (b) is live.
  2. KO STRATIFICATION (the pre-registered test from the 2026-07-14 notes):
     H1-opening (home) vs H2-opening (away) conversion split by whether any
     player was removed (KO/casualty, snapshot-count dip OR removal event)
     during H1. Gap persisting in ZERO-removal games rules the (b) mechanism
     out of the gap; gap concentrated in removal games supports (b).
  3. SCORE-STATE STRATA (replicates the offline reanalysis of the postfix
     arms): H2-opening conversion by halftime score. Controls the main
     surviving H2-specific behavioral channel (nonzero score in H2).

Removal detection is slot-based (player id 1-11 = home slot, 12-22 = away
slot) and uses the union of two signals: (i) any snapshot of the half where a
side's on-pitch count < 11, (ii) any INJURY event with roll >= 8 (KO branch of
injury.cpp; ThickSkull saves emit SKILL, not INJURY) or any CASUALTY event.
(i) misses removals on a drive's final turn (no later snapshot in the drive),
(ii) catches those; together they cover both.

USAGE (repo root, inside venv, never while training owns the engine build):

    python3 diag_h1ko_vs_home_split.py run <label> [N]   # N seeds x fwd+swp
    python3 diag_h1ko_vs_home_split.py report <label>
    python3 diag_h1ko_vs_home_split.py selftest

Arms persist as arm_h1ko_split_<label>_{fwd,swp}.json (same mirror eval
config as diag_first_possession: weights_best both sides, MCTS=100,
vf_blend=0, TV=1000, gate dirichlet/exploration).
"""
from __future__ import annotations

import math
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from diag_first_possession import (MCTS, POLICY_PATH, TV, VF_BLEND, W,
                                   binom_two_sided, wilson)
from run_iteration import GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C, _RACES

BASE_SEED = 20260715          # fresh panel per diag_utils convention
DEFAULT_N = 500
ORIENTS = ("fwd", "swp")
SIDES = ("home", "away")


def arm_path(label: str, orient: str) -> str:
    return f"arm_h1ko_split_{label}_{orient}.json"


def _pid_side(pid: int) -> str:
    """Slot side of a player id (home slots 1-11, away slots 12-22)."""
    return "home" if 1 <= pid <= 11 else "away"


# --------------------------------------------------------------------------
# Per-game reduction (runs inside the worker; only a small dict crosses the
# process boundary). Extends diag_first_possession.summarize_drives with
# on-pitch counts, removal signals and the halftime score.
# --------------------------------------------------------------------------

def summarize_game(turns: list, hs: int, as_: int, swap: bool) -> dict:
    n = len(turns)
    if n == 0:
        return {"hs": hs, "as": as_, "swap": swap, "drives": [],
                "anomalies": 1, "n_snapshots": 0, "ht": None,
                "min_np": {}, "ev_ko": {}, "ev_cas": {}}

    # -- removal signals per (half, slot-side) --------------------------------
    min_np = {h: {s: 11 for s in SIDES} for h in (1, 2)}
    ev_ko = {h: {s: 0 for s in SIDES} for h in (1, 2)}
    ev_cas = {h: {s: 0 for s in SIDES} for h in (1, 2)}
    ht = None
    for t in turns:
        h = t["half"]
        if ht is None and h == 2:
            ht = (t["home_score"], t["away_score"])
        for s in SIDES:
            np_ = len(t[f"{s}_players"])
            if np_ < min_np[h][s]:
                min_np[h][s] = np_
        for ev in t["events"]:
            if ev["type"] == "INJURY" and ev["roll"] >= 8:
                ev_ko[h][_pid_side(ev["player_id"])] += 1
            elif ev["type"] == "CASUALTY":
                ev_cas[h][_pid_side(ev["player_id"])] += 1

    # -- drive segmentation (same rules as diag_first_possession) -------------
    starts = [0]
    for k in range(1, n):
        if turns[k]["half"] != turns[k - 1]["half"] or turns[k - 1]["touchdown"]:
            starts.append(k)

    drives: list[dict] = []
    anomalies = 0
    for d, s in enumerate(starts):
        e = (starts[d + 1] - 1) if d + 1 < len(starts) else n - 1
        seg = turns[s:e + 1]
        recv = seg[0]["active_team"]
        before = (seg[0]["home_score"], seg[0]["away_score"])
        if d + 1 < len(starts):
            nxt = turns[starts[d + 1]]
            after = (nxt["home_score"], nxt["away_score"])
        else:
            after = (hs, as_)
        dh, da = after[0] - before[0], after[1] - before[1]
        if dh < 0 or da < 0 or dh + da > 1:
            anomalies += 1
        scorer = "home" if dh > 0 else ("away" if da > 0 else None)
        if bool(seg[-1]["touchdown"]) != (scorer is not None):
            anomalies += 1
        recv_turns = sum(1 for t in seg if t["active_team"] == recv)
        if scorer is not None:
            end = "td_recv" if scorer == recv else "td_kick"
        elif d + 1 < len(starts):
            end = "half"
        else:
            end = "game"
        drives.append({
            "half": seg[0]["half"],
            "recv": recv,
            "end": end,
            "scorer": scorer,
            "recv_turns": recv_turns,
            # on-pitch counts at the drive's first snapshot: the return audit
            "np0": {s: len(seg[0][f"{s}_players"]) for s in SIDES},
        })

    return {"hs": hs, "as": as_, "swap": swap, "drives": drives,
            "anomalies": anomalies, "n_snapshots": n, "ht": ht,
            "min_np": min_np, "ev_ko": ev_ko, "ev_cas": ev_cas}


def _split_game(args: tuple) -> dict:
    """One mirror game -> summary dict. Task tuple identical in shape to
    diag_first_possession._fp_game (swap exchanges rosters AND weight paths,
    a no-op in the mirror)."""
    (seed, race_idx, home_w, away_w, mcts, vf_blend, tv,
     leaf_lookahead, policy_path, swap) = args
    import bb_engine
    race_a = _RACES[race_idx % len(_RACES)]
    race_b = _RACES[(race_idx + 1) % len(_RACES)]
    if swap:
        race_a, race_b = race_b, race_a
        home_w, away_w = away_w, home_w
    hr = bb_engine.get_developed_roster(race_a, tv)
    ar = bb_engine.get_developed_roster(race_b, tv)
    lgr = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=mcts,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=vf_blend,
        leaf_lookahead=leaf_lookahead,
        policy_weights_path=policy_path,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    )
    return summarize_game(lgr.get_turn_logs(),
                          lgr.result.home_score, lgr.result.away_score, swap)


# --------------------------------------------------------------------------
# Derived per-game predicates.
# --------------------------------------------------------------------------

def _norm(g: dict) -> dict:
    """JSON round-trip turns int keys into strings; normalize access."""
    for key in ("min_np", "ev_ko", "ev_cas"):
        if g.get(key) and not all(isinstance(k, int) for k in g[key]):
            g[key] = {int(k): v for k, v in g[key].items()}
    return g


def h1_removal(g: dict, side: str | None = None) -> bool:
    """Any KO/casualty removal during H1 for `side` (slot), or either side."""
    sides = SIDES if side is None else (side,)
    mn = g["min_np"].get(1, {})
    ko = g["ev_ko"].get(1, {})
    cas = g["ev_cas"].get(1, {})
    return any(mn.get(s, 11) < 11 or ko.get(s, 0) > 0 or cas.get(s, 0) > 0
               for s in sides)


def opening(g: dict, half: int) -> dict | None:
    for d in g["drives"]:
        if d["half"] == half:
            return d
    return None


def conv(d: dict | None) -> bool | None:
    return None if d is None else d["end"] == "td_recv"


# --------------------------------------------------------------------------
# Reporting.
# --------------------------------------------------------------------------

def _ci_str(k: int, n: int) -> str:
    p, lo, hi = wilson(k, n)
    return f"{k}/{n} = {100 * p:.1f}% [{100 * lo:.1f},{100 * hi:.1f}]"


def _stratum_block(games: list[dict], name: str) -> None:
    h1 = [conv(opening(g, 1)) for g in games]
    h2 = [conv(opening(g, 2)) for g in games]
    pairs = [(a, b) for a, b in zip(h1, h2) if a is not None and b is not None]
    k1, n1 = sum(1 for a in h1 if a), sum(1 for a in h1 if a is not None)
    k2, n2 = sum(1 for b in h2 if b), sum(1 for b in h2 if b is not None)
    print(f"  {name}: n={len(games)} games", flush=True)
    print(f"    H1 opening (home recv) conversion: {_ci_str(k1, n1)}", flush=True)
    print(f"    H2 opening (away recv) conversion: {_ci_str(k2, n2)}", flush=True)
    if n1 and n2:
        p1, p2 = k1 / n1, k2 / n2
        se = math.sqrt(p1 * (1 - p1) / n1 + p2 * (1 - p2) / n2)
        diff = p1 - p2
        lo, hi = diff - 1.96 * se, diff + 1.96 * se
        n10 = sum(1 for a, b in pairs if a and not b)
        n01 = sum(1 for a, b in pairs if b and not a)
        p_mcn = binom_two_sided(n10, n10 + n01)
        print(f"    gap (H1-H2): {100 * diff:+.1f}pp [{100 * lo:+.1f},"
              f"{100 * hi:+.1f}]  paired discordants {n10}:{n01} "
              f"exact p={p_mcn:.4f}"
              f"  -> {'CONFIRMED' if p_mcn < 0.05 else 'INCONCLUSIVE'}",
              flush=True)


def report(label: str) -> None:
    arms = {}
    for orient in ORIENTS:
        p = Path(arm_path(label, orient))
        if p.exists():
            _, seeds, res = du.load_arm(p)
            arms[orient] = (seeds, {i: _norm(g) for i, g in res.items()})
    if not arms:
        sys.exit(f"no arms found for label {label!r} -- run first")

    all_games: list[dict] = []
    print(f"=== H1-KO vs home-slot split: label={label!r} "
          f"orientations={list(arms)} ===", flush=True)
    for orient, (_seeds, res) in arms.items():
        games = list(res.values())
        all_games += games
        w = sum(1 for g in games if g["hs"] > g["as"])
        d = sum(1 for g in games if g["hs"] == g["as"])
        anom = sum(g["anomalies"] for g in games)
        print(f"  {orient}: n={len(games)}  {w}W {d}D {len(games) - w - d}L  "
              f"draws {100 * d / len(games):.1f}%  anomalies={anom}", flush=True)

    # ---- 1. RETURN AUDIT ----------------------------------------------------
    print("\n--- 1. drive-start return audit (on-pitch counts at first "
          "snapshot of each drive) ---", flush=True)
    cats = {"H1 opening": Counter(), "H2 opening": Counter(),
            "post-TD": Counter()}
    deficit_h2 = 0
    n_h2 = 0
    for g in all_games:
        seen = set()
        for d in g["drives"]:
            h = d["half"]
            if h not in seen:
                seen.add(h)
                cat = "H1 opening" if h == 1 else "H2 opening"
                if h == 2:
                    n_h2 += 1
                    if min(d["np0"].values()) < 11:
                        deficit_h2 += 1
            else:
                cat = "post-TD"
            cats[cat][(d["np0"]["home"], d["np0"]["away"])] += 1
    for cat, c in cats.items():
        print(f"  {cat:11s}: {dict(sorted(c.items()))}", flush=True)
    print(f"  H2 openings starting with <11 players on either side: "
          f"{deficit_h2}/{n_h2}", flush=True)
    print("  VERDICT: " + (
        "players ALWAYS return for the H2 opening -> hypothesis (b) "
        "KO-return mechanism has nothing to act on" if deficit_h2 == 0 else
        "KO persistence OBSERVED at H2 openings -> hypothesis (b) is live, "
        "code reading of setupHalfOrDrive was wrong"), flush=True)

    # ---- 2. KO stratification -------------------------------------------------
    print("\n--- 2. opening conversion stratified by H1 removals "
          "(KO/casualty) ---", flush=True)
    for strat_name, side in (("any-slot H1 removal", None),
                             ("away-slot H1 removal (the (b) target)", "away"),
                             ("home-slot H1 removal", "home")):
        yes = [g for g in all_games if h1_removal(g, side)]
        no = [g for g in all_games if not h1_removal(g, side)]
        print(f"\n  stratifier: {strat_name}  "
              f"(prevalence {len(yes)}/{len(all_games)})", flush=True)
        _stratum_block(no, "ZERO-removal games")
        _stratum_block(yes, "removal games")

    # ---- 3. score-state strata ------------------------------------------------
    print("\n--- 3. H2-opening (away) conversion by halftime score ---",
          flush=True)
    by_state: dict[str, list[int]] = {}
    for g in all_games:
        d2 = opening(g, 2)
        if d2 is None or g["ht"] is None:
            continue
        h, a = g["ht"]
        state = ("0-0" if (h, a) == (0, 0) else
                 "home-leads" if h > a else
                 "away-leads" if a > h else "tied>0")
        by_state.setdefault(state, [0, 0])
        by_state[state][0] += int(d2["end"] == "td_recv")
        by_state[state][1] += 1
    for state in ("0-0", "home-leads", "away-leads", "tied>0"):
        if state in by_state:
            print(f"  {state:11s}: {_ci_str(*by_state[state])}", flush=True)

    # ---- companion overall numbers ---------------------------------------------
    print("\n--- companion: pooled opening conversions ---", flush=True)
    k1 = sum(1 for g in all_games if conv(opening(g, 1)))
    n1 = sum(1 for g in all_games if opening(g, 1) is not None)
    k2 = sum(1 for g in all_games if conv(opening(g, 2)))
    n2 = sum(1 for g in all_games if opening(g, 2) is not None)
    print(f"  H1 opening: {_ci_str(k1, n1)}", flush=True)
    print(f"  H2 opening: {_ci_str(k2, n2)}", flush=True)
    w = sum(1 for g in all_games if g["hs"] > g["as"])
    l = sum(1 for g in all_games if g["hs"] < g["as"])
    if w + l:
        print(f"  home slot decisive: {w}W {l}L  edge "
              f"{100 * (w - l) / (w + l):+.1f}pp of decisive  "
              f"exact p={binom_two_sided(w, w + l):.4f}", flush=True)


# --------------------------------------------------------------------------
# run
# --------------------------------------------------------------------------

def run(label: str, n: int) -> None:
    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    print(f"=== H1-KO split run: label={label!r} N={n} seeds x "
          f"{len(ORIENTS)} orientations ===", flush=True)
    print(f"weights={W} policy={POLICY_PATH} MCTS={MCTS} vf_blend={VF_BLEND} "
          f"tv={TV} base_seed={BASE_SEED}", flush=True)
    for orient in ORIENTS:
        swap = orient == "swp"
        tasks = [(s, i, W, W, MCTS, VF_BLEND, TV, False, POLICY_PATH, swap)
                 for i, s in enumerate(seeds)]
        res = du.run_arm(f"{label}-{orient}", tasks, game_fn=_split_game,
                         mcts_iterations=MCTS)
        du.save_arm(arm_path(label, orient), f"{label}-{orient}", seeds, res)
    report(label)


# --------------------------------------------------------------------------
# selftest (pure python, no engine import)
# --------------------------------------------------------------------------

def _selftest() -> None:
    def snap(half, team, hsc, asc, td=False, np_home=11, np_away=11,
             events=()):
        return {"half": half, "active_team": team, "home_score": hsc,
                "away_score": asc, "touchdown": td,
                "home_players": [{"id": i} for i in range(1, np_home + 1)],
                "away_players": [{"id": 11 + i} for i in range(1, np_away + 1)],
                "events": list(events)}

    ko_ev = {"type": "INJURY", "roll": 9, "player_id": 15}      # away-slot KO
    stun_ev = {"type": "INJURY", "roll": 6, "player_id": 3}     # stun, no removal
    cas_ev = {"type": "CASUALTY", "roll": 11, "player_id": 4}   # home-slot cas

    # game 1: H1 home scores turn 2 with an away KO visible in the snapshot;
    # H2 away receives with everyone back (np 11/11), no conversion.
    turns = [
        snap(1, "home", 0, 0, events=[ko_ev]),
        snap(1, "away", 0, 0, np_away=10),
        snap(1, "home", 0, 0, td=True, np_away=10),
        snap(1, "away", 1, 0),                       # post-TD drive, all back
        snap(2, "away", 1, 0),                       # H2 opening, away recv
        snap(2, "home", 1, 0),
    ]
    g = summarize_game(turns, 1, 0, swap=False)
    assert g["anomalies"] == 0, g
    assert len(g["drives"]) == 3
    assert g["drives"][0]["np0"] == {"home": 11, "away": 11}
    assert g["drives"][2]["half"] == 2 and g["drives"][2]["recv"] == "away"
    assert g["ht"] == (1, 0)
    assert g["min_np"][1]["away"] == 10 and g["min_np"][1]["home"] == 11
    assert g["ev_ko"][1]["away"] == 1 and g["ev_ko"][1]["home"] == 0
    assert h1_removal(g) and h1_removal(g, "away") and not h1_removal(g, "home")
    assert conv(opening(g, 1)) is True and conv(opening(g, 2)) is False

    # game 2: stun only (roll<8) -> no removal; casualty event on last turn of
    # a drive is caught by the event signal even with counts never dipping.
    turns2 = [
        snap(1, "home", 0, 0, events=[stun_ev]),
        snap(1, "away", 0, 0, events=[cas_ev]),
        snap(2, "away", 0, 0),
    ]
    g2 = summarize_game(turns2, 0, 0, swap=False)
    assert not h1_removal(g2, "away")
    assert h1_removal(g2, "home") and h1_removal(g2)   # via cas_ev
    assert conv(opening(g2, 1)) is False and conv(opening(g2, 2)) is False
    assert g2["ht"] == (0, 0)

    # JSON round-trip normalization
    import json
    g3 = _norm(json.loads(json.dumps(g)))
    assert h1_removal(g3, "away") and g3["min_np"][1]["away"] == 10

    print("diag_h1ko_vs_home_split self-test: ALL PASS")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "run" and len(sys.argv) >= 3:
        run(sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_N)
    elif mode == "report" and len(sys.argv) >= 3:
        report(sys.argv[2])
    elif mode == "selftest":
        _selftest()
    else:
        sys.exit(__doc__)
