"""Cheap offline MCTS-iteration-budget probe (2026-07-21).

Queued since [[project_bloodbowl_day_20260720]] as the next live hypothesis
for the diagnosed policy-net plateau (evidence/fable_policy_learning_diagnosis_20260717.md):
does more MCTS search budget on the CURRENT frozen net sharpen the visit-count
distribution (lower normalized entropy) that the policy head is trying to
imitate? No retraining -- pure inference-time comparison, paired seeds,
current weights_best.json/weights_policy.json held fixed.

Entropy formula matches training_loop.py's own `mcts_visit_entropy` metric
exactly: mean over decisions of -sum(p*log(p))/log(n).

Low parallelism (Pool(2)) and a small paired sample -- deliberately cheap,
and the overnight `--loop 4` training is still consuming most cores.

Usage:
    python3 diag_mcts_budget_probe_20260721.py [n_games=10]
"""
import sys
import time

import numpy as np

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND = 1200, 0.0
BASE_SEED = 30260721  # NOTE: keep well under int32 max (~2.15e9) -- a
# too-large seed here previously caused a confusing pybind "incompatible
# function arguments" TypeError on simulate_game_logged() that looked like
# a multiprocessing/module-identity bug but wasn't (2026-07-21).
ARMS = [100, 400]  # production baseline vs 4x search budget


def _entropy(decisions):
    ents = []
    for d in decisions:
        fr = [v["visit_fraction"] for v in d.get("visits", [])]
        n = len(fr)
        if n < 2:
            continue
        p = np.array(fr, dtype=np.float64)
        s = p.sum()
        if s <= 0:
            continue
        p = p[p > 0] / s
        ents.append(float(-np.sum(p * np.log(p)) / np.log(n)))
    return ents


def _game_worker(args: tuple) -> dict:
    seed, race_idx, mcts_iters = args
    import bb_engine
    ra = RACES[race_idx % len(RACES)]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=mcts_iters,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
    )
    decisions = lgr.get_policy_decisions()
    ents = _entropy(decisions)
    return {"seed": seed, "mcts_iters": mcts_iters, "n_dec": len(decisions),
            "ents": ents}


def main(n_games: int) -> None:
    # Sequential, not Pool -- multiprocessing.Pool hit a reproducible pybind
    # TypeError on simulate_game_logged() here (2026-07-21) that a direct
    # single-process call did not; not worth chasing further for a small,
    # cheap probe. Also: games are ~50s+ each under today's system load
    # (training loop consuming most cores), so parallelism gain is limited
    # anyway.
    tasks = []
    for i in range(n_games):
        for mcts_iters in ARMS:
            tasks.append((BASE_SEED + i, i % len(RACES), mcts_iters))

    t0 = time.time()
    results = {a: [] for a in ARMS}
    for idx, task in enumerate(tasks):
        r = _game_worker(task)
        results[r["mcts_iters"]].extend(r["ents"])
        print(f"[{idx+1}/{len(tasks)}] seed={r['seed']} mcts={r['mcts_iters']} "
              f"n_dec={r['n_dec']} ({time.time()-t0:.0f}s)", flush=True)

    print("\n=== RESULT ===")
    for a in ARMS:
        arr = np.array(results[a])
        se = arr.std(ddof=1) / np.sqrt(len(arr)) if len(arr) > 1 else float("nan")
        print(f"mcts_iterations={a}: n_decisions={len(arr)} "
              f"mean_norm_entropy={arr.mean():.4f} +/- {1.96*se:.4f} (95% CI)")

    if len(ARMS) == 2:
        a0, a1 = results[ARMS[0]], results[ARMS[1]]
        diff = np.mean(a1) - np.mean(a0)
        se = np.sqrt(np.var(a0, ddof=1)/len(a0) + np.var(a1, ddof=1)/len(a1))
        print(f"\ndelta ({ARMS[1]} - {ARMS[0]}) = {diff:.4f} +/- {1.96*se:.4f} (95% CI)")


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    main(n)
