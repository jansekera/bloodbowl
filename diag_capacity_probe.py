#!/usr/bin/env python3
"""Capacity-vs-features probe (follow-up to the REFUTED drive-target pre-filter).

Question (2026-07-15): is the drive-target failure a FEATURE ceiling or a
LINEAR-MODEL-CAPACITY ceiling?  I.e. can a genuinely nonlinear function class
on the SAME 73 features recover the within-episode structure that the
drive-level label T carries (label ep-std 0.07-0.16) but the linear head
could not express (fit ep-std 0.210-0.228, below the 0.234 G-only ref)?

Design (all offline, replay_buffer.pkl only, no engine, no production code):
  * Target T = drive-level target at lam=0.5, D=0.6, d0=0.1 -- the single
    best-behaved grid point of diag_drive_target_diff.py (the only combo that
    passed BOTH the divergence threshold mean|dV|>0.1 AND the MSE guard
    <+10%; it failed only on ep-std V, which is exactly the axis under test).
  * Episode-level train/test split (80/20, seeded) -- no transition leakage.
  * Models on identical standardized 73-dim inputs:
      - RidgeCV                (linear; converged least-squares baseline)
      - HistGradientBoosting   (nonlinear, high capacity, no extra deps)
      - MLPRegressor 2 hidden  (nonlinear, smooth function class)
  * Held-out metrics: MSE & R^2 vs T, within-episode std of predictions
    (mean over test episodes with >=3 states, same as diag_drive_target_diff),
    and WITHIN-EPISODE R^2 (both preds and labels centered per episode) --
    the sharpest test of "does the model capture within-game structure or
    just per-episode offsets".
  * Overfit guard: train-side MSE/R^2 reported next to test-side; a nonlinear
    model with high prediction variance but no held-out MSE/R^2 improvement
    is fitting noise, not signal.
  * Side sanity check: feature-vector distinctness within episodes (unique
    rows / length) -- if states literally collapse to identical vectors, no
    function class can separate them.

Caveat on absolute comparability: today's 0.234 / 0.210-0.228 ep-std numbers
came from 3-pass SGD from weights_best evaluated in-sample; here fits are
converged and evaluated out-of-sample.  The verdict therefore rests on the
linear-vs-nonlinear GAP measured on identical footing, not on matching the
earlier absolute numbers (the in-sample ridge ep-std is printed as a bridge).
"""
from __future__ import annotations

import pickle
import sys
from pathlib import Path

import numpy as np

ROOT = Path('/home/jan/claude/bloodbowl')
sys.path.insert(0, str(ROOT / 'python'))

from blood_bowl import replay_buffer as rb  # noqa: E402,F401 (Transition unpickling)

from sklearn.linear_model import RidgeCV  # noqa: E402
from sklearn.ensemble import HistGradientBoostingRegressor  # noqa: E402
from sklearn.neural_network import MLPRegressor  # noqa: E402
from sklearn.preprocessing import StandardScaler  # noqa: E402

GAMMA = 0.99
LAM, D, D0 = 0.5, 0.6, 0.1     # best-behaved grid point of the REFUTED test
SPLIT_SEED = int(sys.argv[1]) if len(sys.argv) > 1 else 20260715
TEST_FRAC = 0.2


def split_episodes(transitions):
    episodes, cur = [], []
    for tr in transitions:
        cur.append(tr)
        if tr.is_terminal:
            episodes.append(cur)
            cur = []
    return episodes


def drive_segments(ep_rewards):
    segs, start = [], 0
    n = len(ep_rewards)
    for i, r in enumerate(ep_rewards):
        if r != 0.0:
            segs.append((start, i, 1 if r > 0 else -1))
            start = i + 1
    if start < n:
        segs.append((start, n - 1, 0))
    return segs


def build_targets(episodes):
    """Return per-episode arrays: features X, G, T (drive target), terminal."""
    out = []
    for ep in episodes:
        X = np.array([tr.features for tr in ep], dtype=float)
        G = np.array([tr.mc_return if tr.mc_return is not None else tr.reward
                      for tr in ep], dtype=float)
        term = np.array([tr.is_terminal for tr in ep])
        r = [tr.reward_step if tr.reward_step is not None else 0.0 for tr in ep]
        T = np.empty_like(G)
        for start, kd, sign in drive_segments(r):
            outcome = sign * D if sign != 0 else -D0
            for t in range(start, kd + 1):
                comp = (GAMMA ** (kd - t)) * outcome
                T[t] = min(1.0, max(-1.0, LAM * G[t] + (1.0 - LAM) * comp))
        T[term] = G[term]
        out.append((X, G, T))
    return out


