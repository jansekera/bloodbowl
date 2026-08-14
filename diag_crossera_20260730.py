"""Cross-era analysis 2026-07-30 (Fable): weight-space telemetry + HtH tournament.

Answers "are we learning in the long arc?" independently of the gate.

READ-ONLY with respect to production files: never writes weights_best.json,
weights_policy.json or any weights_snap_*. Outputs go to
diag_crossera_20260730_*.json.

Subcommands:
    python3 diag_crossera_20260730.py telemetry
        numpy-only weight-space distances of all weights_snap_* vs champion
    python3 diag_crossera_20260730.py compat
        1 quick game per tournament anchor vs champion (loadability check)
    python3 diag_crossera_20260730.py tournament [workers=6]
        round-robin over ANCHORS, 13 seeds x 2 orientations per pair (26
        games/pair, 390 total), gate regime (MCTS=100, vf_blend=0.15,
        epsilon=0, dirichlet_alpha=0.0, exploration_c=1.0, shared
        weights_policy.json prior regime). Incremental, restartable.
    python3 diag_crossera_20260730.py report
        W/D/L matrix, decisive-win-rate Wilson CI, Bradley-Terry ratings

Engine notes (verified in source, 2026-07-30):
  - value_function.cpp evaluate() uses min(numFeatures, inputSize_), and
    features 70-72 were APPENDED (feature_extractor.h fix #3), so old
    70-feature value nets load and play correctly, simply blind to the 3
    newest loose-ball features.
  - simulate_game_logged loads each side's VALUE net from
    weights_path/away_weights_path; the policy net is a single shared
    policy_weights_path whose presence activates the hand-coded prior-floor
    regime (policy_blend=0 => learned policy content unused). The tournament
    therefore compares VALUE networks under a fixed shared prior regime --
    same as the production gate.
"""
import json
import math
import os
import sys
import time
from datetime import datetime
from glob import glob
from multiprocessing import Pool
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CHAMPION = str(ROOT / "weights_best.json")
POLICY_PATH = str(ROOT / "weights_policy.json")
RACES = ["human", "orc", "skaven", "dwarf", "wood-elf"]
TV = 1200
MCTS = 100
VF_BLEND = 0.15
BASE_SEED = 20260730
SEEDS_PER_PAIR = 13          # x2 orientations = 26 games per pair

TELEMETRY_OUT = ROOT / "diag_crossera_20260730_telemetry.json"
RESULTS_OUT = ROOT / "diag_crossera_20260730_results.json"

# Tournament anchors (era name -> file), chronological by content era.
ANCHORS = {
    "may06": "weights_snap_e8_83pct_+1.0.json",       # oldest snapshot, 70f value-only
    "jun19": "weights_snap_e8_90pct_+1.3.json",       # 70f value + 93x64 policy head
    "champion": "weights_best.json",                   # content 2026-06-29, 73f
    "jul21": "weights_snap_e16_94pct_+2.3.json",      # 73f
    "jul28": "weights_snap_e16_99pct_+2.5.json",      # item1 e16 sibling, 73f
    "jul29": "weights_snap_e2_100pct_+2.9.json",      # item2 smoke e2, 73f
}
ANCHOR_ORDER = ["may06", "jun19", "champion", "jul21", "jul28", "jul29"]


# ---------------------------------------------------------------- telemetry

def _load(fp):
    with open(fp) as f:
        return json.load(f)


def _value_keys(d):
    """Return (W1,b1,W2,b2) key names for the neural value net, or None if the
    file is not a neural value net (alphazero_linear / plain array)."""
    if not isinstance(d, dict):
        return None
    if "value_W1" in d:
        return ("value_W1", "value_b1", "value_W2", "value_b2")
    if "W1" in d:
        return ("W1", "b1", "W2", "b2")
    return None


def _value_vec(d, rows=None):
    """Flatten value part; rows=N restricts W1 to first N feature rows."""
    kW1, kb1, kW2, kb2 = _value_keys(d)
    W1 = np.asarray(d[kW1], dtype=np.float64)
    if rows is not None:
        W1 = W1[:rows]
    parts = [W1.ravel(),
             np.asarray(d[kb1], dtype=np.float64).ravel(),
             np.asarray(d[kW2], dtype=np.float64).ravel(),
             np.atleast_1d(np.asarray(d[kb2], dtype=np.float64)).ravel()]
    return np.concatenate(parts)


