#!/usr/bin/env python3
"""Re-measure the 2026-07-09 weakness-probe asymmetry properly (2026-07-13).

What the old probe showed (research_fable_20260709.md section 5, n=48 each,
champion ALWAYS home):

    champion vs `learning` (same weights, no search): 18W 30D 0L
    champion vs FROZEN champion_backup_91.5pct_20260629: 7W 28D 13L

The frozen matchup looked alarming ("training made it worse?") but is
statistically nothing -- 7W/13L among 20 decisive games has an exact two-sided
binomial p ~= 0.26 -- and it is confounded: the champion always played HOME,
and mirror A/A runs show a home-side W-L edge of the same order (+0.7..+8pp).
This script settles it with the paired-seed instrumentation (diag_utils) that
the 2026-07-10 statistical review made mandatory for decisions.

Phase 1 -- identity trick (12 seeds x 3 games, ~5 min).
    simulate_game_logged seeds ONE shared DiceRoller and BOTH sides' MCTS from
    the single game seed (engine/python/bb_module.cpp:414,458), and rosters are
    static per (race, TV). At vf_blend=0 the trained value weights are provably
    inert in play (gating null-test finding, diag_null_weights.py, confirmed by
    measurement 2026-07-02). If that holds at the trajectory level, then
    champion-vs-FROZEN on seed S must be BIT-IDENTICAL to champion-vs-champion
    on seed S: the only difference between the two games is the away-side
    weights file, and nothing else in the process consumes it. Per seed we hash
    the full game trajectory (turn logs + score + total_actions) of:
        (a) champion vs champion      -- mirror reference
        (b) champion vs frozen        -- the matchup under suspicion
        (c) champion vs frozen AGAIN  -- replay-determinism control
    Decision rule:
      * (b) != (c) on any seed  -> ABORT. Determinism itself broke: a game is
        no longer reproducible from its seed through the worker Pool. This
        invalidates the ENTIRE paired-seed methodology (diag_utils), not just
        this probe -- nothing paired can be trusted until this is root-caused.
      * all (a) == (b)          -> the 07-09 asymmetry is PROVEN noise: the
        frozen checkpoint literally cannot have played a single action
        differently from the champion. Print that verdict, skip Phase 2.
      * some (a) != (b) with (b) == (c)  -> MAJOR FINDING: the value weights DO
        change play at vf_blend=0, which FALSIFIES the gating null-test at game
        level. Say so loudly; Phase 2 then measures the real effect size.
      * any watchdog skip in Phase 1 -> ABORT: a hang on a 36-game probe means
        the engine-hang class thought fixed by 39f689a has regressed; identity
        cannot be judged on incomplete data.

Phase 2 -- side-swapped paired run (N=300 seeds, 600 games, ~55-75 min).
    Each seed is played twice with sides swapped: arm 1 champion HOME / frozen
    away, arm 2 frozen HOME / champion away. Same seed + race_idx per pair, so
    dice stream, matchup and kickoff are shared; comparing HOME results across
    arms cancels the home-side confound exactly (the home seat is the constant,
    the occupant is the treatment). McNemar via diag_utils on home_win /
    home_loss / draw. CONFIRMED only if the 95% CI excludes zero; otherwise
    INCONCLUSIVE (which is NOT "no difference" -- quote the CI).
    Only runs when Phase 1 found divergence (or when forced via `phase2`).

Arm A -- champion vs `learning` (always runs, ~10 min, default n=96).
    W/D/L against `learning` is saturated (0 losses in 48) and uninformative,
    so this arm reports EVENT-LEVEL offense metrics from the turn logs instead:
    TD/game (both sides), pickup success rate per side (PICKUP events carry a
    success flag: engine/src/ball_handler.cpp:20), possession share (fraction
    of turn-boundary snapshots holding the ball), and max carry depth
    (normalized progress of the ball carrier toward the scoring endzone;
    home scores at x=25, away at x=0 -- engine/src/turn_handler.cpp:43).
    Champion stays HOME as in the 07-09 probe for comparability; these are
    within-game rates, not W/D/L, so the home confound does not drive them.

Config notes (deliberate, do not "fix" silently):
    * Eval search config (dirichlet_alpha=0.0, exploration_c=1.0) via
      run_iteration GATE_* -- post-3a7f208 measurement standard. The 07-09
      probe predates that fix, so absolute levels are not comparable to it.
    * TV=1000, MCTS=100, vf_blend=0.0, policy priors ON (weights_policy.json
      loaded, policy_blend=0) -- matches diag_fresh_baseline_20260710.py, the
      current reference stack.
    * Workers default 12, auto-drop to 4 if run_iteration.py is live (same
      courtesy as the 07-09 probe, which ran at 4 next to training).

Usage (from repo root; detached-friendly, all prints flushed):
    python3 diag_weakness_probe_paired.py            # = all
    python3 diag_weakness_probe_paired.py phase1     # identity trick only
    python3 diag_weakness_probe_paired.py phase2 [N] # force paired run
    python3 diag_weakness_probe_paired.py armA [n]   # event-level arm only
    python3 diag_weakness_probe_paired.py all [N]

Expected runtime at 12 workers: phase1 ~5 min, phase2 ~55-75 min, armA ~10 min.
Arm results are persisted as JSON (save_arm) for post-hoc re-analysis.
"""
from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import (GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C, _RACES,
                           _gate_game)

