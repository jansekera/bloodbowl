#!/usr/bin/env python3
"""Shared paired-seed A/B utilities for diag_*.py validation scripts.

Why this exists (2026-07-10 cross-model statistical review, Fable+Opus+Sonnet
independently converging): a single N=150 mirror run pins the draw rate to only
+-8pp at 95%, and the difference between two independent N=150 runs carries a
+-11.3pp confidence interval. Every per-fix delta this project has reported as
"confirmed"/"beze regrese" (+-1-5pp) sat far inside that band. Comparing two
independent proportions is simply the wrong instrument.

The fix is paired-seed (common random number) testing: run the candidate and the
baseline over the SAME seed list, then compare per-seed outcomes with McNemar's
test on the discordant pairs. Shared variance -- race matchup, dice stream,
kickoff -- cancels, so the same number of games buys far more power.

This module owns only the three pieces that are error-prone and worth writing
once:

  paired_seeds()   reproducible seed list, identical across both arms (today's
                   scripts call random.randint, so no run can be reproduced)
  run_arm()        one arm through the EXISTING run_iteration Pool/_imap_watchdog
                   machinery, keyed by pair index -- _imap_watchdog silently
                   drops timed-out games from its yield stream, so without an
                   index tag a single watchdog skip would misalign every
                   subsequent pair between the arms
  mcnemar()        paired stats: delta, SE, z, 95% CI, exact binomial p, and a
                   CONFIRMED / INCONCLUSIVE verdict

plus save_arm()/load_arm(), because most of this project's fixes are C++ changes
and can only be A/B'd across a rebuild: run the baseline arm, rebuild with the
fix, run the candidate arm, compare from the saved JSON.

Each diag script still builds its own task tuples. This module never hides which
weights, MCTS budget or blend values are under test -- that stays visible and
auditable in the script itself.

Read INCONCLUSIVE as "this experiment could not tell", never as "no regression":
failing to reject zero is absence of evidence. For a no-regression claim, quote
the confidence interval and judge whether the whole interval is tolerable.

Usage (run from repo root, like every diag script):

    import sys
    import diag_utils as du

    N = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    seeds = du.paired_seeds(N, base_seed=20260710)   # convention: launch date

    # exact _gate_game 9-tuple; only the weights path differs between arms,
    # seed and race_idx (i) are identical within each pair
    cand = [(s, i, "weights_cand.json", "weights_cand.json", 100, 0.0,
             1000, False, "weights_policy.json") for i, s in enumerate(seeds)]
    base = [(s, i, "weights_best.json", "weights_best.json", 100, 0.0,
             1000, False, "weights_policy.json") for i, s in enumerate(seeds)]

    print(du.mcnemar_report(du.run_arm("candidate", cand),
                            du.run_arm("baseline", base), outcome="draw"))
"""
from __future__ import annotations

import json
import math
import random
import sys
from multiprocessing import Pool
from pathlib import Path

# Same path convention as every existing diag_*.py (must run from repo root).
sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")
from run_iteration import (  # noqa: E402
    _benchmark_game,
    _gate_game,
    _imap_watchdog,
    _pool_init,
)

INIT_ARGS = ("engine/build", "python")   # _pool_init(engine_build, python_src)
DEFAULT_WORKERS = 12                     # house standard in diag scripts
Z95 = 1.959963984540054

# Outcome predicates over a single game result.
# _gate_game returns (home_score, away_score); _benchmark_game returns bool.
OUTCOMES = {
    "draw":      lambda r: r[0] == r[1],
    "home_win":  lambda r: r[0] > r[1],
    "home_loss": lambda r: r[0] < r[1],
    "win":       lambda r: bool(r),      # for _benchmark_game bool results
}


def paired_seeds(n: int, base_seed: int) -> list[int]:
    """Reproducible list of n distinct seeds in [1, 999999] (house range).

    Both arms of a comparison must use this same list, zipped with the same
    enumerate() index as race_idx, so pair i is the identical matchup and dice
    stream in both arms. Re-running a script with the same base_seed reproduces
    the experiment exactly, unlike the per-script random.randint pattern.

    Convention: base_seed = launch date (YYYYMMDD), +1/+2/... for deliberate
    replications on the same day. Rotating it across experiments keeps any one
    150-seed panel from silently becoming the thing every verdict is
    conditioned on.
    """
    if n > 999_999:
        raise ValueError("n exceeds seed space")
    return random.Random(base_seed).sample(range(1, 1_000_000), n)


def _tagged_game(args):
    """Pool worker: run one game, echoing back its pair index.

    _imap_watchdog drops timed-out games from its yield stream, so without the
    index tag a single watchdog skip would misalign every subsequent pair
    between the arms. Tagging makes a skip harmless: the index is simply absent
    from the dict, and mcnemar() intersects the two arms' indices.
    """
    idx, game_fn, inner = args
    return idx, game_fn(inner)