def _policy_vec(d):
    """Flatten neural policy head; None if absent/legacy-linear."""
    if "policy_W1" not in d:
        return None
    W1 = np.asarray(d["policy_W1"], dtype=np.float64).ravel()
    parts = [W1,
             np.asarray(d["policy_b1"], dtype=np.float64).ravel(),
             np.asarray(d["policy_W2"], dtype=np.float64).ravel(),
             np.atleast_1d(np.asarray(d["policy_b2"], dtype=np.float64)).ravel()]
    return np.concatenate(parts)


def _cos(a, b):
    return float(np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b)))


def cmd_telemetry():
    champ = _load(CHAMPION)
    cv_full = _value_vec(champ)                 # 73 rows
    cv_70 = _value_vec(champ, rows=70)
    cp = _policy_vec(champ)                     # 6144-input head (96x64)
    champ_pol_len = len(np.asarray(champ["policy_W1"]).ravel())
    print(f"champion: |value|={np.linalg.norm(cv_full):.4f} "
          f"|value70|={np.linalg.norm(cv_70):.4f} |policy|={np.linalg.norm(cp):.4f} "
          f"policy_W1 len={champ_pol_len} hidden={champ['policy_hidden_size']}")

    rows = []
    excluded = []
    files = sorted(glob(str(ROOT / "weights_snap_*.json")))
    files += sorted(glob(str(ROOT / "champion_backup_*/weights_best.json")))
    for fp in files:
        st = os.stat(fp)
        d = _load(fp)
        mtime = datetime.fromtimestamp(st.st_mtime).strftime("%Y-%m-%d %H:%M")
        if _value_keys(d) is None:
            # alphazero_linear / plain-array: no neural value net -> no
            # meaningful weight-space comparison with the neural champion
            typ = d.get("type", "array") if isinstance(d, dict) else "array"
            excluded.append({"file": os.path.relpath(fp, ROOT), "mtime": mtime,
                             "type": typ, "bytes": st.st_size})
            continue
        nfeat = int(d.get("n_features", len(d[_value_keys(d)[0]])))
        hid = int(d.get("hidden_size", 0))
        if hid != int(champ["hidden_size"]):
            excluded.append({"file": os.path.relpath(fp, ROOT), "mtime": mtime,
                             "type": f"neural_hidden{hid}", "bytes": st.st_size})
            continue
        # policy format classification
        if "policy_W1" in d:
            plen = len(np.asarray(d["policy_W1"]).ravel())
            phid = int(d.get("policy_hidden_size", 0))
            pol_fmt = f"neural_{plen // phid}x{phid}"
        elif "policy_weights" in d:
            pol_fmt = f"linear_{len(d['policy_weights'])}"
        else:
            pol_fmt = "none"

        rec = {
            "file": os.path.relpath(fp, ROOT),
            "mtime": mtime,
            "n_features": nfeat,
            "policy_fmt": pol_fmt,
            "value_norm": float(np.linalg.norm(_value_vec(d))),
        }
        # value: exact when 73f, always the 70-row overlap
        if nfeat == len(champ["value_W1"]):
            v = _value_vec(d)
            rec["value_L2"] = float(np.linalg.norm(v - cv_full))
            rec["value_cos"] = _cos(v, cv_full)
        v70 = _value_vec(d, rows=70)
        rec["value70_L2"] = float(np.linalg.norm(v70 - cv_70))
        rec["value70_cos"] = _cos(v70, cv_70)
        # policy: exact-shape only
        pv = _policy_vec(d)
        if pv is not None and len(pv) == len(cp):
            rec["policy_L2"] = float(np.linalg.norm(pv - cp))
            rec["policy_cos"] = _cos(pv, cp)
        rows.append(rec)

    rows.sort(key=lambda r: r["mtime"])

    # drift-direction analysis among fully compatible (73f) snapshots:
    # pairwise cosine of (snap - champion) difference vectors
    comp = [r for r in rows if r.get("value_L2", 0) > 1e-9
            and "weights_snap" in r["file"]]
    dv = {r["file"]: _value_vec(_load(ROOT / r["file"])) - cv_full for r in comp}
    dp = {r["file"]: _policy_vec(_load(ROOT / r["file"])) - cp
          for r in comp if r.get("policy_L2", 0) > 1e-9}

    def _pairwise_cos(dd):
        ks = list(dd)
        out = []
        for i in range(len(ks)):
            for j in range(i + 1, len(ks)):
                out.append(_cos(dd[ks[i]], dd[ks[j]]))
        return out

    pcv = _pairwise_cos(dv)
    pcp = _pairwise_cos(dp)
    drift = {
        "n_compatible_73f_snaps": len(comp),
        "value_diff_pairwise_cos_mean": float(np.mean(pcv)) if pcv else None,
        "value_diff_pairwise_cos_median": float(np.median(pcv)) if pcv else None,
        "policy_diff_pairwise_cos_mean": float(np.mean(pcp)) if pcp else None,
        "policy_diff_pairwise_cos_median": float(np.median(pcp)) if pcp else None,
    }

    # anchor pairwise distances (value70 space => all anchors comparable)
    anchor_pw = {}
    avecs = {k: _value_vec(_load(ROOT / v), rows=70) for k, v in ANCHORS.items()}
    for i, a in enumerate(ANCHOR_ORDER):
        for b in ANCHOR_ORDER[i + 1:]:
            anchor_pw[f"{a}|{b}"] = {
                "L2": float(np.linalg.norm(avecs[a] - avecs[b])),
                "cos": _cos(avecs[a], avecs[b]),
            }

    out = {"champion_value_norm": float(np.linalg.norm(cv_full)),
           "champion_policy_norm": float(np.linalg.norm(cp)),
           "snapshots": rows, "excluded": excluded, "drift": drift,
           "anchor_pairwise_value70": anchor_pw}
    TELEMETRY_OUT.write_text(json.dumps(out, indent=1))
    print(f"wrote {TELEMETRY_OUT} ({len(rows)} snapshots, "
          f"{len(excluded)} excluded non-neural)")
    print("drift:", json.dumps(drift, indent=1))