# --- experiment constants (house reference stack, see docstring) -------------
W = "weights_best.json"
FROZEN = "champion_backup_91.5pct_20260629/weights_best.json"
POLICY = "weights_policy.json"
TV = 1000
MCTS = 100
VF_BLEND = 0.0
BASE_SEED = 20260713          # convention: launch date
N_PHASE1 = 12                 # seeds; x3 games each
N_PHASE2 = 300                # seeds; x2 games each (side-swapped)
N_ARMA = 96                   # games vs `learning`
STAMP = "20260713"
PITCH_MAX_X = 25              # home scores at x=25, away at x=0


def _now() -> str:
    return time.strftime("[%H:%M:%S]")


def detect_workers() -> int:
    """12 workers normally; 4 if a training run (run_iteration.py) is live.

    Read-only `ps` scan -- never pkill by pattern (see feedback_pkill_self_kill).
    """
    try:
        out = subprocess.run(["ps", "-eo", "args"], capture_output=True,
                             text=True, timeout=10).stdout
    except Exception as e:  # ps failing is no reason to not measure
        print(f"  (worker autodetect failed: {e}; defaulting to 12)", flush=True)
        return 12
    live = [ln for ln in out.splitlines()
            if "run_iteration.py" in ln and "grep" not in ln]
    if live:
        print(f"  training process detected ({len(live)} run_iteration.py "
              f"line(s)) -> dropping to 4 workers", flush=True)
        return 4
    return 12


def _preflight() -> None:
    """Abort early if any input file assumption is broken."""
    missing = [p for p in (W, FROZEN, POLICY) if not Path(p).exists()]
    if missing:
        print(f"*** ABORT: missing input file(s): {missing}. Implies the "
              f"checkpoint/weights layout this script was written against "
              f"(champion_backup_91.5pct_20260629/ + repo-root weights) has "
              f"changed. Nothing was measured. ***", flush=True)
        sys.exit(1)
    if GATE_DIRICHLET_ALPHA != 0.0 or GATE_EXPLORATION_C != 1.0:
        print(f"  WARNING: non-default gate search config "
              f"(dirichlet_alpha={GATE_DIRICHLET_ALPHA}, "
              f"exploration_c={GATE_EXPLORATION_C}) -- BB_GATE_* env overrides "
              f"are active; results will not match the post-3a7f208 eval "
              f"standard.", flush=True)


# --- Phase 1: identity trick --------------------------------------------------

def _traj_hash_game(args):
    """Worker: play one gate-style game, return trajectory hash + scores.

    Identical engine call to run_iteration._gate_game (verified against
    run_iteration.py:238-248), except it keeps the LoggedGameResult and hashes
    the full trajectory instead of discarding everything but the score.
    """
    seed, race_idx, home_w, away_w = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], TV)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    )
    res = lgr.result
    # Turn logs are pure ints/bools/strs (bb_module.cpp get_turn_logs), so
    # canonical JSON is a stable byte representation of the whole game.
    payload = json.dumps(
        {"turns": lgr.get_turn_logs(),
         "score": [res.home_score, res.away_score],
         "actions": res.total_actions},
        sort_keys=True, separators=(",", ":"))
    return (hashlib.sha256(payload.encode()).hexdigest(),
            res.home_score, res.away_score, res.total_actions)


