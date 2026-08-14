"""A2-2 (30.07.2026): Is the consistent value-drift direction of July e16
candidates the descent direction of the value loss on the stale February
replay-buffer data?

READ-ONLY wrt production files. Output: diag_drift_alignment_20260730.json.

Replicates the exact production replay-training target and gradient:
  - method mc_shaped (run_iteration.py:114 default; confirmed in
    smoke_item2_mcts400_20260729.log "Method: mc_shaped, gamma=0.99")
  - replay path: training_loop.py:819-821 -> trainer.train_transition_shaped
    (trainer.py:620-645): target = r + gamma*Phi(s') - Phi(s) (non-terminal),
    r - Phi(s) (terminal), Phi = DEFAULT_SHAPING_WEIGHTS dot features
  - loss/backprop: trainer.py:515-541, loss = 0.5*(target - tanh(...))^2
  - 70->73 zero padding: trainer.py:416-425 (_align_features)
"""
import json
import pickle
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "python"))

OUT = ROOT / "diag_drift_alignment_20260730.json"
GAMMA = 0.99
N_FEATURES = 73

# The 10 July-era e16 snapshots (mtime 2026-07-*, format alphazero_neural 73f),
# identical to the crossera A.3 candidate set.
JULY_E16 = [
    "weights_snap_e16_90pct_+3.7.json",   # 07-08
    "weights_snap_e16_90pct_+3.4.json",   # 07-09
    "weights_snap_e16_93pct_+1.6.json",   # 07-13
    "weights_snap_e16_94pct_+2.4.json",   # 07-16
    "weights_snap_e16_94pct_+2.3.json",   # 07-21
    "weights_snap_e16_92pct_+2.2.json",   # 07-22
    "weights_snap_e16_94pct_+2.2.json",   # 07-22
    "weights_snap_e16_93pct_+2.2.json",   # 07-22
    "weights_snap_e16_95pct_+2.2.json",   # 07-23
    "weights_snap_e16_99pct_+2.5.json",   # 07-28
]

# DEFAULT_SHAPING_WEIGHTS, trainer.py (module-level constant)
SHAPING = [
    (1, 3.0), (2, -3.0), (12, 0.5), (14, -0.8), (67, 0.8), (15, -1.5),
    (8, -0.3), (9, 0.3), (34, 0.5), (35, -0.1), (59, 0.8), (42, -0.8),
    (40, -0.6), (63, -0.4), (70, -0.5), (71, -0.3), (72, 0.3),
]


def _save(results: dict) -> None:
    OUT.write_text(json.dumps(results, indent=1))


def load_value(fp: Path):
    d = json.loads(fp.read_text())
    assert d.get("n_features") == N_FEATURES, (fp, d.get("n_features"))
    W1 = np.array(d["value_W1"], dtype=np.float64)
    b1 = np.array(d["value_b1"], dtype=np.float64)
    W2 = np.array(d["value_W2"], dtype=np.float64)
    b2 = np.atleast_1d(np.array(d["value_b2"], dtype=np.float64))
    return W1, b1, W2, b2


def flatten(W1, b1, W2, b2):
    return np.concatenate([W1.ravel(), b1.ravel(), W2.ravel(), b2.ravel()])


def align(f):
    a = np.zeros(N_FEATURES)
    f = np.asarray(f, dtype=np.float64)
    a[: len(f)] = f[:N_FEATURES]
    return a


def phi(f):
    return sum(w * f[i] for i, w in SHAPING if i < len(f))


def grad_one(W1, b1, W2, b2, f, target):
    """trainer.py:515-541 _backprop, gradient of 0.5*(target - y)^2."""
    z1 = f @ W1 + b1
    h = np.maximum(z1, 0.0)
    z2 = h @ W2 + b2
    y = np.tanh(z2)
    dz2 = -(target - y[0]) * (1.0 - y[0] ** 2)
    dW2 = h.reshape(-1, 1) * dz2
    db2 = np.array([dz2])
    dh = W2.flatten() * dz2 * (z1 > 0).astype(np.float64)
    dW1 = f.reshape(-1, 1) @ dh.reshape(1, -1)
    db1 = dh
    return dW1, db1, dW2, db2, abs(float(target - y[0]))


def mean_grad(W1, b1, W2, b2, transitions):
    gW1 = np.zeros_like(W1); gb1 = np.zeros_like(b1)
    gW2 = np.zeros_like(W2); gb2 = np.zeros_like(b2)
    res_sum = 0.0
    n = 0
    for t in transitions:
        f = align(t.features)
        if t.is_terminal:
            target = t.reward - phi(f)
        else:
            nf = align(t.next_features)
            target = t.reward + GAMMA * phi(nf) - phi(f)
        dW1, db1, dW2, db2, res = grad_one(W1, b1, W2, b2, f, target)
        gW1 += dW1; gb1 += db1; gW2 += dW2; gb2 += db2
        res_sum += res
        n += 1
    return (gW1 / n, gb1 / n, gW2 / n, gb2 / n), res_sum / n, n


def cos(a, b):
    na, nb = np.linalg.norm(a), np.linalg.norm(b)
    return float(a @ b / (na * nb)) if na > 0 and nb > 0 else float("nan")


