#!/usr/bin/env python3
"""Per-drive first-possession diagnostic (paired-seed, side-swap aware).

WHY (2026-07-14, backlog item 3): the stall-guard paired A/B (N=400 pairs,
diag_stall_guard_ab_20260713.log) CONFIRMED home_win +7.0pp [+1.3,+12.7],
McNemar p=0.020 -- in a MIRROR match where both sides run identical weights.
The engine has no coin toss (game_simulator.cpp hard-codes kickingTeam=AWAY,
"Home receives first"), so the leading hypothesis is a latent first-possession
advantage that surfaces as offensive fixes break the 0:0 draw plateau.
Repeating draw-rate runs cannot separate "better offense" from "stronger
first-possession bias" -- only per-drive metrics can.

WHAT IT MEASURES (all derived read-only from get_turn_logs(), no engine change):
  - drive segmentation: a drive starts at snapshot 0, after any touchdown-
    flagged snapshot, and at each half change; the RECEIVER of a drive is the
    active_team of its first snapshot (simpleKickoff sets activeTeam=receiving
    and bumps its turnNumber, so the first post-kickoff snapshot is always the
    receiver's turn). This works pre- and post-H2-kickoff-fix and would keep
    working under any future coin toss: nothing about the schedule is assumed,
    everything is observed.
  - schedule audit: who actually receives drive 1, the H2 opening, and each
    post-TD drive. Doubles as an N-games end-to-end regression check for the
    2026-07-14 H2 kickoff fix (post-fix expectation: H2 opening receiver =
    away in 100% of games; post-TD receiver = conceding side in 100%).
  - first-drive conversion: P(receiver of drive 1 scores that same drive).
  - receiver conversion by side: P(drive ends in receiver TD | home received)
    vs the same for away, pooled over both orientations.
  - TD decomposition per game: 2x2 cells (home/away x scored-as-receiver /
    scored-as-kicker) + received-drive counts per side.
  - TD timing: receiver turns used per converted drive, per half.
  - slot advantage: home W/L on decisive games pooled over BOTH orientations
    (exact binomial vs 0.5) -- the confirmatory test that the +7pp home edge
    is a home-SLOT effect, not a race-assignment artifact.

SIDE-SWAP: every seed is played twice, orientation "fwd" (race A home / race B
away, exactly the production _gate_game matchup) and orientation "swp" (rosters
exchanged; weights are identical in the mirror so nothing else changes). The
swap is deterministic and exactly balanced -- unlike a coin toss it needs no
engine change, no rebuild, and no new mirror baseline.

USAGE (from repo root, inside venv; DO NOT run while a training run owns the
engine build -- this script executes games):

    python3 diag_first_possession.py run <label> [N]      # both orientations
    python3 diag_first_possession.py report <label>       # single-build read
    python3 diag_first_possession.py compare <base> <cand># cross-build paired

Cross-rebuild A/B (the usual C++ fix pattern): run `run baseline N` on the old
binary, rebuild, run `run candidate N`, then `compare baseline candidate`.
Arms persist as arm_first_possession_<label>_<fwd|swp>.json via diag_utils.

PRIMARY metrics for cross-build verdicts are first-drive conversion (McNemar)
and the paired TD-decomposition deltas. Draw rate is companion-only per
feedback_draw_rate_noise_floor (single-run deltas <10pp are noise).
"""
from __future__ import annotations

import math
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

import diag_utils as du
from run_iteration import GATE_DIRICHLET_ALPHA, GATE_EXPLORATION_C, _RACES

W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1000, 0.0, 100
BASE_SEED = 20260714
DEFAULT_N = 300
ORIENTS = ("fwd", "swp")


def arm_path(label: str, orient: str) -> str:
    return f"arm_first_possession_{label}_{orient}.json"


