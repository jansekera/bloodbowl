"""Offline feature A/B (P0-b): old (73+23) vs new (73+23+16) action features.

Protocol pre-registered in evidence/fable_offline_feature_ab_report_20260810.md §1
BEFORE any of this ran. Mirrors the production NeuralPolicyTrainer
(python/blood_bowl/policy_trainer.py): MLP hidden=64 ReLU, minibatch SGD 32,
lr 0.01, Xavier init, CE on visit fractions, 16 passes. 4-fold CV by game,
3 init seeds per arm; all metrics on out-of-fold predictions, averaged over
seeds. Reads rows_*.jsonl from this directory, writes results_ab.json +
results_ab.out.
"""
import json
import os
import sys

import numpy as np

DIR = os.path.dirname(os.path.abspath(__file__))

# v2 (2026-08-10): candidates arrive SORTED BY VISITS from the harness; with
# tied/flat logits np.argmax breaks ties toward index 0 = search's top pick,
# leaking the label through candidate ORDER (old arm got REPOS hit 0.91 with
# zero spread in v1). Fix: deterministic per-decision shuffle at load time.
SHUFFLE_CANDS = os.environ.get("AB_SHUFFLE", "1") == "1"
OUT_SUFFIX = os.environ.get("AB_SUFFIX", "_v2")
PASSES_ENV = os.environ.get("AB_PASSES")
SEEDS_ENV = os.environ.get("AB_SEEDS")
N_SF = 73
N_F = 23
N_NF = 16
HID = 64
LR = 0.01
BATCH = 32
PASSES = 16
SEEDS = [1, 2, 3]
FOLDS = 4

TYPE_ENDTURN = 9
TYPE_REPOS = 8
TYPE_PICKUP = 5

FILES = ["rows_dwsk_a.jsonl", "rows_dwsk_b.jsonl", "rows_wesk.jsonl"]


def load_rows():
    decs = []
    shuffle_rng = np.random.default_rng(20260810)
    for fn in FILES:
        path = os.path.join(DIR, fn)
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    # tolerate a truncated in-flight tail line; a finished
                    # collection file has only complete lines
                    print(f"WARN: skipping malformed line in {fn}",
                          file=sys.stderr)
                    continue
                if len(d["cands"]) < 2:
                    continue
                game = (d["race_h"], d["seed"])
                race = d["race_h"] if d["persp"] == "home" else d["race_a"]
                sf = np.asarray(d["sf"], dtype=np.float64)
                f23 = np.array([c["f"] for c in d["cands"]], dtype=np.float64)
                nf16 = np.array([c["nf"] for c in d["cands"]], dtype=np.float64)
                v = np.array([c["v"] for c in d["cands"]], dtype=np.float64)
                if v.sum() <= 0:
                    continue
                v = v / v.sum()
                types = np.array([c["t"] for c in d["cands"]], dtype=np.int64)
                if SHUFFLE_CANDS:
                    perm = shuffle_rng.permutation(len(v))
                    f23, nf16, v, types = f23[perm], nf16[perm], v[perm], types[perm]
                decs.append(dict(game=game, race=race, sf=sf, f=f23, nf=nf16,
                                 v=v, types=types))
    return decs


class MLP:
    def __init__(self, n_in, rng):
        self.W1 = rng.standard_normal((n_in, HID)) * np.sqrt(2.0 / n_in)
        self.b1 = np.zeros(HID)
        self.W2 = rng.standard_normal(HID) * np.sqrt(2.0 / HID)
        self.b2 = 0.0

    def forward(self, X):
        z1 = X @ self.W1 + self.b1
        h1 = np.maximum(0, z1)
        logits = h1 @ self.W2 + self.b2
        e = np.exp(logits - logits.max())
        return z1, h1, e / e.sum()

    def probs(self, X):
        return self.forward(X)[2]


def build_inputs(dec, arm):
    n = len(dec["v"])
    if arm == "old":
        X = np.zeros((n, N_SF + N_F))
        X[:, :N_SF] = dec["sf"]
        X[:, N_SF:] = dec["f"]
    else:
        X = np.zeros((n, N_SF + N_F + N_NF))
        X[:, :N_SF] = dec["sf"]
        X[:, N_SF:N_SF + N_F] = dec["f"]
        X[:, N_SF + N_F:] = dec["nf"]
    return X


