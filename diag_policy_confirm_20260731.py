"""Krok 1 (design 31.07.): POTVRZOVACI fairtest policy blendu 0.2, N=1600.

Pre-registrace: evidence/fable_policy_activation_design_20260731.md §3.
Presna replika diag_policy_fairtest_20260730.py (A2-1), jedine rameno
B=0.2, 800 side-swapped seed paru = 1600 her, SEED_BASE=31M (disjunktni
od A2-1 30M). Testuje POST-SERIE snapshot policy (cesta = CLI argument,
ne zivy weights_policy.json). Sampion pouze cten, md5 guard pred/po.

Rozhodovaci kriteria (pre-registrovana, nemenit za behu):
- primarni: decisive WR po 1600 hrach; uspech <=> WR >= 0.5 + 1.645*sigma
  (sigma = 0.5/sqrt(decisive); pri ~960 dec. prah ~52.65 %)
- interim po 800 hrach (jediny, Haybittle-Peto): stop marnost WR <= 50.0 %;
  stop uspech z >= 2.8; jinak dobehnout plne N (finalni prah beze zmeny)
- sanity guardy: decisive rate mimo (45 %, 75 %) nebo |WR_home - WR_away|
  > 15 pp na interim -> flag do results, prosetrit pred interpretaci

Spusteni: python3 diag_policy_confirm_20260731.py <policy_snapshot.json>
"""
import hashlib
import json
import math
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, 'engine/build')

BEST = "weights_best.json"
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
BLEND = 0.2
N_PAIRS = 800         # x2 orientace = 1600 her
MCTS, TV, VFB = 100, 1200, 0.15
SEED_BASE = 31_000_000
INTERIM_AT = 800      # her; jediny interim pohled (H-P)
OUT = "diag_policy_confirm_20260731_results.json"

POLICY = None         # nastaveno v __main__ z argv, dedi se do workeru (fork)


def md5(path):
    return hashlib.md5(open(path, 'rb').read()).hexdigest()


def game(args):
    seed_idx, cand_home = args
    import bb_engine
    ra = RACES[seed_idx % len(RACES)]
    rb = RACES[(seed_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=SEED_BASE + seed_idx, mcts_iterations=MCTS,
        weights_path=BEST, away_weights_path=BEST,
        epsilon=0.0, vf_blend=VFB,
        policy_weights_path=POLICY,
        policy_blend=BLEND if cand_home else 0.0,
        away_policy_weights_path='',   # away sdili home net -> floors symetricky
        away_policy_blend=0.0 if cand_home else BLEND)
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
            "threshold_wr": round(0.5 + 1.645 * sigma, 4) if dec else None,
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
    if len(sys.argv) != 2 or not os.path.isfile(sys.argv[1]):
        sys.exit("usage: diag_policy_confirm_20260731.py <policy_snapshot.json>")
    POLICY = sys.argv[1]
    guard = {"weights_best_md5_pre": md5(BEST),
             "weights_best_meta_md5_pre": md5("weights_best_meta.json"),
             "policy_snapshot": POLICY, "policy_snapshot_md5": md5(POLICY),
             "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    tasks = [(i, ch) for i in range(N_PAIRS) for ch in (True, False)]
    out, t0, interim, stop_reason = [], time.time(), None, None
    with Pool(6) as pool:
        it = pool.imap_unordered(game, tasks)
        for r in it:
            out.append(r)
            s = summarize(out)
            json.dump({"done": len(out), "total": len(tasks), "blend": BLEND,
                       "guard": guard, "interim": interim,
                       "stop_reason": stop_reason, "summary": s, "games": out},
                      open(OUT, "w"))
            if len(out) % 50 == 0:
                a = s["all"]
                print(f"{len(out)}/{len(tasks)} ({time.time()-t0:.0f}s): "
                      f"{a['W']}W {a['D']}D {a['L']}L wr={a['decisive_wr']:.3f} "
                      f"z={a['z']}", flush=True)
            if len(out) == INTERIM_AT and interim is None:
                a = s["all"]
                interim = {"at": INTERIM_AT, "wr": a["decisive_wr"], "z": a["z"]}
                if a["decisive_wr"] <= 0.50:
                    stop_reason = "interim_futility_wr_le_50"
                elif a["z"] >= 2.8:
                    stop_reason = "interim_efficacy_z_ge_2.8"
                if stop_reason:
                    print(f"INTERIM STOP: {stop_reason} (wr={a['decisive_wr']:.3f}"
                          f" z={a['z']})", flush=True)
                    pool.terminate()
                    break
                print(f"INTERIM: pokracovat (wr={a['decisive_wr']:.3f} "
                      f"z={a['z']})", flush=True)
    guard["weights_best_md5_post"] = md5(BEST)
    guard["champion_untouched"] = (guard["weights_best_md5_post"]
                                   == guard["weights_best_md5_pre"])
    guard["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    final = summarize(out)
    json.dump({"done": len(out), "total": len(tasks), "blend": BLEND,
               "guard": guard, "interim": interim, "stop_reason": stop_reason,
               "summary": final, "games": out}, open(OUT, "w"))
    a = final["all"]
    verdict = ("SUCCESS" if a["decisive_wr"] >= (a["threshold_wr"] or 1)
               else ("FAIL_LE_50" if a["decisive_wr"] <= 0.5 else "INCONCLUSIVE"))
    if stop_reason == "interim_futility_wr_le_50":
        verdict = "FAIL_INTERIM_FUTILITY"
    if stop_reason == "interim_efficacy_z_ge_2.8":
        verdict = "SUCCESS_INTERIM"
    print(f"FINAL VERDICT: {verdict} | wr={a['decisive_wr']:.4f} "
          f"prah={a['threshold_wr']} z={a['z']} champion_untouched="
          f"{guard['champion_untouched']}", flush=True)
    print("FINAL:", json.dumps({k: v for k, v in final.items() if k != 'games'},
                               indent=1), flush=True)
