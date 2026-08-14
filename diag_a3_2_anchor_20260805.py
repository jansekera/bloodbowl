"""A3-2 (05.08.2026): HtH proti PEVNÉ kotvě — třetí pilíř důkazu učení.

Gate je pohyblivá laťka (kandidát vs aktuální šampion) → při reálném postupu
konverguje k ~50 % navždy. Tohle měření dává ABSOLUTNÍ stopu: každý archivní
artefakt hraje proti TÉŽE zmrazené kotvě, takže řada výsledků je srovnatelná
napříč časem.

Kotva: weights_anchor_jul28.json (md5 b426c64d...) = weights_best commitnutý
99a0d1c (22.07.) a nezměněný až do promoce 03.08. — tj. "jul28 kotva" a
"před-promoční šampion" jsou TENTÝŽ soubor (ověřeno git show 05.08.).
Kotva hraje BEZ policy blendu (blend 0.0), ale s prior floors (sdílí síť
subjektu, vzor diag_policy_confirm) — to odpovídá gate konfiguraci její éry
(GATE_USE_POLICY_PRIORS=1, GATE_POLICY_BLEND=0).

Ramena (každé N=300 side-swapped párů = 600 her, disjunktní seed bloky):
  control      kotva vs kotva (null kontrola harnessu, očekávání ~50 %)
  noreset1-4   víkendová no-reset série iter1-4 (blend 0.0 — hrály před promocí)
  champion     aktuální šampion 17578260 S policy snapshotem cd72ed6b, blend 0.2
               (= systém, jak reálně hraje po promoci 03.08.)

Pre-registrované čtení (nemenit za běhu):
- per rameno: decisive WR + Wilson 95% CI; |WR-0.5| uvnitř CI = ŠUM (legitimní).
- control mimo [0.45, 0.55] na >=100 decisive => flag harness_control_off,
  interpretaci ramen odložit.
- trend noreset1->4->champion je DESKRIPTIVNÍ (žádný gate verdikt); monotonie
  se posuzuje až s CI pásy. Šumové dno draw-rate ±8-11pp na N=150 platí.
- champion rameno testuje: "je dnešní hraný systém měřitelně nad 22.07.?"

Spuštění: ./venv/bin/python diag_a3_2_anchor_20260805.py [--pairs N] [--arms a,b]
Výstup: a3_2_run_20260805/results_<arm>.json + progress log na stdout; po
posledním ramenu vypíše "ALL ARMS DONE".
"""
import argparse
import hashlib
import json
import math
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, 'engine/build')

ANCHOR = "weights_anchor_jul28.json"
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
MCTS, TV, VFB = 100, 1200, 0.15
SEED_BASE = 80_500_000          # disjunktní od 30M/31M fairtestů i A/B běhů
OUTDIR = "a3_2_run_20260805"
WORKERS = 6

# (name, subject_weights, subject_policy_path, subject_policy_blend)
ARMS = [
    ("control",  ANCHOR,                       "", 0.0),
    ("noreset1", "weights_noreset_iter1.json", "", 0.0),
    ("noreset2", "weights_noreset_iter2.json", "", 0.0),
    ("noreset3", "weights_noreset_iter3.json", "", 0.0),
    ("noreset4", "weights_noreset_iter4.json", "", 0.0),
    ("champion", "weights_best.json", "weights_best_policy.json", 0.2),
]

_ARM = None   # (arm_idx, subject_w, subject_pol, subject_blend); dědí se forkem


def md5(path):
    return hashlib.md5(open(path, 'rb').read()).hexdigest()


def game(args):
    seed_idx, subj_home = args
    arm_idx, subj_w, subj_pol, subj_blend = _ARM
    import bb_engine
    ra = RACES[seed_idx % len(RACES)]
    rb = RACES[(seed_idx + 1) % len(RACES)]
    hr = bb_engine.get_developed_roster(ra, TV)
    ar = bb_engine.get_developed_roster(rb, TV)
    # Prior floors na obou stranách: policy síť se vždy načte na home slotu a
    # away ji sdílí (path ''), obsah čte jen strana s blend > 0 — vzor
    # diag_policy_confirm_20260731.py. Subjektová policy síť: explicitní
    # snapshot (champion), jinak subjektův vlastní kombinovaný soubor.
    pol = subj_pol or subj_w
    home_w, away_w = (subj_w, ANCHOR) if subj_home else (ANCHOR, subj_w)
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=SEED_BASE + arm_idx * 100_000 + seed_idx,
        mcts_iterations=MCTS,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=VFB,
        policy_weights_path=pol,
        policy_blend=subj_blend if subj_home else 0.0,
        away_policy_weights_path='',
        away_policy_blend=0.0 if subj_home else subj_blend)
    hs, as_ = lgr.result.home_score, lgr.result.away_score
    cs, os_ = (hs, as_) if subj_home else (as_, hs)
    return {"seed_idx": seed_idx, "subj_home": subj_home,
            "race_h": ra, "race_a": rb, "subj": cs, "anchor": os_}


