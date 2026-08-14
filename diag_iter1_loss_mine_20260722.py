"""Logged mining pass over iteration 1's actual gate matchup (candidate
weights_iter1_20260722_candidate_train_best.json vs frozen
weights_iter1_20260722_frozen.json), same settings as the real gate
(MCTS=100, GATE_VF_BLEND=0.15), to see qualitatively what's happening in
the head-to-head losses (43.2% decisive in the real 600-game run).

Low parallelism (Pool(3)) deliberately, to not compete with the
concurrent iteration 2 self-play's 12 workers (same idiom as
diag_replay_mine_20260721.py).

Usage:
    python3 diag_iter1_loss_mine_20260722.py collect [n=40]
    python3 diag_iter1_loss_mine_20260722.py scan
"""
import gzip
import json
import sys
import time
from multiprocessing import Pool
from pathlib import Path

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
CAND = "weights_iter1_20260722_candidate_train_best.json"
FROZEN = "weights_iter1_20260722_frozen.json"
TV, GATE_VF_BLEND, MCTS = 1200, 0.15, 100
BASE_SEED = 99720722
DATA_ROOT = Path("diag_iter1_loss_mine_20260722_data")


def _game_worker(args: tuple) -> dict:
    seed, i, out_path = args
    import bb_engine
    cand_is_away = (i % 2 == 1)
    race_idx = i % len(RACES)
    ra = RACES[race_idx]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    home_w = FROZEN if cand_is_away else CAND
    away_w = CAND if cand_is_away else FROZEN
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=GATE_VF_BLEND,
    )
    cs = lgr.result.away_score if cand_is_away else lgr.result.home_score
    fs = lgr.result.home_score if cand_is_away else lgr.result.away_score
    turns = lgr.get_turn_logs()
    rec = {
        "seed": seed, "home_race": ra, "away_race": rb,
        "cand_is_away": cand_is_away,
        "cand_score": cs, "frozen_score": fs,
        "home_score": lgr.result.home_score, "away_score": lgr.result.away_score,
        "turn_logs": turns,
    }
    with gzip.open(out_path, "wt") as f:
        json.dump(rec, f, default=lambda o: o.tolist() if hasattr(o, "tolist") else o)
    return {"seed": seed, "cand_score": cs, "frozen_score": fs,
            "cand_is_away": cand_is_away, "n_turns": len(turns)}


def cmd_collect(n: int) -> None:
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i, str(DATA_ROOT / f"g{i:04d}.json.gz")) for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(3) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            outcome = "WIN" if r["cand_score"] > r["frozen_score"] else (
                "LOSS" if r["cand_score"] < r["frozen_score"] else "DRAW")
            print(f"[{done}/{n}] seed={r['seed']} cand{'@A' if r['cand_is_away'] else '@H'} "
                  f"{r['cand_score']}-{r['frozen_score']} {outcome} turns={r['n_turns']} "
                  f"({time.time()-t0:.0f}s)")


def cmd_scan() -> None:
    files = sorted(DATA_ROOT.glob("g*.json.gz"))
    wins = losses = draws = 0
    cand_nil = frozen_nil = both_nil = 0
    cand_turn_counts = []
    frozen_turn_counts = []
    for fp in files:
        with gzip.open(fp, "rt") as f:
            rec = json.load(f)
        if rec["cand_score"] > rec["frozen_score"]:
            wins += 1
        elif rec["cand_score"] < rec["frozen_score"]:
            losses += 1
        else:
            draws += 1
        if rec["cand_score"] == 0:
            cand_nil += 1
        if rec["frozen_score"] == 0:
            frozen_nil += 1
        if rec["cand_score"] == 0 and rec["frozen_score"] == 0:
            both_nil += 1
    n = len(files)
    print(f"games: {n} | cand W{wins} D{draws} L{losses}")
    print(f"cand scored 0: {cand_nil}/{n} ({100*cand_nil/n:.0f}%) | "
          f"frozen scored 0: {frozen_nil}/{n} ({100*frozen_nil/n:.0f}%) | "
          f"0-0 both: {both_nil}/{n} ({100*both_nil/n:.0f}%)")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "collect"
    if cmd == "collect":
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 40
        cmd_collect(n)
    elif cmd == "scan":
        cmd_scan()
    else:
        print(f"unknown command {cmd}")