# --------------------------------------------------------------------------
# Game worker: run one logged game, reduce turn logs to a per-drive summary.
# The reduction happens INSIDE the worker so only a small dict crosses the
# process boundary (LoggedGameResult itself is not picklable/cheap).
# --------------------------------------------------------------------------

def summarize_drives(turns: list, hs: int, as_: int, swap: bool) -> dict:
    """Reduce get_turn_logs() output to a compact per-drive record.

    Drive starts: snapshot 0; any snapshot following a touchdown-flagged one;
    any half change. Receiver = active_team of the drive's first snapshot.
    Scorer of a drive = the side whose score increases between the drive's
    first snapshot and the NEXT drive's first snapshot (or the final result
    for the last drive) -- snapshot scores are captured at turn start, so the
    TD scored during a drive is first visible in the following snapshot.
    """
    n = len(turns)
    drives: list[dict] = []
    anomalies = 0
    if n == 0:
        return {"hs": hs, "as": as_, "swap": swap, "drives": drives,
                "anomalies": 1, "n_snapshots": 0}

    starts = [0]
    for k in range(1, n):
        if turns[k]["half"] != turns[k - 1]["half"] or turns[k - 1]["touchdown"]:
            starts.append(k)

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
            anomalies += 1  # score should change by at most one TD per drive
        scorer = "home" if dh > 0 else ("away" if da > 0 else None)
        td_flag = bool(seg[-1]["touchdown"])
        if td_flag != (scorer is not None):
            anomalies += 1  # flag and score delta must agree
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
            "td_turn": recv_turns if end == "td_recv" else None,
        })

    return {"hs": hs, "as": as_, "swap": swap, "drives": drives,
            "anomalies": anomalies, "n_snapshots": n}


def _fp_game(args: tuple) -> dict:
    """One mirror game -> per-drive summary dict.

    Task tuple mirrors the _gate_game 9-tuple plus a trailing swap flag:
    (seed, race_idx, home_w, away_w, mcts, vf_blend, tv, leaf_lookahead,
     policy_path, swap). With swap=True the ROSTERS are exchanged (race B gets
    the home slot) and the weights paths are exchanged with them -- a no-op in
    the mirror where both paths are equal, but it keeps the function honest if
    someone later points the two paths at different weights.
    """
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
    return summarize_drives(lgr.get_turn_logs(),
                            lgr.result.home_score, lgr.result.away_score, swap)


# --------------------------------------------------------------------------
# Named outcome predicates (named so mcnemar_report prints something legible).
# --------------------------------------------------------------------------

def home_win(g: dict) -> bool:
    return g["hs"] > g["as"]


def draw(g: dict) -> bool:
    return g["hs"] == g["as"]


def first_drive_td(g: dict) -> bool:
    """Receiver of drive 1 scored on drive 1 (first-possession conversion)."""
    return bool(g["drives"]) and g["drives"][0]["end"] == "td_recv"


# --------------------------------------------------------------------------
# Small stats helpers (Wilson CI, exact binomial sign test, paired mean).
# --------------------------------------------------------------------------

def wilson(k: int, n: int, z: float = 1.959964) -> tuple[float, float, float]:
    if n == 0:
        return 0.0, 0.0, 1.0
    p = k / n
    denom = 1 + z * z / n
    center = (p + z * z / (2 * n)) / denom
    half = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denom
    return p, max(0.0, center - half), min(1.0, center + half)


def binom_two_sided(k: int, n: int) -> float:
    """Exact two-sided binomial test of k successes in n vs p=0.5."""
    if n == 0:
        return 1.0
    kk = min(k, n - k)
    tail = sum(math.comb(n, j) for j in range(kk + 1)) / 2 ** n
    return min(1.0, 2.0 * tail)


