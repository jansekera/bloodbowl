#!/usr/bin/env python3
"""Paired-seed A/B: item 10 defensive prior-floor rebalance
(proposals_item10_prior_floor_validation_20260714.md), applied 2026-07-20.

REPOSITION floor 0.05->0.08 (onDef), new FOUL cap 0.08 (onDef). "off" = HEAD
(a5dd758, pre-patch, git worktree at ../bloodbowl_scoreavail_off/engine/build
-- reused unchanged from today's SCORE-availability measurement, still clean).
"on" = this tree's engine/build (item10 applied, 424/424 tests green).

Counter-worker per proposal section 3.1: decision-level attribution split by
onDef (f13, opponent has ball) and node size (len(visits)>=13 proxy for
n>=13, the floor-binding threshold). C3 (engagement guard: BLITZ+BLOCK share
of defensive decisions) is the RED-LINE veto per the decision matrix in
section 3.2 -- a >15% relative drop with CI excluding zero means REVERT
regardless of every other metric, since REPOSITION is the only dice-free/
turnover-free macro and more prior mass there could feed passivity instead
of screens.

Usage: python3 diag_item10_prior_floor_ab_20260720.py <off|on>|compare
"""
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"  # MANDATORY -- floor/cap regime only activates with a policy net loaded
TV, MCTS = 1200, 100
BASE_SEED = 20260720
WORKERS = 10
N = 150