def main():
    results = {"task": "A2-2 drift vs feb-replay gradient alignment",
               "method_replicated": "mc_shaped replay path "
               "(training_loop.py:819-821 -> trainer.py:620-645, backprop trainer.py:515-541)"}

    # 1) champion + drift vectors
    champ = load_value(ROOT / "weights_best.json")
    cvec = flatten(*champ)
    drifts = {}
    for name in JULY_E16:
        v = flatten(*load_value(ROOT / name))
        drifts[name] = v - cvec
    D = np.mean(list(drifts.values()), axis=0)
    dim = len(D)
    results["step1_drift"] = {
        "n_candidates": len(drifts),
        "dim": dim,
        "champion_value_norm": float(np.linalg.norm(cvec)),
        "mean_drift_norm": float(np.linalg.norm(D)),
        "per_candidate_drift_norm": {k: float(np.linalg.norm(v)) for k, v in drifts.items()},
        "per_candidate_cos_to_mean_drift": {k: cos(v, D) for k, v in drifts.items()},
    }
    _save(results)
    print("step1 done: |D| =", np.linalg.norm(D))

    # 2) replay buffer split
    with open(ROOT / "replay_buffer.pkl", "rb") as fh:
        data = pickle.load(fh)
    feb = [t for t in data if len(t.features) == 70]
    fresh = [t for t in data if len(t.features) == 73]
    assert len(feb) + len(fresh) == len(data)
    assert all(t.mc_return is None for t in feb)
    assert all(t.mc_return is not None for t in fresh)
    results["step2_buffer"] = {"n_total": len(data), "n_feb_70f": len(feb),
                               "n_fresh_73f": len(fresh)}
    _save(results)
    print("step2 done:", len(feb), "feb /", len(fresh), "fresh")

    # 3) mean gradients at champion weights
    g_feb, res_feb, n_feb = mean_grad(*champ, feb)
    g_fresh, res_fresh, n_fresh = mean_grad(*champ, fresh)
    gf = flatten(*g_feb)
    gr = flatten(*g_fresh)
    results["step3_gradients"] = {
        "grad_feb_norm": float(np.linalg.norm(gf)),
        "grad_fresh_norm": float(np.linalg.norm(gr)),
        "mean_abs_residual_feb": res_feb,
        "mean_abs_residual_fresh": res_fresh,
        "cos_grad_feb_vs_grad_fresh": cos(gf, gr),
    }
    _save(results)
    print("step3 done: |g_feb| =", np.linalg.norm(gf), "|g_fresh| =", np.linalg.norm(gr))

    # 4) alignment: drift vs descent directions
    desc_feb = -gf
    desc_fresh = -gr
    per_cand = {k: {"cos_desc_feb": cos(v, desc_feb),
                    "cos_desc_fresh": cos(v, desc_fresh)}
                for k, v in drifts.items()}
    random_cos_sd = 1.0 / np.sqrt(dim)
    results["step4_alignment"] = {
        "cos_meanDrift_desc_feb": cos(D, desc_feb),
        "cos_meanDrift_desc_fresh": cos(D, desc_fresh),
        "random_baseline_cos_sd": float(random_cos_sd),
        "per_candidate": per_cand,
    }
    _save(results)
    print("step4 done:", results["step4_alignment"]["cos_meanDrift_desc_feb"],
          "vs", results["step4_alignment"]["cos_meanDrift_desc_fresh"])

    # 5) bonus: how much of |D| does projection on feb-descent explain;
    #    + joint least-squares on both descent directions
    u_feb = desc_feb / np.linalg.norm(desc_feb)
    u_fresh = desc_fresh / np.linalg.norm(desc_fresh)
    proj_feb = float(D @ u_feb)
    proj_fresh = float(D @ u_fresh)
    A = np.stack([desc_feb, desc_fresh], axis=1)
    coef, *_ = np.linalg.lstsq(A, D, rcond=None)
    fit = A @ coef
    resid = D - fit
    # W1 rows 70-72 (features only fresh data can touch)
    W1c = champ[0]
    def w1_rows_slice(vec):
        W1part = vec[: W1c.size].reshape(W1c.shape)
        return W1part
    D_W1 = w1_rows_slice(D)
    results["step5_bonus"] = {
        "proj_len_on_desc_feb": proj_feb,
        "proj_len_on_desc_fresh": proj_fresh,
        "pct_drift_norm_explained_feb": 100.0 * proj_feb / np.linalg.norm(D),
        "pct_drift_norm_explained_fresh": 100.0 * proj_fresh / np.linalg.norm(D),
        "joint_lstsq": {
            "coef_feb": float(coef[0]), "coef_fresh": float(coef[1]),
            "pct_variance_explained": 100.0 * (1.0 - (np.linalg.norm(resid) ** 2)
                                               / (np.linalg.norm(D) ** 2)),
        },
        "drift_norm_W1_rows_70_72": float(np.linalg.norm(D_W1[70:73])),
        "drift_norm_W1_rows_0_69": float(np.linalg.norm(D_W1[:70])),
        "note_rows70_72": "feb transitions are zero-padded -> their gradient on "
                          "W1 rows 70-72 is exactly zero; drift there can only "
                          "come from fresh data",
    }
    _save(results)
    print("step5 done. Written to", OUT)


if __name__ == "__main__":
    main()