def paired_mean_report(diffs: list[float], name: str,
                       mean_a: float, mean_b: float,
                       label_a: str = "candidate",
                       label_b: str = "baseline") -> str:
    """Paired mean-difference CI, same shape as diag_stall_guard_blitz.py."""
    n = len(diffs)
    if n < 2:
        return f"  {name}: <2 pairs, no test"
    mean = sum(diffs) / n
    var = sum((d - mean) ** 2 for d in diffs) / (n - 1)
    se = (var / n) ** 0.5
    lo, hi = mean - 1.96 * se, mean + 1.96 * se
    verdict = "CONFIRMED" if (lo > 0 or hi < 0) else "INCONCLUSIVE"
    return (f"  {name}: {label_a} {mean_a:.3f} vs {label_b} {mean_b:.3f}  "
            f"paired delta {mean:+.3f} (95% CI [{lo:+.3f},{hi:+.3f}], n={n}) "
            f"-> {verdict}")


# --------------------------------------------------------------------------
# Per-game feature extraction used by report/compare.
# --------------------------------------------------------------------------

def game_cells(g: dict) -> dict:
    """2x2 TD decomposition + possession counts for one game."""
    c = {"home_recv_td": 0, "away_recv_td": 0,
         "home_kick_td": 0, "away_kick_td": 0,
         "home_recv_n": 0, "away_recv_n": 0, "drives": len(g["drives"])}
    for d in g["drives"]:
        c[f"{d['recv']}_recv_n"] += 1
        if d["scorer"] is None:
            continue
        role = "recv" if d["end"] == "td_recv" else "kick"
        c[f"{d['scorer']}_{role}_td"] += 1
    return c


def mean_cell(games: list[dict], key: str) -> float:
    if not games:
        return 0.0
    return sum(game_cells(g)[key] for g in games) / len(games)


# --------------------------------------------------------------------------
# report: single-build characterization over both orientations.
# --------------------------------------------------------------------------

def load_label(label: str) -> dict[str, tuple[list[int], dict[int, dict]]]:
    arms = {}
    for orient in ORIENTS:
        p = Path(arm_path(label, orient))
        if p.exists():
            _, seeds, res = du.load_arm(p)
            arms[orient] = (seeds, res)
    if not arms:
        sys.exit(f"no arms found for label {label!r} "
                 f"(expected {arm_path(label, 'fwd')} etc.) -- run first")
    return arms


def schedule_audit(games: list[dict]) -> str:
    """Empirically verify the kickoff schedule -- e2e check of the H2 fix."""
    d1 = Counter()
    h2 = Counter()
    post_ok = post_bad = 0
    for g in games:
        ds = g["drives"]
        if ds:
            d1[ds[0]["recv"]] += 1
        for prev, cur in zip(ds, ds[1:]):
            if cur["half"] != prev["half"]:
                h2[cur["recv"]] += 1
            elif prev["scorer"] is not None:
                conceded = "away" if prev["scorer"] == "home" else "home"
                if cur["recv"] == conceded:
                    post_ok += 1
                else:
                    post_bad += 1
    lines = [
        f"  drive-1 receiver : {dict(d1)}  (engine hard-codes home)",
        f"  H2 opening recv  : {dict(h2)}  "
        f"(post H2-fix expectation: 100% away; a home entry here means the "
        f"old last-drive-flip behavior survives)",
        f"  post-TD receiver : conceding side {post_ok}/{post_ok + post_bad}"
        + ("  <-- VIOLATIONS, kicking-team-after-TD regression!"
           if post_bad else "  (OK)"),
    ]
    return "\n".join(lines)


