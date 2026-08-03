"""F0 (2026-08-03): reusable fairtest scheduling + paired per-race evaluation.

Fixes the two methodology gaps documented in
evidence/fable_dwarf_playstyle_gap_20260803.md §3a/§3b:

1. SCHEDULE. The historic period-5 cycle
       ra = RACES[i % 5]; rb = RACES[(i + 1) % 5]
   (diag_policy_confirm_20260731.py:48-49, also run_iteration.py gate/benchmark)
   only ever plays 5 of the 25 ordered race pairs -- dwarf meets EXCLUSIVELY
   skaven and wood-elf, and the dwarf mirror is never played at all. Per-race
   numbers from such a schedule conflate the tested change with matchup
   strength. `matchup_for_seed` replaces it with a full round-robin INCLUDING
   mirrors: every block of len(races)**2 consecutive seed indices covers every
   ordered pair exactly once, so each race appears equally often, on both
   home and away orientation, against every race including itself.
   Deterministic in seed_idx alone -- drop-in for the old two-liner.

2. EVALUATION. Per-race results must be read PAIRED WITHIN-MATCHUP against
   the baseline playing the same race (dwarf report §3b), never against a
   global 50% threshold. With the standard side-swapped pair protocol
   (same seed_idx played twice, candidate home then candidate away), each
   pair yields for BOTH races of the matchup one candidate-as-X game and one
   baseline-as-X game -- a within-seed paired sample. `per_race_paired`
   computes, per race and per opposing race: candidate-as-X vs baseline-as-X
   decisive win rates, their delta, and a paired t on chess scores
   (W=1 / D=0.5 / L=0).

Row contract (matches diag_policy_confirm_20260731.py's game() output):
    {"seed_idx": int, "cand_home": bool, "race_h": str, "race_a": str,
     "cand": int, "base": int}
where cand/base are the final scores from the candidate's/baseline's side.
"""
from __future__ import annotations

import math
from collections import defaultdict

DEFAULT_RACES = ("human", "orc", "skaven", "dwarf", "wood-elf")


# ── schedule ────────────────────────────────────────────────────────────────