# --------------------------------------------------------------- tournament

def _game_worker(task):
    """task = (a_name, b_name, seed_idx, a_is_home). Returns result record.
    Candidate-first scores regardless of orientation (gate side-swap pattern)."""
    a_name, b_name, seed_idx, a_is_home = task
    import bb_engine
    seed = BASE_SEED + seed_idx
    ri = seed_idx % len(RACES)
    hr = bb_engine.get_developed_roster(RACES[ri], TV)
    ar = bb_engine.get_developed_roster(RACES[(ri + 1) % len(RACES)], TV)
    a_path, b_path = str(ROOT / ANCHORS[a_name]), str(ROOT / ANCHORS[b_name])
    home_w, away_w = (a_path, b_path) if a_is_home else (b_path, a_path)
    t0 = time.time()
    res = bb_engine.simulate_game_logged(
        hr, ar, home_ai="macro_mcts", away_ai="macro_mcts",
        seed=seed, mcts_iterations=MCTS,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=VF_BLEND,
        policy_weights_path=POLICY_PATH,
        dirichlet_alpha=0.0, exploration_c=1.0,
    ).result
    a_s, b_s = ((res.home_score, res.away_score) if a_is_home
                else (res.away_score, res.home_score))
    return {"a": a_name, "b": b_name, "seed_idx": seed_idx,
            "a_home": bool(a_is_home), "a_score": int(a_s), "b_score": int(b_s),
            "secs": round(time.time() - t0, 1)}


def _all_tasks():
    tasks = []
    for i, a in enumerate(ANCHOR_ORDER):
        for b in ANCHOR_ORDER[i + 1:]:
            for s in range(SEEDS_PER_PAIR):
                tasks.append((a, b, s, True))
                tasks.append((a, b, s, False))
    return tasks


def _key(t):
    return f"{t[0]}|{t[1]}|{t[2]}|{int(t[3])}"


def _load_results():
    if RESULTS_OUT.exists():
        return json.loads(RESULTS_OUT.read_text())
    return {}


def cmd_compat():
    import bb_engine
    print(f"NUM_FEATURES={bb_engine.NUM_FEATURES}")
    for name in ANCHOR_ORDER:
        if name == "champion":
            continue
        t0 = time.time()
        r = _game_worker((name, "champion", 0, True))
        print(f"{name:9s} vs champion: {r['a_score']}-{r['b_score']} "
              f"OK ({time.time() - t0:.0f}s)  [{ANCHORS[name]}]")