def report(label: str) -> None:
    arms = load_label(label)
    print(f"=== first-possession report: label={label!r}  "
          f"orientations={list(arms)} ===", flush=True)

    all_games: list[dict] = []
    for orient, (_seeds, res) in arms.items():
        games = list(res.values())
        all_games += games
        w = sum(1 for g in games if g["hs"] > g["as"])
        d = sum(1 for g in games if g["hs"] == g["as"])
        l = len(games) - w - d
        anom = sum(g["anomalies"] for g in games)
        print(f"\n--- orientation {orient!r}: n={len(games)}  "
              f"{w}W {d}D {l}L  home_win {100 * w / len(games):.1f}%  "
              f"draws {100 * d / len(games):.1f}%  "
              f"(segmentation anomalies: {anom}) ---", flush=True)
        k1 = sum(1 for g in games if first_drive_td(g))
        p, lo, hi = wilson(k1, len(games))
        print(f"  first-drive conversion: {k1}/{len(games)} = {100 * p:.1f}% "
              f"(95% CI [{100 * lo:.1f}, {100 * hi:.1f}]%)", flush=True)

    # --- slot advantage: pooled over both orientations -----------------------
    print("\n--- slot advantage (pooled over orientations) ---", flush=True)
    w = sum(1 for g in all_games if g["hs"] > g["as"])
    l = sum(1 for g in all_games if g["hs"] < g["as"])
    dec = w + l
    p_sign = binom_two_sided(w, dec)
    edge = (w - l) / dec if dec else 0.0
    print(f"  decisive games: {dec}/{len(all_games)}  home {w}W {l}L  "
          f"edge {100 * edge:+.1f}pp of decisive  exact p={p_sign:.4f}",
          flush=True)
    print("  VERDICT: " + ("CONFIRMED home-slot advantage"
                           if p_sign < 0.05 else
                           "INCONCLUSIVE (no slot advantage demonstrated)"),
          flush=True)
    if len(arms) == 2:
        _, res_f = arms["fwd"]
        _, res_s = arms["swp"]
        print("\n  race-vs-slot check (fwd vs swp, paired by seed; a "
              "CONFIRMED delta here\n  means race assignment to the home "
              "slot matters, i.e. NOT a pure slot effect):", flush=True)
        print("  " + du.mcnemar_report(res_f, res_s, outcome=home_win,
                                       label_a="fwd", label_b="swp")
              .replace("\n", "\n  "), flush=True)

    # --- schedule audit ------------------------------------------------------
    print("\n--- kickoff schedule audit (e2e check of the H2 fix) ---",
          flush=True)
    print(schedule_audit(all_games), flush=True)

    # --- receiver conversion by side, pooled ---------------------------------
    print("\n--- receiver conversion by side (pooled, drives as units) ---",
          flush=True)
    conv = {}
    for side in ("home", "away"):
        drives = [d for g in all_games for d in g["drives"]
                  if d["recv"] == side]
        k = sum(1 for d in drives if d["end"] == "td_recv")
        conv[side] = (k, len(drives))
        p, lo, hi = wilson(k, len(drives))
        print(f"  {side} received {len(drives)} drives, converted {k} "
              f"= {100 * p:.1f}% (95% CI [{100 * lo:.1f}, {100 * hi:.1f}]%)",
              flush=True)
    (kh, nh), (ka, na) = conv["home"], conv["away"]
    if nh and na:
        ph, pa = kh / nh, ka / na
        se = math.sqrt(ph * (1 - ph) / nh + pa * (1 - pa) / na)
        diff = ph - pa
        lo, hi = diff - 1.96 * se, diff + 1.96 * se
        asym = lo > 0 or hi < 0
        print(f"  home-away conversion diff: {100 * diff:+.1f}pp "
              f"(95% CI [{100 * lo:+.1f}, {100 * hi:+.1f}]pp; drives treated "
              f"as independent -- slightly optimistic)", flush=True)
        print("  READING: " + (
            "side-dependent conversion -> engine SIDE ASYMMETRY suspected "
            "beyond possession structure" if asym else
            "no side-dependent conversion -> home edge, if confirmed above, "
            "is consistent with POSSESSION STRUCTURE (who receives when)"),
            flush=True)

    # --- TD decomposition + possession counts --------------------------------
    print("\n--- per-game means (pooled) ---", flush=True)
    for key, desc in (
            ("home_recv_td", "home TDs as receiver"),
            ("away_recv_td", "away TDs as receiver"),
            ("home_kick_td", "home TDs as kicker (counter)"),
            ("away_kick_td", "away TDs as kicker (counter)"),
            ("home_recv_n", "drives received by home"),
            ("away_recv_n", "drives received by away"),
            ("drives", "drives per game")):
        print(f"  {desc:32s}: {mean_cell(all_games, key):.3f}", flush=True)

    # --- TD timing ------------------------------------------------------------
    print("\n--- TD timing (receiver turns used, converted drives only) ---",
          flush=True)
    for half in (1, 2):
        tts = [d["td_turn"] for g in all_games for d in g["drives"]
               if d["half"] == half and d["td_turn"] is not None]
        if tts:
            mean_tt = sum(tts) / len(tts)
            hist = Counter(tts)
            print(f"  H{half}: n={len(tts)}  mean {mean_tt:.2f} turns  "
                  f"hist {dict(sorted(hist.items()))}", flush=True)
        else:
            print(f"  H{half}: no converted drives", flush=True)


