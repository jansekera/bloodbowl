#!/usr/bin/env python3
"""Decision-level sanity check for the SCORE-availability patch (A+B),
per proposals_score_availability_20260714.md section 6, step 2: cheap,
mechanism-level check BEFORE escalating the noisy game-level A/B to N=400+.

Measures, over a batch of self-play games with policy-decision logging:
"of decisions where a SCORE-family macro (SCORE/BLITZ_AND_SCORE/HAND_OFF_SCORE/
PASS_SCORE/CHAIN_SCORE, action_features[0]==1) is a candidate at all, what
fraction have it as the most-visited (chosen) candidate?"

Target from the doc: 7.7% -> >30% (Patch A alone), safe-walk-in subset >50%.
This directly answers "did the mechanism fire" independent of the noisy
game-level draw-rate outcome (N=150 A/B today came back INCONCLUSIVE but
right-direction, delta -2.7pp draws, CI[-9.8,+4.5]pp).

Usage: python3 diag_score_availability_decisionlevel_20260720.py <off|on> [N]
"""
import gzip
import json
import sys
import time
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, "python")

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, MCTS = 1200, 100
BASE_SEED = 20260731  # distinct from the game-level A/B's 20260720
DATA_ROOT = Path("diag_score_availability_decisionlevel_data")

ENGINES = {
    "off": "../bloodbowl_scoreavail_off/engine/build",
    "on": "engine/build",
}


def _game_worker(args: tuple) -> dict:
    seed, race_idx, out_path, engine_path = args
    import sys as _sys
    _sys.path.insert(0, engine_path)
    import bb_engine
    ra = RACES[race_idx % len(RACES)]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=0.0,
        policy_weights_path=POLICY_PATH,
    )
    decisions = lgr.get_policy_decisions()

    def carrier_tz(d):
        """Tacklezone count on the ball carrier, if the carrier is on the
        deciding (perspective) side -- None if no carrier or carrier is the
        opponent's (can't be a SCORE candidate for this decision either way)."""
        if not d.get("ball_held") or d.get("ball_carrier_id", -1) == -1:
            return None
        cid = d["ball_carrier_id"]
        mine = d["home_players"] if d["perspective"] == "home" else d["away_players"]
        theirs = d["away_players"] if d["perspective"] == "home" else d["home_players"]
        carrier = next((p for p in mine if p["id"] == cid), None)
        if carrier is None:
            return None  # carrier belongs to the other side
        return sum(1 for p in theirs if p["state"] == 0
                   and max(abs(p["x"] - carrier["x"]), abs(p["y"] - carrier["y"])) == 1)

    # Keep only what we need (action_features[0] = SCORE-family one-hot,
    # visit_fraction, plus the carrier's TZ count for the marker-free split).
    compact = [
        {"visits": [{"score": bool(v["action_features"][0] > 0.5),
                     "vf": v["visit_fraction"]} for v in d["visits"]],
         "carrier_tz": carrier_tz(d)}
        for d in decisions
    ]
    with gzip.open(out_path, "wt") as f:
        json.dump(compact, f)
    return {"seed": seed, "n_dec": len(decisions)}


def collect(arm: str, n: int) -> None:
    engine_path = ENGINES[arm]
    out_dir = DATA_ROOT / arm
    out_dir.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i % len(RACES), str(out_dir / f"g{i:04d}.json.gz"), engine_path)
             for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(10) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            print(f"[{arm}][{done}/{n}] seed={r['seed']} decisions={r['n_dec']} "
                  f"({time.time()-t0:.0f}s)", flush=True)
    print(f"[{arm}] DONE {n} games in {time.time()-t0:.0f}s -> {out_dir}")


def analyze(arm: str) -> None:
    out_dir = DATA_ROOT / arm
    # buckets: 'free' (carrier_tz == 0, the patch's actual target subset),
    # 'marked' (carrier_tz >= 1, forced-dodge cases the patch mispriced),
    # 'other' (no carrier on this side, e.g. hand-off/pass/chain scoring paths)
    buckets = {"free": [0, 0], "marked": [0, 0], "other": [0, 0]}
    for f in sorted(out_dir.glob("g*.json.gz")):
        with gzip.open(f, "rt") as fh:
            decisions = json.load(fh)
        for d in decisions:
            visits = d["visits"]
            if not visits:
                continue
            has_score = any(v["score"] for v in visits)
            if not has_score:
                continue
            tz = d.get("carrier_tz")
            bucket = "other" if tz is None else ("free" if tz == 0 else "marked")
            top = max(visits, key=lambda v: v["vf"])
            buckets[bucket][0] += 1
            if top["score"]:
                buckets[bucket][1] += 1

    for name, (n, c) in buckets.items():
        pct = 100.0 * c / n if n else float("nan")
        print(f"[{arm}] {name:>6}: SCORE candidate in {n} decisions; "
              f"chosen in {c} ({pct:.1f}%)")
    n_all = sum(n for n, _ in buckets.values())
    c_all = sum(c for _, c in buckets.values())
    print(f"[{arm}]  total: {c_all}/{n_all} ({100.0*c_all/n_all:.1f}%)")


if __name__ == "__main__":
    arm = sys.argv[1]
    if len(sys.argv) > 2 and sys.argv[2] == "analyze":
        analyze(arm)
    else:
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 30
        collect(arm, n)
        analyze(arm)
