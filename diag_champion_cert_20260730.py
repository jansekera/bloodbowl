"""N4 certification (fable_pipeline_audit_20260730): current weights_best.json
(uncertified, appeared via out-of-gate overwrite 22.07) vs the last
gate-PROMOTED champion 842c200 (16.06, type=neural, 70 features -- engine
loader ignores the 3 newer features via min()). Side-swapped seed pairs,
gate-like config (MCTS=100, vf_blend=0.15)."""
import json, sys, time
from multiprocessing import Pool
sys.path.insert(0, 'engine/build')

CUR, OLD = "weights_best.json", "weights_cert_842c200_20260616.json"
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
N_PAIRS, MCTS, TV, VFB = 150, 100, 1200, 0.15

def game(args):
    seed_idx, cur_home = args
    import bb_engine
    ra = RACES[seed_idx % len(RACES)]; rb = RACES[(seed_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV); ar = bb_engine.get_developed_roster(rb, TV)
    hw, aw = (CUR, OLD) if cur_home else (OLD, CUR)
    lgr = bb_engine.simulate_game_logged(hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=20260730 + seed_idx, mcts_iterations=MCTS, weights_path=hw,
        away_weights_path=aw, epsilon=0.0, vf_blend=VFB)
    hs, as_ = lgr.result.home_score, lgr.result.away_score
    cs, os_ = (hs, as_) if cur_home else (as_, hs)
    return {"seed_idx": seed_idx, "cur_home": cur_home, "cur": cs, "old": os_}

if __name__ == "__main__":
    tasks = [(i, s == 0) for i in range(N_PAIRS) for s in (0, 1)]
    out, t0 = [], time.time()
    with Pool(8) as pool:
        for r in pool.imap_unordered(game, tasks):
            out.append(r)
            if len(out) % 20 == 0:
                w = sum(1 for x in out if x["cur"] > x["old"]); l = sum(1 for x in out if x["cur"] < x["old"])
                d = len(out) - w - l
                print(f"{len(out)}/{len(tasks)}: cur {w}W {d}D {l}L ({time.time()-t0:.0f}s)", flush=True)
            json.dump(out, open("diag_champion_cert_20260730_results.json", "w"))
    w = sum(1 for x in out if x["cur"] > x["old"]); l = sum(1 for x in out if x["cur"] < x["old"])
    d = len(out) - w - l; dec = w + l
    print(f"FINAL: current {w}W {d}D {l}L | decisive {100*w/max(dec,1):.1f}% z {dec}", flush=True)