# --------------------------------------------------------------------------
# compare: cross-build paired A/B on the derived metrics.
# --------------------------------------------------------------------------

def compare(label_base: str, label_cand: str) -> None:
    base = load_label(label_base)
    cand = load_label(label_cand)
    for orient in ORIENTS:
        if (orient in base) != (orient in cand):
            sys.exit(f"orientation {orient!r} present in only one label -- "
                     f"not comparable")
        if orient in base and base[orient][0] != cand[orient][0]:
            sys.exit(f"ABORT: {orient!r} arms ran on different seed lists -- "
                     f"not paired")
    print(f"=== first-possession paired compare: candidate={label_cand!r} "
          f"vs baseline={label_base!r} ===", flush=True)

    for orient in [o for o in ORIENTS if o in base]:
        res_b, res_c = base[orient][1], cand[orient][1]
        print(f"\n--- orientation {orient!r} ---", flush=True)
        print(du.mcnemar_report(res_c, res_b, outcome=first_drive_td),
              flush=True)
        print(flush=True)
        print(du.mcnemar_report(res_c, res_b, outcome=home_win), flush=True)
        print(flush=True)
        print(du.mcnemar_report(res_c, res_b, outcome=draw), flush=True)

        common = sorted(set(res_c) & set(res_b))
        print("\n  paired per-game mean deltas (candidate - baseline):",
              flush=True)
        specs = (
            ("TD/game", lambda g: g["hs"] + g["as"]),
            ("receiver TDs/game (offense)",
             lambda g: game_cells(g)["home_recv_td"]
             + game_cells(g)["away_recv_td"]),
            ("kicker TDs/game (counter)",
             lambda g: game_cells(g)["home_kick_td"]
             + game_cells(g)["away_kick_td"]),
            ("drives/game", lambda g: game_cells(g)["drives"]),
            ("home score margin", lambda g: g["hs"] - g["as"]),
        )
        for name, f in specs:
            diffs = [f(res_c[i]) - f(res_b[i]) for i in common]
            ma = sum(f(res_c[i]) for i in common) / len(common)
            mb = sum(f(res_b[i]) for i in common) / len(common)
            print(paired_mean_report(diffs, name, ma, mb), flush=True)

    print("\nREADING GUIDE: an offensive fix should move 'receiver TDs/game'",
          flush=True)
    print("and first-drive conversion in BOTH orientations symmetrically; a",
          flush=True)
    print("fix that mostly moves home_win / home score margin while receiver",
          flush=True)
    print("conversion stays flat is amplifying the first-possession bias,",
          flush=True)
    print("not improving offense.", flush=True)


# --------------------------------------------------------------------------
# run: both orientations over one seed list.
# --------------------------------------------------------------------------