def cmd_tournament(workers=6):
    done = _load_results()
    tasks = [t for t in _all_tasks() if _key(t) not in done]
    print(f"{len(done)} games already done, {len(tasks)} remaining, "
          f"workers={workers}")
    t0 = time.time()
    n = 0
    with Pool(workers) as pool:
        for rec in pool.imap_unordered(_game_worker, tasks):
            k = f"{rec['a']}|{rec['b']}|{rec['seed_idx']}|{int(rec['a_home'])}"
            done[k] = rec
            n += 1
            # incremental save every game (cheap, ~400 records)
            tmp = RESULTS_OUT.with_suffix(".tmp")
            tmp.write_text(json.dumps(done))
            tmp.replace(RESULTS_OUT)
            el = time.time() - t0
            eta = el / n * (len(tasks) - n)
            print(f"[{n}/{len(tasks)}] {rec['a']} vs {rec['b']} "
                  f"{rec['a_score']}-{rec['b_score']} home={rec['a_home']} "
                  f"({rec['secs']}s game, {el/60:.0f}m elapsed, ETA {eta/60:.0f}m)",
                  flush=True)
    print("tournament complete")


# ------------------------------------------------------------------- report

def _wilson(k, n, z=1.96):
    if n == 0:
        return (0.0, 1.0)
    p = k / n
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return ((c - h) / d, (c + h) / d)


def cmd_report():
    done = _load_results()
    print(f"{len(done)} games")
    names = ANCHOR_ORDER
    # pair stats: from a's perspective, a earlier-era than b by construction
    pair = {}
    for rec in done.values():
        key = (rec["a"], rec["b"])
        w, d, l = pair.get(key, (0, 0, 0))
        if rec["a_score"] > rec["b_score"]:
            w += 1
        elif rec["a_score"] == rec["b_score"]:
            d += 1
        else:
            l += 1
        pair[key] = (w, d, l)

    print("\nMatrix (row vs col: W-D-L, decisive winrate [95% CI]):")
    for (a, b), (w, d, l) in sorted(pair.items(),
                                    key=lambda kv: (names.index(kv[0][0]),
                                                    names.index(kv[0][1]))):
        dec = w + l
        p = w / dec if dec else float("nan")
        lo, hi = _wilson(w, dec)
        print(f"  {a:9s} vs {b:9s}: {w}-{d}-{l}  dec_wr={p:.2f} [{lo:.2f},{hi:.2f}]")

    # Bradley-Terry with draws = half win, MM algorithm
    wins = {(a, b): 0.0 for a in names for b in names if a != b}
    for (a, b), (w, d, l) in pair.items():
        wins[(a, b)] += w + d / 2
        wins[(b, a)] += l + d / 2
    gamma = {n: 1.0 for n in names}
    for _ in range(500):
        new = {}
        for i in names:
            num = sum(wins[(i, j)] for j in names if j != i)
            den = sum((wins[(i, j)] + wins[(j, i)]) / (gamma[i] + gamma[j])
                      for j in names if j != i)
            new[i] = num / den if den else gamma[i]
        s = sum(new.values())
        gamma = {k: v * len(names) / s for k, v in new.items()}
    elo = {k: 400 * math.log10(v / gamma["champion"]) for k, v in gamma.items()}
    print("\nBradley-Terry rating (Elo scale, champion=0):")
    for k in sorted(names, key=lambda n: -elo[n]):
        tot = [0, 0, 0]
        for (a, b), (w, d, l) in pair.items():
            if a == k:
                tot = [tot[0] + w, tot[1] + d, tot[2] + l]
            elif b == k:
                tot = [tot[0] + l, tot[1] + d, tot[2] + w]
        print(f"  {k:9s} {elo[k]:+7.1f}  (overall {tot[0]}-{tot[1]}-{tot[2]})")


if __name__ == "__main__":
    cmd = sys.argv[1] if len(sys.argv) > 1 else "telemetry"
    if cmd == "telemetry":
        cmd_telemetry()
    elif cmd == "compat":
        cmd_compat()
    elif cmd == "tournament":
        cmd_tournament(int(sys.argv[2]) if len(sys.argv) > 2 else 6)
    elif cmd == "report":
        cmd_report()
    else:
        print(f"unknown command {cmd}")
