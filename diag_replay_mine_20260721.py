"""Fresh replay-mining pass on the post-item7/item10 engine (2026-07-21).

Generates a modest self-play corpus (macro_mcts vs macro_mcts, current
weights_best.json/weights_policy.json, engine includes item7 top-2 PICKUP
and item10 REPOSITION/FOUL floor rebalance) and scans turn_logs for
candidate situations worth a human walkthrough -- same narrative method as
the 07-15/16 12-situation survey, redone because that corpus predates
today's engine changes.

Low parallelism (Pool(3)) deliberately, to not compete with the concurrent
overnight training run's 12 workers.

Usage:
    python3 diag_replay_mine_20260721.py collect [n=24]
    python3 diag_replay_mine_20260721.py scan
"""
import gzip
import json
import sys
import time
from multiprocessing import Pool
from pathlib import Path

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
BASE_SEED = 20260721
DATA_ROOT = Path("diag_replay_mine_20260721_data")


def _game_worker(args: tuple) -> dict:
    seed, race_idx, out_path = args
    import bb_engine
    ra = RACES[race_idx % len(RACES)]
    rb = RACES[(race_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=W, away_weights_path=W,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
    )
    turns = lgr.get_turn_logs()
    rec = {
        "seed": seed, "home_race": ra, "away_race": rb,
        "home_score": lgr.result.home_score, "away_score": lgr.result.away_score,
        "turn_logs": turns,
    }
    with gzip.open(out_path, "wt") as f:
        json.dump(rec, f, default=lambda o: o.tolist() if hasattr(o, "tolist") else o)
    return {"seed": seed, "hs": lgr.result.home_score, "as": lgr.result.away_score,
            "races": f"{ra}/{rb}", "n_turns": len(turns)}


def cmd_collect(n: int) -> None:
    DATA_ROOT.mkdir(parents=True, exist_ok=True)
    tasks = [(BASE_SEED + i, i % len(RACES), str(DATA_ROOT / f"g{i:04d}.json.gz"))
              for i in range(n)]
    t0 = time.time()
    done = 0
    with Pool(3) as pool:
        for r in pool.imap_unordered(_game_worker, tasks):
            done += 1
            print(f"[{done}/{n}] seed={r['seed']} {r['races']} "
                  f"{r['hs']}-{r['as']} turns={r['n_turns']} "
                  f"({time.time()-t0:.0f}s)")


def cmd_scan() -> None:
    files = sorted(DATA_ROOT.glob("g*.json.gz"))
    print(f"scanning {len(files)} games")
    for fp in files:
        with gzip.open(fp, "rt") as f:
            rec = json.load(f)
        turns = rec["turn_logs"]
        for i, t in enumerate(turns):
            for ev in t.get("events", []):
                if ev["type"] == "FOUL":
                    print(f"{fp.name} half{t['half']}turn{t['turn']} FOUL "
                          f"player={ev['player_id']} target={ev['target_id']} "
                          f"roll={ev['roll']} success={ev['success']} "
                          f"score={t['home_score']}-{t['away_score']}")
                if ev["type"] == "PICKUP" and not ev["success"]:
                    print(f"{fp.name} half{t['half']}turn{t['turn']} PICKUP FAIL "
                          f"player={ev['player_id']} at=({ev['from_x']},{ev['from_y']}) "
                          f"roll={ev['roll']}")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "collect"
    if cmd == "collect":
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 24
        cmd_collect(n)
    elif cmd == "scan":
        cmd_scan()
    else:
        print(f"unknown command {cmd}")
