"""Null-test kroku 2 (design 31.07. §4.2): frozen vs frozen, blend na OBOU stranach.

Validuje PRODUKCNI plumbing (run_iteration._gate_game s 11. prvkem policy_cfg),
ne repliku: sampion hraje sam proti sobe, obe strany blend 0.2 nad shodnou
policy -> ocekavane decisive WR ~50 %. Odchylka |z| > 2 = bug v plumbing
(asymetrie stran), NEaktivovat BB_GATE_POLICY_BLEND v ostrem behu.

Spusteni: python3 diag_null_policy_blend_20260803.py <policy_snapshot.json>
"""
import hashlib
import json
import math
import os
import sys
import time
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from run_iteration import _gate_game, _pool_init

BEST = "weights_best.json"
BLEND = 0.2
N = 300               # her; i%2 = side-swap orientace (cand slot je kosmeticky)
MCTS, TV, VFB = 100, 1200, 0.15
SEED_BASE = 32_000_000
OUT = "diag_null_policy_blend_20260803_results.json"


def md5(path):
    return hashlib.md5(open(path, 'rb').read()).hexdigest()


def wilson(w, n, z=1.96):
    if n == 0:
        return (0.0, 0.0, 1.0)
    p = w / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * ((p * (1 - p) / n + z * z / (4 * n * n)) ** 0.5) / d
    return (p, max(0.0, c - h), min(1.0, c + h))


def summarize(rows):
    w = sum(1 for cs, fs, _ in rows if cs > fs)
    l = sum(1 for cs, fs, _ in rows if cs < fs)
    d = len(rows) - w - l
    dec = w + l
    p, lo, hi = wilson(w, dec)
    sigma = 0.5 / math.sqrt(dec) if dec else float('inf')
    z = (p - 0.5) / sigma if dec else 0.0
    return {"n": len(rows), "W": w, "D": d, "L": l, "decisive": dec,
            "decisive_wr": round(p, 4), "wilson95": [round(lo, 4), round(hi, 4)],
            "z": round(z, 3), "draw_rate": round(d / len(rows), 4) if rows else None,
            "null_ok": bool(dec) and abs(z) <= 2.0}


if __name__ == "__main__":
    if len(sys.argv) != 2 or not os.path.isfile(sys.argv[1]):
        sys.exit("usage: diag_null_policy_blend_20260803.py <policy_snapshot.json>")
    policy = sys.argv[1]
    guard = {"weights_best_md5_pre": md5(BEST),
             "policy_snapshot": policy, "policy_snapshot_md5": md5(policy),
             "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())}
    # Produkcni 11-tuple: gate_path == frozen_path == BEST, obe strany blend
    # nad sdilenou siti (policy_cfg jako v selection H2H).
    tasks = [(SEED_BASE + i, i, BEST, BEST, MCTS, VFB, TV,
              False, policy, i % 2 == 1, (BLEND, '', BLEND))
             for i in range(N)]
    _root = os.path.dirname(os.path.abspath(__file__))
    init_args = (os.path.join(_root, 'engine', 'build'), os.path.join(_root, 'python'))
    out, t0 = [], time.time()
    with Pool(6, initializer=_pool_init, initargs=init_args) as pool:
        for r in pool.imap_unordered(_gate_game, tasks):
            out.append(r)
            s = summarize(out)
            json.dump({"done": len(out), "total": N, "blend": BLEND,
                       "guard": guard, "summary": s, "games": out}, open(OUT, "w"))
            if len(out) % 50 == 0:
                print(f"{len(out)}/{N} ({time.time()-t0:.0f}s): {s['W']}W "
                      f"{s['D']}D {s['L']}L wr={s['decisive_wr']:.3f} z={s['z']}",
                      flush=True)
    guard["weights_best_md5_post"] = md5(BEST)
    guard["champion_untouched"] = (guard["weights_best_md5_post"]
                                   == guard["weights_best_md5_pre"])
    guard["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    s = summarize(out)
    json.dump({"done": len(out), "total": N, "blend": BLEND,
               "guard": guard, "summary": s, "games": out}, open(OUT, "w"))
    print(f"NULL-TEST: wr={s['decisive_wr']:.3f} z={s['z']} "
          f"-> {'OK' if s['null_ok'] else 'FAIL (|z|>2, plumbing bug?)'}", flush=True)