def run_arm(label: str, tasks: list[tuple], game_fn=_gate_game,
            workers: int = DEFAULT_WORKERS, mcts_iterations: int = 100,
            progress_every: int = 10) -> dict[int, object]:
    """Run one arm's task list through the production Pool/watchdog pattern.

    tasks[i] must be a full task tuple for game_fn -- the exact tuples the
    existing diag scripts already build for _gate_game/_benchmark_game.
    Returns {pair_index: result}; indices missing from the dict were
    watchdog-skipped (already logged by _imap_watchdog itself).
    """
    n = len(tasks)
    tagged = [(i, game_fn, t) for i, t in enumerate(tasks)]
    results: dict[int, object] = {}
    w = d = l = 0
    print(f"\n--- arm {label!r}: n={n} workers={workers} "
          f"mcts={mcts_iterations} ---", flush=True)
    with Pool(workers, initializer=_pool_init, initargs=INIT_ARGS) as pool:
        for idx, res in _imap_watchdog(pool, _tagged_game, tagged, label,
                                       mcts_iterations=mcts_iterations):
            results[idx] = res
            # gate results: 2-tuple (hs, as) legacy, 3-tuple (cs, fs, ca)
            # side-swap (2026-07-14) -- both are candidate/home-first scores
            if isinstance(res, tuple) and len(res) in (2, 3):
                if res[0] > res[1]:
                    w += 1
                elif res[0] == res[1]:
                    d += 1
                else:
                    l += 1
            done = len(results)
            if done % progress_every == 0 or done == n:
                tail = (f" -- {w}W {d}D {l}L = {100 * d / done:.1f}% draws"
                        if (w + d + l) == done else "")
                print(f"  [{label}] {done}/{n} done{tail}", flush=True)
    if len(results) < n:
        print(f"  [{label}] {n - len(results)}/{n} watchdog-skipped; their "
              f"pairs will be dropped from BOTH arms.", flush=True)
    return results


def save_arm(path: str | Path, label: str, seeds: list[int],
             results: dict[int, object]) -> None:
    """Persist one arm so the other can run after a rebuild (C++ A/B).

    Record the git SHA alongside this file: a saved baseline is only reusable
    while the binary and config it was measured under are unchanged.
    """
    Path(path).write_text(json.dumps({
        "label": label,
        "seeds": seeds,
        "results": {str(i): r for i, r in results.items()},
    }))
    print(f"  [{label}] saved {len(results)} results -> {path}", flush=True)


def load_arm(path: str | Path) -> tuple[str, list[int], dict[int, object]]:
    """Inverse of save_arm; gate results come back as (hs, as) tuples."""
    d = json.loads(Path(path).read_text())
    results = {int(i): (tuple(r) if isinstance(r, list) else r)
               for i, r in d["results"].items()}
    return d["label"], d["seeds"], results


def mcnemar(res_a: dict[int, object], res_b: dict[int, object],
            outcome="draw") -> dict:
    """Paired (McNemar-style) comparison of one binary outcome across arms.

    res_a is the candidate, res_b the baseline; both come from run_arm() or
    load_arm(). Only indices present in BOTH arms are compared. outcome is a key
    into OUTCOMES or a callable(result) -> bool.

    Returns n_pairs, n_dropped, per-arm rates, the discordant counts n10/n01,
    delta = rate_a - rate_b, its paired SE and Wald 95% CI, McNemar z, the exact
    two-sided binomial p on the discordant pairs, and a verdict: CONFIRMED iff
    the 95% CI excludes zero, else INCONCLUSIVE.
    """
    f = OUTCOMES[outcome] if isinstance(outcome, str) else outcome
    common = sorted(set(res_a) & set(res_b))
    n = len(common)
    if n == 0:
        raise ValueError("no overlapping pairs between arms")
    n_dropped = max(len(res_a), len(res_b)) - n
    n11 = n10 = n01 = n00 = 0
    for i in common:
        a, b = bool(f(res_a[i])), bool(f(res_b[i]))
        if a and b:
            n11 += 1
        elif a:
            n10 += 1
        elif b:
            n01 += 1
        else:
            n00 += 1
    nd = n10 + n01
    delta = (n10 - n01) / n
    # Paired difference-of-proportions SE: the shared variance cancels, which is
    # the entire point of running both arms on one seed list.
    se = math.sqrt(max(0.0, nd - (n10 - n01) ** 2 / n)) / n
    z = (n10 - n01) / math.sqrt(nd) if nd else 0.0
    ci_lo, ci_hi = delta - Z95 * se, delta + Z95 * se
    # Exact two-sided binomial test on the discordant pairs, Bin(nd, 1/2).
    if nd:
        k = min(n10, n01)
        tail = sum(math.comb(nd, j) for j in range(k + 1)) / 2 ** nd
        p_exact = min(1.0, 2.0 * tail)
    else:
        p_exact = 1.0
    verdict = "CONFIRMED" if nd and (ci_lo > 0 or ci_hi < 0) else "INCONCLUSIVE"
    return {
        "n_pairs": n, "n_dropped": n_dropped,
        "rate_a": (n11 + n10) / n, "rate_b": (n11 + n01) / n,
        "n11": n11, "n10": n10, "n01": n01, "n00": n00,
        "delta": delta, "se": se, "ci_lo": ci_lo, "ci_hi": ci_hi,
        "z": z, "p_exact": p_exact, "verdict": verdict,
    }