def phase1(workers: int) -> bool:
    """Returns True iff champion-vs-frozen is bit-identical to the mirror
    on every probe seed (=> asymmetry proven noise, Phase 2 unnecessary)."""
    print(f"\n{_now()} === PHASE 1: identity trick, {N_PHASE1} seeds x 3 games "
          f"===", flush=True)
    print(f"  home={W}  away(mirror)={W}  away(frozen)={FROZEN}", flush=True)
    print(f"  TV={TV} MCTS={MCTS} vf_blend={VF_BLEND} "
          f"dirichlet_alpha={GATE_DIRICHLET_ALPHA} "
          f"exploration_c={GATE_EXPLORATION_C}", flush=True)

    seeds = du.paired_seeds(N_PHASE1, base_seed=BASE_SEED)
    # Per seed i: index 3i = mirror, 3i+1 = champ-vs-frozen, 3i+2 = repeat of 3i+1.
    tasks = []
    for i, s in enumerate(seeds):
        tasks.append((s, i, W, W))
        tasks.append((s, i, W, FROZEN))
        tasks.append((s, i, W, FROZEN))
    results = du.run_arm("phase1-identity", tasks, game_fn=_traj_hash_game,
                         workers=workers, mcts_iterations=MCTS,
                         progress_every=6)

    if len(results) < len(tasks):
        print(f"\n*** ABORT (phase 1): {len(tasks) - len(results)}/{len(tasks)} "
              f"game(s) watchdog-skipped. A hang on a 36-game probe means the "
              f"engine-hang class believed fixed by 39f689a (expandScore "
              f"off-pitch-carrier) has REGRESSED, and identity cannot be "
              f"judged on incomplete data. Investigate the hang before "
              f"re-running; do not proceed to Phase 2. ***", flush=True)
        sys.exit(1)

    det_fail, diverged = [], []
    print(f"\n  {'seed':>7}  {'mirror(a)':<13} {'cvf(b)':<13} {'cvf-rep(c)':<13} "
          f"b==c a==b  score(a)/score(b)", flush=True)
    for i, s in enumerate(seeds):
        ha, hb, hc = results[3 * i], results[3 * i + 1], results[3 * i + 2]
        same_bc = hb[0] == hc[0]
        same_ab = ha[0] == hb[0]
        if not same_bc:
            det_fail.append(s)
        elif not same_ab:
            diverged.append(s)
        print(f"  {s:>7}  {ha[0][:12]:<13} {hb[0][:12]:<13} {hc[0][:12]:<13} "
              f"{'OK' if same_bc else 'FAIL':>4} {'OK' if same_ab else 'DIFF':>4}"
              f"  {ha[1]}-{ha[2]}/{hb[1]}-{hb[2]}", flush=True)

    if det_fail:
        print(f"\n*** ABORT (phase 1): replay-determinism control FAILED on "
              f"{len(det_fail)}/{N_PHASE1} seeds {det_fail}. The SAME matchup "
              f"(champion vs frozen, same seed) produced different "
              f"trajectories twice. This means a game is NOT reproducible "
              f"from its seed through the worker Pool anymore -- the "
              f"foundation of the ENTIRE paired-seed methodology (diag_utils, "
              f"every McNemar verdict since 2026-07-10) is broken, not just "
              f"this probe. Root-cause the nondeterminism (new engine RNG "
              f"consumer? uninitialized memory? thread?) before trusting ANY "
              f"paired measurement. ***", flush=True)
        sys.exit(1)

    if not diverged:
        print(f"\n{'=' * 70}", flush=True)
        print(f"PHASE 1 VERDICT: IDENTITY HOLDS on all {N_PHASE1} seeds.", flush=True)
        print(f"  champion-vs-frozen is BIT-IDENTICAL to champion-vs-champion", flush=True)
        print(f"  at vf_blend={VF_BLEND}: the frozen checkpoint cannot have played", flush=True)
        print(f"  one action differently. The 2026-07-09 7W/13L asymmetry is", flush=True)
        print(f"  therefore PROVEN NOISE (home-side edge + n=48 sampling), not", flush=True)
        print(f"  a regression. Phase 2 is unnecessary: both its arms would be", flush=True)
        print(f"  the same games. Consistent with the gating null-test finding.", flush=True)
        print(f"{'=' * 70}", flush=True)
        return True

    print(f"\n{'=' * 70}", flush=True)
    print(f"PHASE 1 VERDICT: *** DIVERGENCE -- MAJOR FINDING ***", flush=True)
    print(f"  Trajectories differ from the mirror on {len(diverged)}/{N_PHASE1} "
          f"seeds {diverged}", flush=True)
    print(f"  while the replay control passed (determinism intact). The value", flush=True)
    print(f"  weights DO alter play at vf_blend={VF_BLEND}. This FALSIFIES the", flush=True)
    print(f"  gating null-test finding (diag_null_weights.py, 2026-07-02) at", flush=True)
    print(f"  game level: some path DOES consume the weights file. Locate it", flush=True)
    print(f"  (leaf eval? priors? tie-breaking?) -- and Phase 2 below measures", flush=True)
    print(f"  the real champion-vs-frozen effect with the home seat controlled.", flush=True)
    print(f"{'=' * 70}", flush=True)
    return False


