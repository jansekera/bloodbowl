"""Post-fix validation for the ball-stuck deadlock (commit cc1560c, 2026-07-27).

Regenerates a fresh self-play corpus with the SAME config as
diag_replay_mine_20260721.py (24 games, macro_mcts vs macro_mcts,
weights_best/weights_policy, MCTS=100, vf_blend=0) but against the
post-fix engine build, then reruns the same "loose ball streak >= 4
consecutive turn-snapshots" scan used in mine_situations_20260723.py to
find the original 14% (111/769) prevalence. Also reports the
longest-streak metric (whole dead halves) directly, since that was the
most severe symptom flagged in
project_bloodbowl_ball_stuck_deadlock_20260723.

Usage:
    python3 verify_ballstuck_fix_20260727.py collect [n=24]
    python3 verify_ballstuck_fix_20260727.py scan
"""
import gzip
import json
import sys
import time
from multiprocessing import Pool
from pathlib import Path

sys.path.insert(0, "python")
sys.path.insert(0, "engine/build")

RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV, VF_BLEND, MCTS = 1200, 0.0, 100
BASE_SEED = 20260721  # same seeds as the original corpus, for apples-to-apples
DATA_ROOT = Path("verify_ballstuck_fix_20260727_data")


def _game_worker(args):
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


def cmd_collect(n):
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


def _player_at(t, x, y):
    for pl in (t["home_players"], t["away_players"]):
        for p in pl:
            if p["x"] == x and p["y"] == y:
                return p
    return None


def cmd_scan():
    files = sorted(DATA_ROOT.glob("g*.json.gz"))
    total_snapshots = 0
    loose_snapshots = 0
    pinned_snapshots = 0  # loose AND a player sits exactly on the ball square
    games_with_pin = set()
    streaks = []  # (game, streak_start, length) -- consecutive PINNED snapshots

    for fp in files:
        rec = json.load(gzip.open(fp, "rt"))
        turns = rec["turn_logs"]
        total_snapshots += len(turns)

        streak = 0
        streak_start = None
        for t in turns:
            loose = not t["ball_held"]
            pinned = loose and _player_at(t, t["ball_x"], t["ball_y"]) is not None
            if loose:
                loose_snapshots += 1
            if pinned:
                pinned_snapshots += 1
                games_with_pin.add(fp.name)
                if streak == 0:
                    streak_start = (t["half"], t["turn"], t["active_team"],
                                     t["ball_x"], t["ball_y"])
                streak += 1
            else:
                if streak >= 4:
                    streaks.append((fp.name, streak_start, streak))
                streak = 0
        if streak >= 4:
            streaks.append((fp.name, streak_start, streak))

    print(f"games scanned: {len(files)}")
    print(f"total turn-snapshots: {total_snapshots}")
    print(f"snapshots with ball loose (not held): {loose_snapshots} "
          f"({100*loose_snapshots/total_snapshots:.1f}%)")
    print(f"snapshots with ball PINNED under a player: {pinned_snapshots} "
          f"({100*pinned_snapshots/total_snapshots:.1f}%), "
          f"{len(games_with_pin)}/{len(files)} games affected")
    print(f"streaks >=4 consecutive pinned snapshots (whole-half deadlocks): "
          f"{len(streaks)}")
    for s in streaks:
        print("  ", s)
    print("\nOriginal (pre-fix, 2026-07-23) baseline for comparison:")
    print("  111/769 snapshots (14%) loose-ball-pinned across 13/24 games")
    print("  several whole 16-turn second halves scoreless because of it")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "collect"
    if cmd == "collect":
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 24
        cmd_collect(n)
    elif cmd == "scan":
        cmd_scan()
    else:
        print(f"unknown command {cmd}")