def mcnemar_report(res_a, res_b, outcome="draw",
                   label_a="candidate", label_b="baseline") -> str:
    """Human-readable multi-line report for the standard diag printout."""
    name = outcome if isinstance(outcome, str) else getattr(
        outcome, "__name__", "outcome")
    m = mcnemar(res_a, res_b, outcome)
    lines = [
        f"=== PAIRED A/B ({name})  {label_a} vs {label_b}  "
        f"n={m['n_pairs']} pairs"
        + (f" ({m['n_dropped']} dropped to watchdog skips)"
           if m["n_dropped"] else "") + " ===",
        f"  {name} rate: {label_a} {100 * m['rate_a']:.1f}%  vs  "
        f"{label_b} {100 * m['rate_b']:.1f}%",
        f"  discordant pairs: {m['n10']} ({label_a}-only) vs "
        f"{m['n01']} ({label_b}-only)  of {m['n_pairs']}",
        f"  delta = {100 * m['delta']:+.1f}pp   SE = {100 * m['se']:.1f}pp   "
        f"95% CI [{100 * m['ci_lo']:+.1f}, {100 * m['ci_hi']:+.1f}]pp",
        f"  McNemar z = {m['z']:+.2f}   exact p = {m['p_exact']:.4f}",
        f"  VERDICT: {m['verdict']}"
        + (f" -- {label_a} {name} rate "
           f"{'HIGHER' if m['delta'] > 0 else 'LOWER'} than {label_b}"
           if m["verdict"] == "CONFIRMED" else
           " -- CI includes 0; NOT evidence of no effect, see CI width"),
    ]
    if 0 < m["n10"] + m["n01"] < 10:
        lines.append(
            f"  NOTE: only {m['n10'] + m['n01']} discordant pairs -- the "
            f"normal-approx CI is shaky at this size; weigh the exact p above.")
    return "\n".join(lines)


if __name__ == "__main__":
    # Self-test: pure stats math on synthetic results, no engine involved.
    # Case 1: 100 pairs, candidate draws on 20 pairs the baseline doesn't and
    # the baseline on 5 the candidate doesn't -> delta=+15pp, z=3.0, CONFIRMED.
    A = {i: (1, 1) for i in range(25)}            # 25 draws total in A
    A.update({i: (2, 0) for i in range(25, 100)})
    B = {i: (1, 1) for i in range(5)}             # concordant draws 0-4
    B.update({i: (2, 0) for i in range(5, 25)})   # A-only draws 5-24 (n10=20)
    B.update({i: (1, 1) for i in range(25, 30)})  # B-only draws 25-29 (n01=5)
    B.update({i: (2, 0) for i in range(30, 100)})
    m = mcnemar(A, B, "draw")
    assert m["n10"] == 20 and m["n01"] == 5 and m["n_pairs"] == 100
    assert abs(m["delta"] - 0.15) < 1e-12
    assert abs(m["z"] - 3.0) < 1e-12
    assert abs(m["se"] - math.sqrt(25 - 225 / 100) / 100) < 1e-12
    assert m["verdict"] == "CONFIRMED" and m["p_exact"] < 0.01
    # Case 2: perfectly balanced discordants -> INCONCLUSIVE, delta=0.
    B2 = dict(B)
    B2.update({i: (1, 1) for i in range(15, 25)})   # concordant: n10 20->10
    B2.update({i: (1, 1) for i in range(30, 35)})   # grow n01 5->10
    m2 = mcnemar(A, B2, "draw")
    assert m2["n10"] == m2["n01"] == 10
    assert m2["delta"] == 0.0 and m2["verdict"] == "INCONCLUSIVE"
    # Case 3: a watchdog skip drops the pair from both arms.
    A3 = dict(A)
    del A3[7]
    m3 = mcnemar(A3, B, "draw")
    assert m3["n_pairs"] == 99 and m3["n_dropped"] == 1
    # Case 4: seed list reproducibility, house range, task-tuple plumbing.
    s1, s2 = paired_seeds(150, 20260710), paired_seeds(150, 20260710)
    assert s1 == s2 and len(set(s1)) == 150
    assert all(1 <= s <= 999_999 for s in s1)
    assert paired_seeds(150, 20260711) != s1
    idx, res = _tagged_game((3, lambda t: (t[0] % 7, t[1]), (s1[3], 3)))
    assert idx == 3 and res == (s1[3] % 7, 3)
    # Case 5: save/load round-trip preserves tuples and indices.
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".json") as tf:
        save_arm(tf.name, "t", s1[:3], {0: (2, 1), 2: (0, 0)})
        lbl, seeds, res = load_arm(tf.name)
    assert lbl == "t" and seeds == s1[:3] and res == {0: (2, 1), 2: (0, 0)}
    print(mcnemar_report(A, B, "draw"))
    print(mcnemar_report(A, B2, "draw"))
    print("\ndiag_utils self-test: ALL PASS")