# --- Phase 2: side-swapped paired run -----------------------------------------

def phase2(workers: int, n: int = N_PHASE2) -> None:
    print(f"\n{_now()} === PHASE 2: side-swapped paired run, N={n} seeds "
          f"(x2 games) ===", flush=True)
    print(f"  arm champ-home : home={W} away={FROZEN}", flush=True)
    print(f"  arm frozen-home: home={FROZEN} away={W}", flush=True)
    print(f"  TV={TV} MCTS={MCTS} vf_blend={VF_BLEND}  same seed+race per pair "
          f"-> home-side confound cancels exactly", flush=True)

    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    # Exact _gate_game 9-tuple (run_iteration.py:226-234): (seed, race_idx,
    # gate_path, frozen_path, mcts, vf_blend, tv, leaf_lookahead, policy_path).
    champ_home = [(s, i, W, FROZEN, MCTS, VF_BLEND, TV, False, POLICY)
                  for i, s in enumerate(seeds)]
    frozen_home = [(s, i, FROZEN, W, MCTS, VF_BLEND, TV, False, POLICY)
                   for i, s in enumerate(seeds)]

    res_ch = du.run_arm("champ-home", champ_home, game_fn=_gate_game,
                        workers=workers, mcts_iterations=MCTS)
    du.save_arm(f"weakness_probe_paired_champhome_{STAMP}.json",
                "champ-home", seeds, res_ch)
    res_fh = du.run_arm("frozen-home", frozen_home, game_fn=_gate_game,
                        workers=workers, mcts_iterations=MCTS)
    du.save_arm(f"weakness_probe_paired_frozenhome_{STAMP}.json",
                "frozen-home", seeds, res_fh)

    common = sorted(set(res_ch) & set(res_fh))
    if not common:
        print("*** ABORT (phase 2): no overlapping pairs -- every game was "
              "watchdog-skipped in at least one arm. Systemic engine hang; "
              "no verdict possible. ***", flush=True)
        sys.exit(1)

    # The home seat is constant across arms; the occupant is the treatment.
    # P(home wins | home=champion) - P(home wins | home=frozen) is the
    # champion-minus-frozen strength effect with the home edge removed.
    print(f"\n{_now()} --- paired comparisons (home seat fixed, occupant "
          f"swapped) ---", flush=True)
    print(du.mcnemar_report(res_ch, res_fh, outcome="home_win",
                            label_a="champion@home", label_b="frozen@home"),
          flush=True)
    print(du.mcnemar_report(res_ch, res_fh, outcome="home_loss",
                            label_a="champion@home", label_b="frozen@home"),
          flush=True)
    print(du.mcnemar_report(res_ch, res_fh, outcome="draw",
                            label_a="champion@home", label_b="frozen@home"),
          flush=True)

    # Descriptive: champion W/D/L pooled over both seats, and the home edge.
    cw = cd = cl = 0
    identical = 0
    for i in common:
        h1, a1 = res_ch[i]           # champion is home
        h2, a2 = res_fh[i]           # champion is away
        if (h1, a1) == (h2, a2):
            identical += 1
        for cs, os_ in ((h1, a1), (a2, h2)):     # champion score, opponent score
            if cs > os_:
                cw += 1
            elif cs == os_:
                cd += 1
            else:
                cl += 1
    ng = 2 * len(common)
    hw = sum(res_ch[i][0] > res_ch[i][1] for i in common) \
        + sum(res_fh[i][0] > res_fh[i][1] for i in common)
    aw = sum(res_ch[i][0] < res_ch[i][1] for i in common) \
        + sum(res_fh[i][0] < res_fh[i][1] for i in common)
    print(f"\n  champion pooled over both seats ({ng} games, {len(common)} "
          f"paired seeds): {cw}W {cd}D {cl}L "
          f"= {100 * cw / ng:.1f}%W / {100 * cd / ng:.1f}%D / "
          f"{100 * cl / ng:.1f}%L", flush=True)
    print(f"  home-seat edge pooled over both arms: home {hw} vs away {aw} "
          f"wins of {ng} ({100 * (hw - aw) / ng:+.1f}pp W-L) -- compare to the "
          f"+0.7..+8pp mirror A/A edge that confounded the 07-09 probe",
          flush=True)
    print(f"  identical scorelines across arms: {identical}/{len(common)} "
          f"seeds (high count = weights nearly inert, echoing Phase 1)",
          flush=True)
    print(f"\n  Read INCONCLUSIVE as 'could not tell', never 'no difference'; "
          f"quote the CI. (feedback_draw_rate_noise_floor)", flush=True)