def train(decs, arm, seed):
    rng = np.random.default_rng(seed)
    n_in = N_SF + N_F + (N_NF if arm == "new" else 0)
    net = MLP(n_in, rng)
    Xs = [build_inputs(d, arm) for d in decs]
    Ts = [d["v"] for d in decs]
    idx = np.arange(len(decs))
    for _ in range(PASSES):
        rng.shuffle(idx)
        dW1 = np.zeros_like(net.W1)
        db1 = np.zeros_like(net.b1)
        dW2 = np.zeros_like(net.W2)
        db2 = 0.0
        bc = 0
        for i in idx:
            X, t = Xs[i], Ts[i]
            z1, h1, p = net.forward(X)
            dl = p - t
            dW2 += h1.T @ dl
            db2 += dl.sum()
            dh = np.outer(dl, net.W2) * (z1 > 0)
            dW1 += X.T @ dh
            db1 += dh.sum(axis=0)
            bc += 1
            if bc >= BATCH:
                s = 1.0 / bc
                net.W1 -= LR * dW1 * s
                net.b1 -= LR * db1 * s
                net.W2 -= LR * dW2 * s
                net.b2 -= LR * db2 * s
                dW1[:] = 0
                db1[:] = 0
                dW2[:] = 0
                db2 = 0.0
                bc = 0
        if bc:
            s = 1.0 / bc
            net.W1 -= LR * dW1 * s
            net.b1 -= LR * db1 * s
            net.W2 -= LR * dW2 * s
            net.b2 -= LR * db2 * s
    return net


def spearman_perm(x, y, rng, n_perm=10000):
    def rank(a):
        order = np.argsort(a)
        r = np.empty_like(order, dtype=np.float64)
        r[order] = np.arange(len(a))
        # average ties
        vals, inv, cnt = np.unique(a, return_inverse=True, return_counts=True)
        csum = np.cumsum(cnt) - cnt
        avg = csum + (cnt - 1) / 2.0
        return avg[inv]
    rx, ry = rank(x), rank(y)
    rx = (rx - rx.mean()) / (rx.std() + 1e-12)
    ry = (ry - ry.mean()) / (ry.std() + 1e-12)
    rho = float(np.mean(rx * ry))
    cnt = 0
    for _ in range(n_perm):
        perm = rng.permutation(len(ry))
        if np.mean(rx * ry[perm]) >= rho:
            cnt += 1
    return rho, (cnt + 1) / (n_perm + 1)