def wilson(w, n, z=1.96):
    if n == 0:
        return (0.0, 0.0, 1.0)
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * ((p * (1 - p) / n + z * z / (4 * n * n)) ** 0.5) / d
    return (p, max(0.0, c - h), min(1.0, c + h))


def stats(rows):
    w = sum(1 for r in rows if r["subj"] > r["anchor"])
    l = sum(1 for r in rows if r["subj"] < r["anchor"])
    d = len(rows) - w - l
    dec = w + l
    p, lo, hi = wilson(w, dec)
    sigma = 0.5 / math.sqrt(dec) if dec else float('inf')
    return {"n": len(rows), "W": w, "D": d, "L": l, "decisive": dec,
            "decisive_wr": round(p, 4), "wilson95": [round(lo, 4), round(hi, 4)],
            "z": round((p - 0.5) / sigma, 3) if dec else 0.0,
            "chess_score": round((w + 0.5 * d) / len(rows), 4) if rows else None,
            "draw_rate": round(d / len(rows), 4) if rows else None}


def summarize(rows, arm_name):
    home = [r for r in rows if r["subj_home"]]
    away = [r for r in rows if not r["subj_home"]]
    s = {"arm": arm_name, "all": stats(rows),
         "subj_home": stats(home), "subj_away": stats(away),
         "per_race_subj": {ra: stats([r for r in rows
                                      if (r["race_h"] if r["subj_home"]
                                          else r["race_a"]) == ra])
                           for ra in RACES}}
    flags = []
    a = s["all"]
    if arm_name == "control" and a["decisive"] >= 100 \
            and not (0.45 <= a["decisive_wr"] <= 0.55):
        flags.append("harness_control_off")
    if (s["subj_home"]["decisive"] >= 30 and s["subj_away"]["decisive"] >= 30
            and abs(s["subj_home"]["decisive_wr"]
                    - s["subj_away"]["decisive_wr"]) > 0.15):
        flags.append("home_away_divergence_gt_15pp")
    s["sanity_flags"] = flags
    return s


def _init_arm(arm):
    global _ARM
    _ARM = arm


def run_arm(arm_idx, name, subj_w, subj_pol, subj_blend, n_pairs):
    out_path = os.path.join(OUTDIR, f"results_{name}.json")
    guard = {"anchor_md5": md5(ANCHOR), "subject": subj_w,
             "subject_md5": md5(subj_w),
             "subject_policy": subj_pol or None,
             "subject_policy_md5": md5(subj_pol) if subj_pol else None,
             "subject_policy_blend": subj_blend,
             "champion_md5_pre": md5("weights_best.json"),
             "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    tasks = [(i, sh) for i in range(n_pairs) for sh in (True, False)]
    out, t0 = [], time.time()
    arm = (arm_idx, subj_w, subj_pol, subj_blend)
    with Pool(WORKERS, initializer=_init_arm, initargs=(arm,)) as pool:
        for r in pool.imap_unordered(game, tasks):
            out.append(r)
            if len(out) % 50 == 0 or len(out) == len(tasks):
                s = summarize(out, name)
                json.dump({"done": len(out), "total": len(tasks),
                           "guard": guard, "summary": s, "games": out},
                          open(out_path, "w"))
                a = s["all"]
                print(f"[{name}] {len(out)}/{len(tasks)} "
                      f"({time.time()-t0:.0f}s): {a['W']}W {a['D']}D {a['L']}L "
                      f"wr={a['decisive_wr']:.3f} z={a['z']}", flush=True)
    guard["champion_md5_post"] = md5("weights_best.json")
    guard["champion_untouched"] = (guard["champion_md5_post"]
                                   == guard["champion_md5_pre"])
    guard["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    final = summarize(out, name)
    json.dump({"done": len(out), "total": len(tasks),
               "guard": guard, "summary": final, "games": out},
              open(out_path, "w"))
    a = final["all"]
    print(f"ARM {name} DONE: {a['W']}W {a['D']}D {a['L']}L "
          f"wr={a['decisive_wr']:.4f} CI={a['wilson95']} "
          f"flags={final['sanity_flags']} "
          f"champion_untouched={guard['champion_untouched']}", flush=True)
    return final


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--pairs", type=int, default=300)
    ap.add_argument("--arms", default=None,
                    help="čárkou oddělená jména ramen (default: všechna)")
    args = ap.parse_args()
    os.makedirs(OUTDIR, exist_ok=True)
    want = set(args.arms.split(",")) if args.arms else None
    if md5(ANCHOR) != "b426c64d55c172fe16e273928716b1ce":
        sys.exit(f"ABORT: kotva {ANCHOR} má nečekané md5 — extrahovat znovu "
                 f"z gitu: git show 99a0d1c:weights_best.json")
    finals = []
    for i, (name, w, pol, blend) in enumerate(ARMS):
        if want and name not in want:
            continue
        if not os.path.isfile(w):
            print(f"SKIP {name}: {w} neexistuje", flush=True)
            continue
        finals.append(run_arm(i, name, w, pol, blend, args.pairs))
    json.dump(finals, open(os.path.join(OUTDIR, "summary_all_arms.json"), "w"),
              indent=1)
    print("ALL ARMS DONE", flush=True)