def ep_pred_std(preds_per_ep):
    stds = [p.std() for p in preds_per_ep if len(p) >= 3]
    return float(np.mean(stds))


def within_ep_r2(labels_per_ep, preds_per_ep):
    """R^2 after centering BOTH label and prediction per episode."""
    num = den = 0.0
    for y, p in zip(labels_per_ep, preds_per_ep):
        if len(y) < 3:
            continue
        yc, pc = y - y.mean(), p - p.mean()
        num += float(((yc - pc) ** 2).sum())
        den += float((yc ** 2).sum())
    return 1.0 - num / den


def within_ep_corr(labels_per_ep, preds_per_ep):
    """Scale-free check: Pearson r of per-episode-centered pred vs label."""
    ys, ps = [], []
    for y, p in zip(labels_per_ep, preds_per_ep):
        if len(y) < 3:
            continue
        ys.append(y - y.mean())
        ps.append(p - p.mean())
    return float(np.corrcoef(np.concatenate(ys), np.concatenate(ps))[0, 1])


def between_ep_r2(labels_per_ep, preds_per_ep):
    """R^2 on per-episode MEANS -- attributes overall gains to the
    between-episode component."""
    ym = np.array([y.mean() for y in labels_per_ep])
    pm = np.array([p.mean() for p in preds_per_ep])
    return 1.0 - float(np.mean((ym - pm) ** 2)) / float(np.var(ym))


def metrics(name, model, Xtr, ytr, Xte, yte, te_slices, te_labels):
    p_tr, p_te = model.predict(Xtr), model.predict(Xte)
    mse_tr = float(np.mean((p_tr - ytr) ** 2))
    mse_te = float(np.mean((p_te - yte) ** 2))
    r2_tr = 1.0 - mse_tr / float(np.var(ytr))
    r2_te = 1.0 - mse_te / float(np.var(yte))
    preds_per_ep = [p_te[a:b] for a, b in te_slices]
    eps = ep_pred_std(preds_per_ep)
    wr2 = within_ep_r2(te_labels, preds_per_ep)
    wcorr = within_ep_corr(te_labels, preds_per_ep)
    br2 = between_ep_r2(te_labels, preds_per_ep)
    print(f'  {name:<22} MSE tr/te {mse_tr:.4f}/{mse_te:.4f}   '
          f'R2 tr/te {r2_tr:+.4f}/{r2_te:+.4f}   '
          f'ep-std(pred) {eps:.4f}   within-ep R2 {wr2:+.4f} '
          f'(corr {wcorr:+.3f})   between-ep R2 {br2:+.4f}')
    return dict(mse_te=mse_te, r2_te=r2_te, ep_std=eps, wr2=wr2,
                wcorr=wcorr, br2=br2)