def evaluate(decs, oof_probs, label):
    """oof_probs: list of prob arrays (out-of-fold), aligned with decs."""
    out = {}
    # --- aggregate ---
    ce, top1 = [], []
    for d, p in zip(decs, oof_probs):
        ce.append(float(-np.sum(d["v"] * np.log(p + 1e-8))))
        top1.append(int(np.argmax(p) == np.argmax(d["v"])))
    out["n"] = len(decs)
    out["ce"] = float(np.mean(ce))
    out["top1"] = float(np.mean(top1))

    # --- A: END_TURN ranking (search >=50% END_TURN, >=3 cands) ---
    a = {"all": {"n": 0, "top1": 0, "ranks": []},
         "dwarf": {"n": 0, "top1": 0, "ranks": []}}
    for d, p in zip(decs, oof_probs):
        iv = int(np.argmax(d["v"]))
        if d["types"][iv] != TYPE_ENDTURN or d["v"][iv] < 0.5 or len(d["v"]) < 3:
            continue
        rank = int(np.sum(p > p[iv])) + 1  # 1-based
        for key in (["all", "dwarf"] if d["race"] == "dwarf" else ["all"]):
            a[key]["n"] += 1
            a[key]["top1"] += int(rank == 1)
            a[key]["ranks"].append(rank)
    for key in a:
        g = a[key]
        g["top1_share"] = g["top1"] / g["n"] if g["n"] else None
        g["median_rank"] = float(np.median(g["ranks"])) if g["ranks"] else None
        del g["ranks"]
    out["A_endturn"] = a

    # --- B: REPOS spread + directional validity (>=3 REPOS cands) ---
    b = {}
    for scope in ["dwarf", "all"]:
        spreads, hits, chance = [], [], []
        hits_maxset, chance_maxset = [], []
        for d, p in zip(decs, oof_probs):
            if scope == "dwarf" and d["race"] != "dwarf":
                continue
            ridx = np.where(d["types"] == TYPE_REPOS)[0]
            if len(ridx) < 3:
                continue
            pr = p[ridx]
            spreads.append(float(pr.max() - pr.min()))
            vi = d["v"][ridx]
            hits.append(int(np.argmax(pr) == np.argmax(vi)))
            chance.append(1.0 / len(ridx))
            # tie-robust variant: does the arm's top REPOS pick land in the
            # search max-visit SET (visit fractions tie often at MCTS-100);
            # chance for a flat/random predictor = m/n
            maxset = vi >= vi.max() - 1e-12
            hits_maxset.append(int(maxset[int(np.argmax(pr))]))
            chance_maxset.append(float(maxset.sum()) / len(ridx))
        b[scope] = dict(n=len(spreads),
                        spread=float(np.mean(spreads)) if spreads else None,
                        hit_rate=float(np.mean(hits)) if hits else None,
                        chance=float(np.mean(chance)) if chance else None,
                        hit_maxset=float(np.mean(hits_maxset)) if hits_maxset else None,
                        chance_maxset=float(np.mean(chance_maxset)) if chance_maxset else None)
        # search visit spread for context
        vspreads = []
        for d in decs:
            if scope == "dwarf" and d["race"] != "dwarf":
                continue
            ridx = np.where(d["types"] == TYPE_REPOS)[0]
            if len(ridx) < 3:
                continue
            vi = d["v"][ridx]
            vspreads.append(float(vi.max() - vi.min()))
        b[scope]["search_spread"] = float(np.mean(vspreads)) if vspreads else None
    out["B_repos"] = b

    # --- C: PICKUP mass delta per race + mechanism corr ---
    c = {}
    per_race = {}
    mech_x, mech_y = [], []
    for d, p in zip(decs, oof_probs):
        loose = d["sf"][14] >= 0.5
        pidx = np.where(d["types"] == TYPE_PICKUP)[0]
        if not loose or len(pidx) == 0:
            continue
        pm = float(np.sum(p[pidx]))
        vm = float(np.sum(d["v"][pidx]))
        per_race.setdefault(d["race"], []).append(pm - vm)
        n_c = len(d["v"])
        for i in pidx:
            mech_x.append(1.0 - d["nf"][i][12])  # 1 - p_fail
            mech_y.append(float(p[i]) * n_c)     # uniform-relative prior
    for r, vals in per_race.items():
        c[f"delta_{r}"] = float(np.mean(vals))
        c[f"n_{r}"] = len(vals)
    rng = np.random.default_rng(12345)
    if len(mech_x) >= 10:
        rho, pval = spearman_perm(np.array(mech_x), np.array(mech_y), rng)
        c["spearman_rho"] = rho
        c["spearman_p"] = pval
        c["n_pickup_cands"] = len(mech_x)
    out["C_pickup"] = c
    return out


def counterfactual_tv(decs, nets_by_fold, fold_of, arm):
    """Dwarf decisions: swap state capability features to elf values
    (indices 19/38/36/48/50, as in diag_analyze.py), TV distance of
    type-mass distribution of priors. Out-of-fold nets."""
    tvs = []
    for k, d in enumerate(decs):
        if d["race"] != "dwarf":
            continue
        net = nets_by_fold[fold_of[k]]
        X = build_inputs(d, arm)
        p0 = net.probs(X)
        d2 = dict(d)
        sf2 = d["sf"].copy()
        sf2[19] = 3.0 / 5
        sf2[38] = 4.0 / 5
        sf2[36] = 7.7 / 10
        sf2[48] = 0.10
        sf2[50] = 0.50
        d2["sf"] = sf2
        p1 = net.probs(build_inputs(d2, arm))
        m0, m1 = {}, {}
        for t, a0, a1 in zip(d["types"], p0, p1):
            m0[t] = m0.get(t, 0) + a0
            m1[t] = m1.get(t, 0) + a1
        tvs.append(0.5 * sum(abs(m1.get(t, 0) - m0.get(t, 0))
                             for t in set(m0) | set(m1)))
    return float(np.mean(tvs)) if tvs else None


