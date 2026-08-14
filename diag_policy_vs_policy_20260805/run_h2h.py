"""Ukol 3 (zadani evidence/fable_postpromotion_dynamics_20260805.md):
parovy H2H dvou policy hlav — sampionova promotnuta (weights_best_policy.json,
md5 cd72ed6b...) vs. obnoveny stash s rejected deltou z 04.08.
(weights_policy.json, md5 fa7698b8...). Obe strany stejne value vahy
(weights_best.json) + policy blend 0.2, side-swapped pary.

PRE-REGISTROVANE CTENI (fixni pred spustenim, nemenit za behu):
- kandidat (CAND) = weights_policy.json (stash s dnesni rejected deltou),
  baseline = weights_best_policy.json (promotnuta policy sampiona)
- primarni metrika: decisive WR kandidata po vsech hrach + Wilson 95% CI
- interpretace: WR >= 0.5 + 1.645*sigma -> rejected trenink policy ZLEPSIL;
  WR <= 0.5 - 1.645*sigma -> ZHORSIL; jinak SUM (legitimni vysledek);
  ekvivalentne: 0.5 uvnitr Wilson CI = SUM
- zadny interim stop (N je male, 300 paru = 600 her); sanity guardy jako
  diag_policy_confirm: decisive rate mimo (45,75) %, |WR_home-WR_away|>15pp
- sampion weights_best.json md5 guard pred/po; vsechny vahy JEN CIST

Spusteni: nice -19, Pool(2) — limit ze zadani (max 2 procesy).
"""
import hashlib
import json
import math
import os
import time
from multiprocessing import Pool

import sys
sys.path.insert(0, '/home/jan/claude/bloodbowl/engine/build')
os.chdir('/home/jan/claude/bloodbowl')

BEST = "weights_best.json"
POL_CAND = "weights_policy.json"          # stash, md5 fa7698b8...
POL_BASE = "weights_best_policy.json"     # promotnuta, md5 cd72ed6b...
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
BLEND = 0.2
N_PAIRS = 300
MCTS, TV, VFB = 100, 1200, 0.15
SEED_BASE = 35_000_000
OUT = "/home/jan/claude/bloodbowl/diag_policy_vs_policy_20260805/results.json"


def md5(path):
    return hashlib.md5(open(path, 'rb').read()).hexdigest()


def game(args):
    seed_idx, cand_home = args
    import bb_engine
    ra = RACES[seed_idx % len(RACES)]
    rb = RACES[(seed_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    home_pol = POL_CAND if cand_home else POL_BASE
    away_pol = POL_BASE if cand_home else POL_CAND
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=SEED_BASE + seed_idx, mcts_iterations=MCTS,
        weights_path=BEST, away_weights_path=BEST,
        epsilon=0.0, vf_blend=VFB,
        policy_weights_path=home_pol, policy_blend=BLEND,
        away_policy_weights_path=away_pol, away_policy_blend=BLEND)
    hs, as_ = lgr.result.home_score, lgr.result.away_score
    cs, os_ = (hs, as_) if cand_home else (as_, hs)
    return {"seed_idx": seed_idx, "cand_home": cand_home,
            "race_h": ra, "race_a": rb, "cand": cs, "base": os_}


def wilson(w, n, z=1.96):
    if n == 0:
        return (0.0, 0.0, 1.0)
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * ((p * (1 - p) / n + z * z / (4 * n * n)) ** 0.5) / d
    return (p, max(0.0, c - h), min(1.0, c + h))


def stats(rows):
    w = sum(1 for r in rows if r["cand"] > r["base"])
    l = sum(1 for r in rows if r["cand"] < r["base"])
    d = len(rows) - w - l
    dec = w + l
    p, lo, hi = wilson(w, dec)
    sigma = 0.5 / math.sqrt(dec) if dec else float('inf')
    z = (p - 0.5) / sigma if dec else 0.0
    return {"n": len(rows), "W": w, "D": d, "L": l, "decisive": dec,
            "decisive_wr": round(p, 4), "wilson95": [round(lo, 4), round(hi, 4)],
            "z": round(z, 3), "sigma_pp": round(sigma * 100, 2),
            "thr_hi": round(0.5 + 1.645 * sigma, 4) if dec else None,
            "thr_lo": round(0.5 - 1.645 * sigma, 4) if dec else None,
            "chess_score": round((w + 0.5 * d) / len(rows), 4) if rows else None,
            "draw_rate": round(d / len(rows), 4) if rows else None}


def summarize(rows):
    home = [r for r in rows if r["cand_home"]]
    away = [r for r in rows if not r["cand_home"]]
    s = {"all": stats(rows), "cand_home": stats(home), "cand_away": stats(away),
         "per_race_cand": {ra: stats([r for r in rows
                                      if (r["race_h"] if r["cand_home"]
                                          else r["race_a"]) == ra])
                           for ra in RACES}}
    flags = []
    a = s["all"]
    if a["n"] >= 200:
        if not (0.45 <= (1 - a["draw_rate"]) <= 0.75):
            flags.append("decisive_rate_out_of_band")
        if (s["cand_home"]["decisive"] >= 30 and s["cand_away"]["decisive"] >= 30
                and abs(s["cand_home"]["decisive_wr"]
                        - s["cand_away"]["decisive_wr"]) > 0.15):
            flags.append("home_away_divergence_gt_15pp")
    s["sanity_flags"] = flags
    return s


if __name__ == "__main__":
    guard = {"weights_best_md5_pre": md5(BEST),
             "pol_cand": POL_CAND, "pol_cand_md5": md5(POL_CAND),
             "pol_base": POL_BASE, "pol_base_md5": md5(POL_BASE),
             "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    assert guard["pol_cand_md5"] == "fa7698b80a006ecc033e01baedb4e0e7"
    assert guard["pol_base_md5"] == "cd72ed6b4f52f6d6633a0c938c392d97"
    assert guard["weights_best_md5_pre"] == "17578260a77dab8f8b901bef32e2e0c4"
    tasks = [(i, ch) for i in range(N_PAIRS) for ch in (True, False)]
    out, t0 = [], time.time()
    with Pool(2) as pool:
        for r in pool.imap_unordered(game, tasks):
            out.append(r)
            if len(out) % 20 == 0 or len(out) == len(tasks):
                s = summarize(out)
                json.dump({"done": len(out), "total": len(tasks),
                           "blend": BLEND, "guard": guard, "summary": s,
                           "games": out}, open(OUT, "w"))
                a = s["all"]
                print(f"{len(out)}/{len(tasks)} ({time.time()-t0:.0f}s): "
                      f"{a['W']}W {a['D']}D {a['L']}L wr={a['decisive_wr']:.3f}"
                      f" z={a['z']}", flush=True)
    guard["weights_best_md5_post"] = md5(BEST)
    guard["champion_untouched"] = (guard["weights_best_md5_post"]
                                   == guard["weights_best_md5_pre"])
    guard["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    final = summarize(out)
    json.dump({"done": len(out), "total": len(tasks), "blend": BLEND,
               "guard": guard, "summary": final, "games": out}, open(OUT, "w"))
    a = final["all"]
    lo, hi = a["wilson95"]
    verdict = ("SUM_noise" if lo <= 0.5 <= hi else
               ("CAND_better" if a["decisive_wr"] > 0.5 else "CAND_worse"))
    print(f"FINAL VERDICT: {verdict} | wr={a['decisive_wr']:.4f} "
          f"CI=[{lo:.4f},{hi:.4f}] z={a['z']} "
          f"champion_untouched={guard['champion_untouched']}", flush=True)
