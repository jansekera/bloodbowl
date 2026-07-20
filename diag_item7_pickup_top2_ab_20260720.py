#!/usr/bin/env python3
"""Paired-seed A/B: item 7 (top-2 PICKUP pickers,
proposals_item7_pickup_top2_20260714.md), applied 2026-07-20.

"off" = HEAD (0f70dab, item10 applied, item7 not yet) -- git worktree,
reused/rebuilt from the item10 baseline. "on" = this tree's engine/build
(item7 applied on top, 428/428 tests green).

Per section 5.3, primary metric is per-turn ground recovery (mining, not
directly available from decision logs -- approximated here by the
decision-level proxy "PICKUP chosen when a loose-ball decision exists").
Also tracks the over-crowding tripwire (PICKUP family visit-mass on
loose-ball nodes, red line >0.60) and REPOSITION+BLITZ share (collateral
dilution check). Draw-rate is a tripwire only, not the target metric.

Usage: python3 diag_item7_pickup_top2_ab_20260720.py <off|on>|compare
"""
import sys
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1200, 100
BASE_SEED = 20260720
WORKERS = 10
N = 150

ARMS = {
    "off": dict(engine="../bloodbowl_item7_off/engine/build", n=N),
    "on":  dict(engine="engine/build", n=N),
}
ORDER = ["off", "on"]


def path_for(arm: str) -> str:
    return f"arm_item7_ab_{arm}_20260720.json"


def counters_path_for(arm: str) -> str:
    return f"arm_item7_counters_{arm}_20260720.json"


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
    c = dict(loose_dec=0, loose_pickup_chosen=0, loose_pickup_visitmass=0.0,
             loose_repo_blitz_chosen=0, def_dec=0, def_engage=0)
    for d in lgr.get_policy_decisions():
        v = d["visits"]
        if not v:
            continue
        top = v[0]["action_features"]
        f = d["state_features"]
        if f[14] > 0.5:  # loose ball
            c["loose_dec"] += 1
            if top[5] > 0.5:
                c["loose_pickup_chosen"] += 1
            if top[8] > 0.5 or top[3] > 0.5:
                c["loose_repo_blitz_chosen"] += 1
            pickup_mass = sum(vv["visit_fraction"] for vv in v
                              if vv["action_features"][5] > 0.5)
            c["loose_pickup_visitmass"] += pickup_mass
        if f[13] > 0.5:  # defensive (unaffected by item7 -- attribution guard)
            c["def_dec"] += 1
            if top[3] > 0.5 or top[4] > 0.5:
                c["def_engage"] += 1
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
    print(f"=== item7 top-2-pickup A/B: arm={arm} N={a['n']} base_seed={BASE_SEED} "
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
    tot = dict(loose_dec=0, loose_pickup_chosen=0, loose_pickup_visitmass=0.0,
               loose_repo_blitz_chosen=0, def_dec=0, def_engage=0)
    for c in counters.values():
        for k in tot:
            tot[k] += c.get(k, 0)
    return tot


def _rate(num, den):
    return num / den if den else float("nan")


def compare() -> None:
    import json, math
    data = {}
    for arm in ORDER:
        p = Path(counters_path_for(arm))
        if not p.exists():
            continue
        with open(p) as f:
            data[arm] = json.load(f)

    print(f"\n{'=' * 16} COMPARE (item7) {'=' * 16}\n")
    for arm in ORDER:
        if arm not in data:
            continue
        counters = {int(k): v for k, v in data[arm]["counters"].items()}
        agg = _agg(counters)
        print(f"[{arm}] loose_dec={agg['loose_dec']} "
              f"pickup_chosen_rate={_rate(agg['loose_pickup_chosen'], agg['loose_dec']):.3%} "
              f"pickup_visitmass_rate={_rate(agg['loose_pickup_visitmass'], agg['loose_dec']):.3%} "
              f"repo_blitz_chosen_rate={_rate(agg['loose_repo_blitz_chosen'], agg['loose_dec']):.3%} "
              f"def_engage_rate={_rate(agg['def_engage'], agg['def_dec']):.3%}")

    if "off" in data and "on" in data:
        c_off = {int(k): v for k, v in data["off"]["counters"].items()}
        c_on = {int(k): v for k, v in data["on"]["counters"].items()}
        common = sorted(set(c_off) & set(c_on))
        print(f"\npaired games: {len(common)}")

        def paired_rate_delta(metric_num, metric_den):
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
                       ci_lo=mean_diff - 1.96 * se, ci_hi=mean_diff + 1.96 * se)

        print("\n--- primary proxy: PICKUP chosen rate on loose-ball decisions ---")
        r = paired_rate_delta("loose_pickup_chosen", "loose_dec")
        if r:
            print(f"  off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}] n={r['n']}")

        print("\n--- over-crowding tripwire: PICKUP family visit-mass on loose-ball nodes ---")
        r = paired_rate_delta("loose_pickup_visitmass", "loose_dec")
        if r:
            print(f"  off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}]")
            if r['on'] > 0.60:
                print("  *** RED: PICKUP family visit-mass >0.60 -> over-crowding, search pushed out ***")
            else:
                print("  OK: below 0.60 tripwire")

        print("\n--- collateral: REPOSITION+BLITZ chosen rate on loose-ball nodes ---")
        r = paired_rate_delta("loose_repo_blitz_chosen", "loose_dec")
        if r:
            print(f"  off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}]")

        print("\n--- attribution guard: defensive engagement (item7 shouldn't touch onDef nodes) ---")
        r = paired_rate_delta("def_engage", "def_dec")
        if r:
            print(f"  off={r['off']:.3%} on={r['on']:.3%} delta={r['delta']:+.3%} "
                  f"CI[{r['ci_lo']:+.3%},{r['ci_hi']:+.3%}] (expect ~0)")

    arms = {}
    for a in ORDER:
        p = Path(path_for(a))
        if p.exists():
            arms[a] = du.load_arm(p)
    if "off" in arms and "on" in arms:
        print("\n--- draw-rate tripwire ---")
        _, s_off, r_off = arms["off"]
        _, s_on, r_on = arms["on"]
        for outcome in ("draw", "home_win"):
            print(du.mcnemar_report(r_on, r_off, outcome, label_a="item7_on", label_b="item7_off"))
            print()
        n_off, n_on = len(r_off), len(r_on)
        print(f"[off] n={n_off} watchdog-skipped={ARMS['off']['n']-n_off}")
        print(f"[on]  n={n_on} watchdog-skipped={ARMS['on']['n']-n_on}")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "compare":
        compare()
    elif mode in ARMS:
        run(mode)
    else:
        sys.exit(__doc__)