# --- Arm A: champion vs `learning`, event-level metrics -----------------------

def _learning_game(args):
    """Worker: champion (macro_mcts, home) vs `learning` (VF argmax, away,
    same champion weights, epsilon=0). Returns per-game event-level metrics
    computed inside the worker so only a small dict crosses the Pool."""
    seed, race_idx = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], TV)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai="macro_mcts", away_ai="learning",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,   # "same weights, no search"
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    )
    res = lgr.result
    turns = lgr.get_turn_logs()

    id_side: dict[int, str] = {}
    for t in turns:
        for p in t["home_players"]:
            id_side.setdefault(p["id"], "home")
        for p in t["away_players"]:
            id_side.setdefault(p["id"], "away")

    pick_att = {"home": 0, "away": 0}
    pick_ok = {"home": 0, "away": 0}
    hold = {"home": 0, "away": 0}
    depth = {"home": None, "away": None}   # max normalized carry depth
    for t in turns:
        for side, plist in (("home", t["home_players"]),
                            ("away", t["away_players"])):
            for p in plist:
                if p["has_ball"]:
                    hold[side] += 1
                    d = (p["x"] / PITCH_MAX_X if side == "home"
                         else (PITCH_MAX_X - p["x"]) / PITCH_MAX_X)
                    if depth[side] is None or d > depth[side]:
                        depth[side] = d
        for ev in t["events"]:
            if ev["type"] == "PICKUP":
                side = id_side.get(ev["player_id"])
                if side:
                    pick_att[side] += 1
                    pick_ok[side] += 1 if ev["success"] else 0

    return {
        "td_home": res.home_score, "td_away": res.away_score,
        "actions": res.total_actions,
        "pick_att_home": pick_att["home"], "pick_ok_home": pick_ok["home"],
        "pick_att_away": pick_att["away"], "pick_ok_away": pick_ok["away"],
        "hold_home": hold["home"], "hold_away": hold["away"],
        "n_turn_snapshots": len(turns),
        "depth_home": depth["home"], "depth_away": depth["away"],
    }