ARMS = {
    "off": dict(engine="../bloodbowl_scoreavail_off/engine/build", n=N),
    "on":  dict(engine="engine/build", n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_item10_ab_{arm}_20260720.json"


def counters_path_for(arm: str) -> str:
    return f"arm_item10_counters_{arm}_20260720.json"


def _gate_game_counters(args):
    (seed, race_idx, gate_path, frozen_path, mcts_iterations,
     vf_blend, tv, leaf_lookahead, policy_path, engine_path) = args
    import sys as _sys
    _sys.path.insert(0, engine_path)
    import bb_engine
    races = ["human", "orc", "skaven", "dwarf", "wood-elf"]
    hr = bb_engine.get_developed_roster(races[race_idx % 5], tv)
    ar = bb_engine.get_developed_roster(races[(race_idx + 1) % 5], tv)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, away_weights_path=frozen_path,
        epsilon=0.0, vf_blend=vf_blend,
        policy_weights_path=policy_path,
    )
    c = dict(def_dec=0, def_engage=0, def_foul=0, def_big=0, def_big_repo=0,
             def_small=0, def_small_repo=0, loose_dec=0, loose_foul=0)
    for d in lgr.get_policy_decisions():
        v = d["visits"]
        if not v:
            continue
        top = v[0]["action_features"]  # visits sorted descending -> [0] = executed
        f = d["state_features"]
        if f[13] > 0.5:  # defensive decision (opponent holds ball)
            c["def_dec"] += 1
            if top[3] > 0.5 or top[4] > 0.5:
                c["def_engage"] += 1
            if top[7] > 0.5:
                c["def_foul"] += 1
            big = len(v) >= 13
            c["def_big" if big else "def_small"] += 1
            if top[8] > 0.5:
                c["def_big_repo" if big else "def_small_repo"] += 1
        elif f[14] > 0.5:  # loose ball
            c["loose_dec"] += 1
            if top[7] > 0.5:
                c["loose_foul"] += 1
    r = lgr.result
    return (r.home_score, r.away_score, c)


def _tagged(args):
    idx, inner = args
    return idx, _gate_game_counters(inner)


def tasks_for(seeds, engine_path):
    return [(s, i, W, W, MCTS, 0.0, TV, False, POLICY_PATH, engine_path)
            for i, s in enumerate(seeds)]


def run(arm: str) -> None:
    a = ARMS[arm]
    seeds = du.paired_seeds(a["n"], base_seed=BASE_SEED)
    tasks = [(i, t) for i, t in enumerate(tasks_for(seeds, a["engine"]))]
    print(f"=== item10 prior-floor A/B: arm={arm} N={a['n']} base_seed={BASE_SEED} "
          f"MCTS={MCTS} TV={TV} workers={WORKERS} engine={a['engine']} ===", flush=True)

    from multiprocessing import Pool
    import time
    t0 = time.time()
    results = {}
    with Pool(WORKERS) as pool:
        done = 0
        for idx, res in pool.imap_unordered(_tagged, tasks):
            results[idx] = res
            done += 1
            if done % 10 == 0 or done == len(tasks):
                print(f"  [{arm}] {done}/{len(tasks)} done ({time.time()-t0:.0f}s)", flush=True)

    import json
    scores = {i: (r[0], r[1]) for i, r in results.items()}
    counters = {i: r[2] for i, r in results.items()}
    du.save_arm(path_for(arm), arm, seeds, scores)
    with open(counters_path_for(arm), "w") as f:
        json.dump({"seeds": seeds, "counters": counters}, f)
    print(f"[{arm}] DONE {len(results)}/{len(tasks)} in {time.time()-t0:.0f}s", flush=True)


def _agg(counters: dict) -> dict:
    tot = dict(def_dec=0, def_engage=0, def_foul=0, def_big=0, def_big_repo=0,
               def_small=0, def_small_repo=0, loose_dec=0, loose_foul=0)
    for c in counters.values():
        for k in tot:
            tot[k] += c.get(k, 0)
    return tot


def _rate(num, den):
    return num / den if den else float("nan")


def compare() -> None:
    import json
    data = {}
    for arm in ORDER:
        p = Path(counters_path_for(arm))
        if not p.exists():
            continue
        with open(p) as f:
            data[arm] = json.load(f)

    print(f"\n{'=' * 16} COMPARE (item10) {'=' * 16}\n")
    for arm in ORDER:
        if arm not in data:
            continue
        seeds, counters = data[arm]["seeds"], {int(k): v for k, v in data[arm]["counters"].items()}
        agg = _agg(counters)
        print(f"[{arm}] def_dec={agg['def_dec']} def_engage_rate={_rate(agg['def_engage'], agg['def_dec']):.3%} "
              f"def_foul_rate={_rate(agg['def_foul'], agg['def_dec']):.3%} "
              f"def_big={agg['def_big']} def_big_repo_rate={_rate(agg['def_big_repo'], agg['def_big']):.3%} "
              f"def_small={agg['def_small']} def_small_repo_rate={_rate(agg['def_small_repo'], agg['def_small']):.3%} "
              f"loose_dec={agg['loose_dec']} loose_foul_rate={_rate(agg['loose_foul'], agg['loose_dec']):.3%}")

    if "off" in data and "on" in data:
        seeds_off = data["off"]["seeds"]
        seeds_on = data["on"]["seeds"]
        assert seeds_off == seeds_on, "seed lists differ -- not paired"
        c_off = {int(k): v for k, v in data["off"]["counters"].items()}
        c_on = {int(k): v for k, v in data["on"]["counters"].items()}
        common = sorted(set(c_off) & set(c_on))
        print(f"\npaired games: {len(common)}")

        def paired_rate_delta(metric_num, metric_den):
            import math
            per_pair_off, per_pair_on = [], []
            for i in common:
                o, n = c_off[i], c_on[i]
                do, no = o.get(metric_den, 0), n.get(metric_den, 0)
                if do == 0 or no == 0:
                    continue
                per_pair_off.append(o.get(metric_num, 0) / do)
                per_pair_on.append(n.get(metric_num, 0) / no)
            if not per_pair_off:
                return None
            mean_off = sum(per_pair_off) / len(per_pair_off)
            mean_on = sum(per_pair_on) / len(per_pair_on)
            diffs = [a - b for a, b in zip(per_pair_on, per_pair_off)]
            mean_diff = sum(diffs) / len(diffs)
            var = sum((d - mean_diff) ** 2 for d in diffs) / max(len(diffs) - 1, 1)
            se = math.sqrt(var / len(diffs))
            return dict(n=len(diffs), off=mean_off, on=mean_on, delta=mean_diff, se=se,
                       ci_lo=mean_diff - 1.96 * se, ci_hi=mean_diff + 1.96 * se,
                       rel_pct=(mean_diff / mean_off * 100 if mean_off else float("nan")))

        print("\n--- C3 engagement guard (RED LINE) ---")
        r = paired_rate_delta("def_engage", "def_dec")
        if r:
            print(f"def_engage_rate: off={r['off']:.3%} on={r['on']:.3%} "
                  f"delta={r['delta']:+.3%} rel={r['rel_pct']:+.1f}% "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}] n={r['n']}")
            if r['rel_pct'] < -15 and r['ci_hi'] < 0:
                print("  *** RED: engagement drop >15% with CI excluding 0 -> REVERT signal ***")
            elif r['rel_pct'] < -10:
                print("  ~ borderline: relative drop >10%, check CI width / consider N=300")
            else:
                print("  OK: no red-line engagement drop")

        print("\n--- C2 manipulation check: REPOSITION share by node size ---")
        for label, num, den in [("big (n>=13)", "def_big_repo", "def_big"),
                                ("small (n<=12)", "def_small_repo", "def_small")]:
            r = paired_rate_delta(num, den)
            if r:
                print(f"  {label}: off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                      f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}]")

        print("\n--- C5 FOUL usage decomposition ---")
        r = paired_rate_delta("def_foul", "def_dec")
        if r:
            print(f"  def_foul_rate: off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}]")
        r = paired_rate_delta("loose_foul", "loose_dec")
        if r:
            print(f"  loose_foul_rate (null check, expect ~0 delta): off={r['off']:.3%} on={r['on']:.3%} "
                  f"delta={r['delta']:+.3%} CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}]")

    # draw-rate tripwire via the standard score-based arms
    arms = {}
    for a in ORDER:
        p = Path(path_for(a))
        if p.exists():
            arms[a] = du.load_arm(p)
    if "off" in arms and "on" in arms:
        print("\n--- C6 draw-rate tripwire ---")
        _, s_off, r_off = arms["off"]
        _, s_on, r_on = arms["on"]
        for outcome in ("draw", "home_win"):
            print(du.mcnemar_report(r_on, r_off, outcome, label_a="item10_on", label_b="item10_off"))
            print()

        w = d = l = 0
        for r in r_off.values():
            w += r[0] > r[1]; d += r[0] == r[1]; l += r[0] < r[1]
        n_off = len(r_off)
        print(f"[off] {w}W {d}D {l}L  n={n_off} watchdog-skipped={ARMS['off']['n']-n_off}")
        w = d = l = 0
        for r in r_on.values():
            w += r[0] > r[1]; d += r[0] == r[1]; l += r[0] < r[1]
        n_on = len(r_on)
        print(f"[on]  {w}W {d}D {l}L  n={n_on} watchdog-skipped={ARMS['on']['n']-n_on}")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