def run(label: str, n: int) -> None:
    seeds = du.paired_seeds(n, base_seed=BASE_SEED)
    print(f"=== first-possession run: label={label!r}  N={n} seeds x "
          f"{len(ORIENTS)} orientations ===", flush=True)
    print(f"weights={W}  policy={POLICY_PATH}  MCTS={MCTS}  "
          f"vf_blend={VF_BLEND}  tv={TV}", flush=True)
    print(f"gate config: dirichlet_alpha={GATE_DIRICHLET_ALPHA} "
          f"exploration_c={GATE_EXPLORATION_C}", flush=True)
    for orient in ORIENTS:
        swap = orient == "swp"
        tasks = [(s, i, W, W, MCTS, VF_BLEND, TV, False, POLICY_PATH, swap)
                 for i, s in enumerate(seeds)]
        res = du.run_arm(f"{label}-{orient}", tasks, game_fn=_fp_game,
                         mcts_iterations=MCTS)
        du.save_arm(arm_path(label, orient), f"{label}-{orient}", seeds, res)
    report(label)


def _selftest() -> None:
    """Pure-python check of summarize_drives on a synthetic turn-log stream."""
    def snap(half, team, hsc, asc, td=False):
        return {"half": half, "active_team": team, "home_score": hsc,
                "away_score": asc, "touchdown": td}
    # H1: home receives, scores on its 2nd turn; away receives next, half
    # ends scoreless; H2: away receives, kicker (home) counter-scores.
    turns = [
        snap(1, "home", 0, 0),            # drive 1, home recv, turn 1
        snap(1, "away", 0, 0),
        snap(1, "home", 0, 0, td=True),   # home TD on its 2nd turn
        snap(1, "away", 1, 0),            # drive 2, away recv
        snap(1, "home", 1, 0),
        snap(2, "away", 1, 0),            # H2 opening, away recv
        snap(2, "home", 1, 0, td=True),   # counter TD by home
        snap(2, "away", 2, 0),            # drive 4, away recv again
    ]
    g = summarize_drives(turns, 2, 0, swap=False)
    ds = g["drives"]
    assert len(ds) == 4 and g["anomalies"] == 0, ds
    assert ds[0] == {"half": 1, "recv": "home", "end": "td_recv",
                     "scorer": "home", "recv_turns": 2, "td_turn": 2}, ds[0]
    assert ds[1]["recv"] == "away" and ds[1]["end"] == "half"
    assert ds[2] == {"half": 2, "recv": "away", "end": "td_kick",
                     "scorer": "home", "recv_turns": 1, "td_turn": None}, ds[2]
    assert ds[3]["recv"] == "away" and ds[3]["end"] == "game"
    assert first_drive_td(g) and home_win(g) and not draw(g)
    cells = game_cells(g)
    assert cells["home_recv_td"] == 1 and cells["home_kick_td"] == 1
    assert cells["away_recv_n"] == 3 and cells["home_recv_n"] == 1
    # anomaly: score jumps without touchdown flag
    bad = [snap(1, "home", 0, 0), snap(1, "away", 0, 0)]
    gb = summarize_drives(bad, 1, 0, swap=False)
    assert gb["anomalies"] == 1, gb
    # stats helpers
    assert abs(binom_two_sided(5, 10) - 1.0) < 1e-12
    assert binom_two_sided(60, 100) < 0.06
    p, lo, hi = wilson(30, 100)
    assert lo < 0.3 < hi and abs(p - 0.3) < 1e-12
    print("diag_first_possession self-test: ALL PASS")


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else ""
    if mode == "run" and len(sys.argv) >= 3:
        run(sys.argv[2],
            int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_N)
    elif mode == "report" and len(sys.argv) >= 3:
        report(sys.argv[2])
    elif mode == "compare" and len(sys.argv) >= 4:
        compare(sys.argv[2], sys.argv[3])
    elif mode == "selftest":
        _selftest()
    else:
        sys.exit(__doc__)