def arm_a(workers: int, n: int = N_ARMA) -> None:
    print(f"\n{_now()} === ARM A: champion(macro_mcts,{MCTS}it,HOME) vs "
          f"`learning`(VF argmax, same weights, AWAY), n={n} ===", flush=True)
    print(f"  W/D/L vs learning is saturated (0L/48 on 07-09) -> reporting "
          f"event-level offense metrics", flush=True)

    seeds = du.paired_seeds(n, base_seed=BASE_SEED + 1)   # own panel
    tasks = [(s, i) for i, s in enumerate(seeds)]
    res = du.run_arm("armA-learning", tasks, game_fn=_learning_game,
                     workers=workers, mcts_iterations=MCTS)
    if not res:
        print("*** ABORT (arm A): every game watchdog-skipped -- systemic "
              "engine hang vs the `learning` opponent; no metrics possible. "
              "***", flush=True)
        sys.exit(1)

    games = list(res.values())
    m = len(games)
    w = sum(g["td_home"] > g["td_away"] for g in games)
    d = sum(g["td_home"] == g["td_away"] for g in games)
    losses = m - w - d

    def mean(vals):
        vals = [v for v in vals if v is not None]
        return sum(vals) / len(vals) if vals else float("nan")

    pa_h = sum(g["pick_att_home"] for g in games)
    po_h = sum(g["pick_ok_home"] for g in games)
    pa_a = sum(g["pick_att_away"] for g in games)
    po_a = sum(g["pick_ok_away"] for g in games)
    held = [(g["hold_home"], g["hold_away"]) for g in games
            if g["hold_home"] + g["hold_away"] > 0]
    poss = mean([h / (h + a) for h, a in held]) if held else float("nan")
    carried_h = sum(g["depth_home"] is not None for g in games)
    carried_a = sum(g["depth_away"] is not None for g in games)

    print(f"\n{'=' * 70}", flush=True)
    print(f"ARM A (champion HOME vs learning AWAY), n={m}/{n} completed", flush=True)
    print(f"  W/D/L          : {w}W {d}D {losses}L (context only -- saturated "
          f"metric; 07-09 was 18W 30D 0L pre-3a7f208)", flush=True)
    print(f"  TD/game        : champion {mean([g['td_home'] for g in games]):.2f}"
          f"   learning {mean([g['td_away'] for g in games]):.2f}", flush=True)
    print(f"  pickup success : champion {po_h}/{pa_h}"
          f" = {100 * po_h / pa_h if pa_h else float('nan'):.1f}%"
          f"   learning {po_a}/{pa_a}"
          f" = {100 * po_a / pa_a if pa_a else float('nan'):.1f}%", flush=True)
    print(f"  possession     : champion share {100 * poss:.1f}% of held "
          f"turn-snapshots (mean over {len(held)} games with any possession)",
          flush=True)
    print(f"  max carry depth: champion {mean([g['depth_home'] for g in games]):.2f}"
          f" (carried in {carried_h}/{m})   learning "
          f"{mean([g['depth_away'] for g in games]):.2f}"
          f" (carried in {carried_a}/{m})   [1.0 = scoring endzone]", flush=True)
    print(f"  actions/game   : {mean([g['actions'] for g in games]):.0f}", flush=True)
    print(f"{'=' * 70}", flush=True)

    out = f"weakness_probe_armA_learning_{STAMP}.json"
    Path(out).write_text(json.dumps(
        {"seeds": seeds, "games": {str(i): g for i, g in res.items()}}))
    print(f"  per-game metrics saved -> {out}", flush=True)


# --- entry point ---------------------------------------------------------------

def main() -> None:
    mode = sys.argv[1] if len(sys.argv) > 1 else "all"
    if mode not in ("phase1", "phase2", "armA", "all"):
        print(__doc__.split("Usage")[1], flush=True)
        sys.exit(2)
    n_override = int(sys.argv[2]) if len(sys.argv) > 2 else None

    print(f"{_now()} diag_weakness_probe_paired mode={mode}", flush=True)
    _preflight()
    workers = detect_workers()
    print(f"  workers={workers}", flush=True)

    if mode == "phase1":
        phase1(workers)
    elif mode == "phase2":
        phase2(workers, n_override or N_PHASE2)
    elif mode == "armA":
        arm_a(workers, n_override or N_ARMA)
    else:  # all
        identical = phase1(workers)
        if identical:
            print(f"\n{_now()} Phase 2 SKIPPED: identity proven, both arms "
                  f"would replay the same games.", flush=True)
        else:
            phase2(workers, n_override or N_PHASE2)
        arm_a(workers)

    print(f"\n{_now()} done.", flush=True)


if __name__ == "__main__":
    main()
