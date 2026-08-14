"""A2-1 (fable 2026-07-30): PRVNI ferove vyhodnoceni naucene policy hlavy.

Sampion vs sampion (weights_best.json obe strany, vf_blend=0.15, MCTS=100,
epsilon=0). Obe strany sdileji policy sit weights_policy.json (away_policy_
weights_path='' -> away sdili home net, ea92bd0), takze prior-floor rezim
(macro_mcts.cpp expand(), gated na config_.policy != nullptr) je aktivni
SYMETRICKY. Jedina asymetrie: kandidatni strana ma policy_blend=B, druha 0.0.

3 ramena B in {0.1, 0.2, 0.3}; kazde 75 side-swapped seed paru = 150 her
(stejny seed hran s kandidatem home i away, vzor diag_champion_cert_20260730).
Stejna sada seedu napric rameny. Dirichlet/exploration_c = engine defaults
(0.3 / 0.5) jako diag_champion_cert. Inkrementalni zapis vysledku po kazde hre
-> diag_policy_fairtest_20260730_results.json (odolnost proti padu session).
"""
import json
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, 'engine/build')

BEST = "weights_best.json"
POLICY = "weights_policy.json"
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
BLENDS = [0.1, 0.2, 0.3]
N_PAIRS = 75          # na rameno; x2 orientace = 150 her/rameno, 450 celkem
MCTS, TV, VFB = 100, 1200, 0.15
SEED_BASE = 30_000_000
OUT = "diag_policy_fairtest_20260730_results.json"


def game(args):
    blend, seed_idx, cand_home = args
    import bb_engine
    ra = RACES[seed_idx % len(RACES)]
    rb = RACES[(seed_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    home_blend = blend if cand_home else 0.0
    away_blend = 0.0 if cand_home else blend
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=SEED_BASE + seed_idx, mcts_iterations=MCTS,
        weights_path=BEST, away_weights_path=BEST,
        epsilon=0.0, vf_blend=VFB,
        policy_weights_path=POLICY, policy_blend=home_blend,
        away_policy_weights_path='',      # away sdili home net -> floors symetricky
        away_policy_blend=away_blend)
    hs, as_ = lgr.result.home_score, lgr.result.away_score
    cs, os_ = (hs, as_) if cand_home else (as_, hs)
    return {"blend": blend, "seed_idx": seed_idx, "cand_home": cand_home,
            "cand": cs, "base": os_}


def wilson(w, n, z=1.96):
    if n == 0:
        return (0.0, 0.0, 1.0)
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * ((p * (1 - p) / n + z * z / (4 * n * n)) ** 0.5) / d
    return (p, max(0.0, c - h), min(1.0, c + h))


def summarize(rows):
    out = {}
    for b in BLENDS:
        rs = [r for r in rows if r["blend"] == b]
        w = sum(1 for r in rs if r["cand"] > r["base"])
        l = sum(1 for r in rs if r["cand"] < r["base"])
        d = len(rs) - w - l
        p, lo, hi = wilson(w, w + l)
        out[str(b)] = {"n": len(rs), "W": w, "D": d, "L": l,
                       "decisive_wr": round(p, 4),
                       "wilson95": [round(lo, 4), round(hi, 4)]}
    return out


if __name__ == "__main__":
    tasks = [(b, i, ch) for b in BLENDS for i in range(N_PAIRS)
             for ch in (True, False)]
    out, t0 = [], time.time()
    with Pool(6) as pool:
        for r in pool.imap_unordered(game, tasks):
            out.append(r)
            json.dump({"done": len(out), "total": len(tasks),
                       "summary": summarize(out), "games": out},
                      open(OUT, "w"))
            if len(out) % 25 == 0:
                print(f"{len(out)}/{len(tasks)} ({time.time()-t0:.0f}s): "
                      + " | ".join(f"B={k}: {v['W']}W {v['D']}D {v['L']}L"
                                   for k, v in summarize(out).items()),
                      flush=True)
    print("FINAL:", json.dumps(summarize(out), indent=1), flush=True)