def main():
    with open(ROOT / 'replay_buffer.pkl', 'rb') as f:
        buf = pickle.load(f)
    episodes = split_episodes(buf)
    data = build_targets(episodes)
    n_ep = len(data)

    rng = np.random.default_rng(SPLIT_SEED)
    perm = rng.permutation(n_ep)
    n_te = int(round(TEST_FRAC * n_ep))
    te_ids = set(perm[:n_te].tolist())
    tr_ids = [i for i in range(n_ep) if i not in te_ids]
    te_ids = [i for i in range(n_ep) if i in te_ids]

    def stack(ids):
        X = np.vstack([data[i][0] for i in ids])
        G = np.concatenate([data[i][1] for i in ids])
        T = np.concatenate([data[i][2] for i in ids])
        slices, k = [], 0
        for i in ids:
            m = len(data[i][1])
            slices.append((k, k + m))
            k += m
        return X, G, T, slices

    Xtr, Gtr, Ttr, tr_sl = stack(tr_ids)
    Xte, Gte, Tte, te_sl = stack(te_ids)
    te_T_labels = [Tte[a:b] for a, b in te_sl]
    te_G_labels = [Gte[a:b] for a, b in te_sl]

    print(f'episodes: {n_ep} (train {len(tr_ids)}, test {len(te_ids)})   '
          f'transitions: train {len(Gtr)}, test {len(Gte)}   '
          f'feature dim: {Xtr.shape[1]}')
    print(f'target T (lam={LAM}, D={D}, d0={D0}):  '
          f'ep-std label T (test) = {ep_pred_std(te_T_labels):.4f}   '
          f'ep-std label G (test) = {ep_pred_std(te_G_labels):.4f}')

    # feature distinctness inside episodes (can ANY function separate states?)
    uniq_fracs, dup_pair = [], 0
    tot_pair = 0
    for X, _, _ in data:
        if len(X) < 3:
            continue
        uniq = len(np.unique(np.round(X, 9), axis=0))
        uniq_fracs.append(uniq / len(X))
        d = np.abs(np.diff(X, axis=0)).max(axis=1)
        dup_pair += int((d < 1e-9).sum())
        tot_pair += len(d)
    print(f'feature distinctness: unique-rows/episode-len mean = '
          f'{np.mean(uniq_fracs):.3f};  consecutive identical-vector pairs = '
          f'{dup_pair}/{tot_pair} ({dup_pair / tot_pair:.1%})')

    scaler = StandardScaler().fit(Xtr)
    Xtr_s, Xte_s = scaler.transform(Xtr), scaler.transform(Xte)

    print('\n=== target T (drive-level) ===')
    ridge = RidgeCV(alphas=np.logspace(-4, 3, 15)).fit(Xtr_s, Ttr)
    print(f'  [ridge alpha={ridge.alpha_:g}]')
    res_lin = metrics('linear (RidgeCV)', ridge, Xtr_s, Ttr, Xte_s, Tte,
                      te_sl, te_T_labels)
    # bridge to today's in-sample numbers: full-data ep-std of the ridge fit
    p_all_tr = [ridge.predict(Xtr_s[a:b]) for a, b in tr_sl]
    print(f'  (bridge: in-sample train ep-std of ridge preds = '
          f'{ep_pred_std(p_all_tr):.4f})')

    gbt = HistGradientBoostingRegressor(
        max_iter=500, learning_rate=0.08, max_depth=None, max_leaf_nodes=63,
        l2_regularization=1e-3, early_stopping=True, validation_fraction=0.15,
        random_state=SPLIT_SEED).fit(Xtr_s, Ttr)
    res_gbt = metrics(f'GBT ({gbt.n_iter_} iters)', gbt, Xtr_s, Ttr, Xte_s,
                      Tte, te_sl, te_T_labels)

    mlp = MLPRegressor(hidden_layer_sizes=(128, 64), activation='relu',
                       alpha=1e-4, max_iter=400, early_stopping=True,
                       n_iter_no_change=20, random_state=SPLIT_SEED
                       ).fit(Xtr_s, Ttr)
    res_mlp = metrics(f'MLP 128-64 ({mlp.n_iter_} ep)', mlp, Xtr_s, Ttr,
                      Xte_s, Tte, te_sl, te_T_labels)

    print('\n=== target G (reference, same models) ===')
    ridge_g = RidgeCV(alphas=np.logspace(-4, 3, 15)).fit(Xtr_s, Gtr)
    metrics('linear (RidgeCV)', ridge_g, Xtr_s, Gtr, Xte_s, Gte,
            te_sl, te_G_labels)
    gbt_g = HistGradientBoostingRegressor(
        max_iter=500, learning_rate=0.08, max_leaf_nodes=63,
        l2_regularization=1e-3, early_stopping=True, validation_fraction=0.15,
        random_state=SPLIT_SEED).fit(Xtr_s, Gtr)
    metrics(f'GBT ({gbt_g.n_iter_} iters)', gbt_g, Xtr_s, Gtr, Xte_s, Gte,
            te_sl, te_G_labels)

    print('\nverdict inputs: nonlinear-vs-linear on T  '
          f'dMSE_te = {res_gbt["mse_te"] - res_lin["mse_te"]:+.4f} (GBT), '
          f'{res_mlp["mse_te"] - res_lin["mse_te"]:+.4f} (MLP);  '
          f'dR2_te = {res_gbt["r2_te"] - res_lin["r2_te"]:+.4f} (GBT), '
          f'{res_mlp["r2_te"] - res_lin["r2_te"]:+.4f} (MLP);  '
          f'ep-std {res_lin["ep_std"]:.4f} -> {res_gbt["ep_std"]:.4f} (GBT) / '
          f'{res_mlp["ep_std"]:.4f} (MLP) vs label {ep_pred_std(te_T_labels):.4f}')


if __name__ == '__main__':
    main()