def matchup_for_seed(seed_idx, races=DEFAULT_RACES):
    """Deterministic full round-robin incl. mirrors.

    Returns (home_race, away_race) for this seed index. Every window of
    len(races)**2 consecutive indices contains each ordered pair exactly
    once. Replaces `ra = RACES[i % 5]; rb = RACES[(i + 1) % 5]`.
    """
    if not races:
        raise ValueError("races must be non-empty")
    if seed_idx < 0:
        raise ValueError("seed_idx must be >= 0")
    n = len(races)
    k = seed_idx % (n * n)
    return races[k // n], races[k % n]


def schedule(n_seeds, races=DEFAULT_RACES):
    """Matchup list for seed indices 0..n_seeds-1."""
    return [matchup_for_seed(i, races) for i in range(n_seeds)]


def recommended_n_pairs(target, races=DEFAULT_RACES):
    """Smallest multiple of len(races)**2 that is >= target.

    Running a whole number of round-robin blocks keeps the per-matchup game
    counts exactly balanced (e.g. target 800 with 5 races -> 800, which is
    32 full blocks of 25; target 790 -> 800).
    """
    block = len(races) ** 2
    if target <= 0:
        return block
    return ((target + block - 1) // block) * block


# ── paired per-race evaluation ──────────────────────────────────────────────

def chess_score(own, opp):
    """W=1, D=0.5, L=0."""
    return 1.0 if own > opp else (0.0 if own < opp else 0.5)


def _paired_stats(samples):
    """Aggregate a list of paired samples.

    Each sample: (cand_chess, base_chess) -- the same race played by the
    candidate and by the baseline within one side-swapped seed pair.
    """
    n = len(samples)
    cand_w = sum(1 for c, _ in samples if c == 1.0)
    cand_l = sum(1 for c, _ in samples if c == 0.0)
    base_w = sum(1 for _, b in samples if b == 1.0)
    base_l = sum(1 for _, b in samples if b == 0.0)
    cand_dec = cand_w + cand_l
    base_dec = base_w + base_l
    cand_wr = cand_w / cand_dec if cand_dec else None
    base_wr = base_w / base_dec if base_dec else None

    deltas = [c - b for c, b in samples]
    mean_d = sum(deltas) / n if n else 0.0
    t = None
    if n >= 2:
        var = sum((d - mean_d) ** 2 for d in deltas) / (n - 1)
        sd = math.sqrt(var)
        t = mean_d / (sd / math.sqrt(n)) if sd > 0 else (0.0 if mean_d == 0 else float("inf") * (1 if mean_d > 0 else -1))
    return {
        "n_paired": n,
        "cand_decisive_wr": round(cand_wr, 4) if cand_wr is not None else None,
        "base_decisive_wr": round(base_wr, 4) if base_wr is not None else None,
        "delta_decisive_pp": (round(100 * (cand_wr - base_wr), 2)
                              if cand_wr is not None and base_wr is not None else None),
        "mean_delta_chess": round(mean_d, 4),
        "t": round(t, 3) if t not in (None, float("inf"), float("-inf")) else t,
        "significant_95": (t is not None and t not in (float("inf"), float("-inf"))
                           and abs(t) > 1.96),
    }


def per_race_paired(rows, races=None):
    """Paired within-matchup per-race summary (dwarf report §3b pattern).

    Groups rows into side-swapped pairs by seed_idx. For a pair with
    matchup (ra, rb):
      orientation cand_home=True : candidate plays ra, baseline plays rb;
      orientation cand_home=False: baseline plays ra, candidate plays rb.
    So race ra gets the paired sample
      (chess(cand as ra), chess(base as ra))
    and race rb symmetrically. Mirrors (ra == rb) contribute two samples per
    pair, one from each orientation.

    Returns {race: {"overall": stats, "vs": {other_race: stats}}} plus a
    "_meta" entry with unpaired-row diagnostics. Rows lacking their mirror
    orientation are skipped (paired analysis is meaningless for them).
    """
    by_seed = defaultdict(dict)
    for r in rows:
        by_seed[r["seed_idx"]][bool(r["cand_home"])] = r

    # samples[race][opponent_race] -> list of (cand_chess, base_chess)
    samples = defaultdict(lambda: defaultdict(list))
    unpaired = 0
    for seed_idx in sorted(by_seed):
        pair = by_seed[seed_idx]
        if True not in pair or False not in pair:
            unpaired += len(pair)
            continue
        rt, rf = pair[True], pair[False]
        if (rt["race_h"], rt["race_a"]) != (rf["race_h"], rf["race_a"]):
            raise ValueError(
                f"seed {seed_idx}: mirror orientations disagree on matchup "
                f"({rt['race_h']},{rt['race_a']}) vs ({rf['race_h']},{rf['race_a']})")
        ra, rb = rt["race_h"], rt["race_a"]
        # race ra: candidate plays it in rt, baseline plays it in rf
        samples[ra][rb].append((chess_score(rt["cand"], rt["base"]),
                                chess_score(rf["base"], rf["cand"])))
        # race rb: candidate plays it in rf, baseline plays it in rt
        samples[rb][ra].append((chess_score(rf["cand"], rf["base"]),
                                chess_score(rt["base"], rt["cand"])))

    race_order = list(races) if races else sorted(samples)
    out = {}
    for race in race_order:
        if race not in samples:
            continue
        vs = {opp: _paired_stats(s) for opp, s in sorted(samples[race].items())}
        all_samples = [smp for s in samples[race].values() for smp in s]
        out[race] = {"overall": _paired_stats(all_samples), "vs": vs}
    out["_meta"] = {"n_rows": len(rows), "n_unpaired_rows_skipped": unpaired,
                    "n_pairs": sum(1 for p in by_seed.values()
                                   if True in p and False in p)}
    return out