def main():
    global PASSES, SEEDS
    if PASSES_ENV:
        PASSES = int(PASSES_ENV)
    if SEEDS_ENV:
        SEEDS = [int(s) for s in SEEDS_ENV.split(",")]
    print(f"config: shuffle={SHUFFLE_CANDS} passes={PASSES} seeds={SEEDS} "
          f"suffix={OUT_SUFFIX}", flush=True)
    decs = load_rows()
    games = sorted(set(d["game"] for d in decs), key=str)
    # stratified round-robin fold assignment per matchup
    fold_map = {}
    by_race = {}
    for g in games:
        by_race.setdefault(g[0], []).append(g)
    for r, gs in by_race.items():
        for i, g in enumerate(gs):
            fold_map[g] = i % FOLDS
    fold_of = [fold_map[d["game"]] for d in decs]
    print(f"loaded {len(decs)} decisions from {len(games)} games; "
          f"folds sizes: {[fold_of.count(k) for k in range(FOLDS)]}", flush=True)

    results = {}
    for arm in ["old", "new"]:
        per_seed = []
        cf_per_seed = []
        for seed in SEEDS:
            oof = [None] * len(decs)
            nets_by_fold = {}
            for fold in range(FOLDS):
                train_decs = [d for d, f in zip(decs, fold_of) if f != fold]
                net = train(train_decs, arm, seed * 100 + fold)
                nets_by_fold[fold] = net
                for k, (d, f) in enumerate(zip(decs, fold_of)):
                    if f == fold:
                        oof[k] = net.probs(build_inputs(d, arm))
                print(f"  arm={arm} seed={seed} fold={fold} trained "
                      f"({len(train_decs)} decs)", flush=True)
            ev = evaluate(decs, oof, f"{arm}-s{seed}")
            ev["counterfactual_tv"] = counterfactual_tv(decs, nets_by_fold,
                                                        fold_of, arm)
            per_seed.append(ev)
            print(f"  arm={arm} seed={seed}: ce={ev['ce']:.4f} "
                  f"top1={ev['top1']:.3f}", flush=True)
        results[arm] = per_seed

    with open(os.path.join(DIR, "results_ab" + OUT_SUFFIX + ".json"), "w") as f:
        json.dump(results, f, indent=1, default=str)

    # --- mean-over-seeds summary ---
    def mean_path(arm, path):
        vals = []
        for ev in results[arm]:
            v = ev
            for k in path:
                v = v[k] if v is not None else None
                if v is None:
                    break
            if v is not None:
                vals.append(v)
        return float(np.mean(vals)) if vals else None

    lines = []
    lines.append(f"decisions={len(decs)} games={len(games)}")
    for arm in ["old", "new"]:
        lines.append(f"\n=== arm {arm} (mean over seeds {SEEDS}) ===")
        lines.append(f"CE={mean_path(arm, ['ce']):.4f} "
                     f"top1={mean_path(arm, ['top1']):.3f} "
                     f"cfTV={mean_path(arm, ['counterfactual_tv']):.4f}")
        for scope in ["all", "dwarf"]:
            lines.append(f"A END_TURN[{scope}]: n={results[arm][0]['A_endturn'][scope]['n']} "
                         f"top1_share={mean_path(arm, ['A_endturn', scope, 'top1_share'])} "
                         f"median_rank={mean_path(arm, ['A_endturn', scope, 'median_rank'])}")
        for scope in ["dwarf", "all"]:
            lines.append(f"B REPOS[{scope}]: n={results[arm][0]['B_repos'][scope]['n']} "
                         f"spread={mean_path(arm, ['B_repos', scope, 'spread'])} "
                         f"hit={mean_path(arm, ['B_repos', scope, 'hit_rate'])} "
                         f"chance={mean_path(arm, ['B_repos', scope, 'chance'])} "
                         f"hit_maxset={mean_path(arm, ['B_repos', scope, 'hit_maxset'])} "
                         f"chance_maxset={mean_path(arm, ['B_repos', scope, 'chance_maxset'])} "
                         f"search_spread={mean_path(arm, ['B_repos', scope, 'search_spread'])}")
        ckeys = sorted(results[arm][0]["C_pickup"].keys())
        lines.append("C PICKUP: " + " ".join(
            f"{k}={mean_path(arm, ['C_pickup', k])}" for k in ckeys))
    text = "\n".join(lines)
    print(text)
    with open(os.path.join(DIR, "results_ab" + OUT_SUFFIX + ".out"), "w") as f:
        f.write(text + "\n")


if __name__ == "__main__":
    main()
