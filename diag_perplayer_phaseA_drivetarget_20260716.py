#!/usr/bin/env python3
"""Phase A retest (2026-07-16) against the DRIVE-LEVEL target instead of G.

Follow-up to diag_perplayer_phaseA.py (2026-07-15), which found mean|dV|=0.024
(4x below the roadmap's >0.1 bar) for 7 grounded per-player candidate features
-- but flagged an unresolved caveat: G is nearly flat within an episode
(ep-std 0.030), the same structural ceiling that sank mc_td_mix and the
drive-level target itself (as a TRAINING target). The proposed fairer test
(never run until now, see memory project_bloodbowl_phaseA_result_20260715):
retest the same 7 candidates as a RIDGE-FIT target against the drive-level
label instead of G -- richer within-episode structure (ep-std 0.07-0.16 per
evidence/fable_drive_target_prefilter_20260715.md), even though that same
report showed a trained value head still can't express it. This script asks
the narrower, cheaper question Phase A actually needs: does adding the
candidate features improve a LINEAR FIT's within-episode structure against
this richer target, independent of whether an MLP/trained head can learn it.

ALSO: this run uses the freshly regenerated 150-game corpus
(diag_perplayer_grounding_data/main_postfix/), built on the POST-hasActed-fix
+ post-throwin-fix engine (2026-07-16) -- the original 2026-07-15 corpus and
Phase A run both predate those fixes (see
project_bloodbowl_survey_hasacted_contamination_20260716 memory: 7/12 curated
situations in that corpus showed the bug's reactivation signature). This is
simultaneously the "clean corpus" and "fair target" retest in one run.

Drive-level target construction (mirrors diag_drive_target_diff.py's
build_drive_target, adapted from replay_buffer Transition.reward_step to the
turn-snapshot JSON's home_score/away_score deltas, since these episodes come
from diag_perplayer_grounding_data, not replay_buffer.pkl):

    T_t = clip( lam*G_t + (1-lam)*gamma^(k_d - t)*drive_outcome(drive of t), -1, 1 )
    drive_outcome(d) = +D if drive d ends in own TD, -D if opponent TD, -d0 if it fizzles

Using lam=0.5, D=0.6, d0=0.1 -- one of the exact grid cells already tested in
diag_drive_target_diff.py (not a new ad-hoc choice), representative of the
"diverges but still roughly calibrated" region of that grid.

USAGE:
  python3 diag_perplayer_phaseA_drivetarget_20260716.py run [label]   # default main_postfix
  python3 diag_perplayer_phaseA_drivetarget_20260716.py fit [seeds...]
"""
from __future__ import annotations

import pickle
import sys
from pathlib import Path

import numpy as np

ROOT = Path('/home/jan/claude/bloodbowl')
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'python'))
sys.path.insert(0, str(ROOT / 'engine/build'))

from blood_bowl.rewards import episode_returns, terminal_value  # noqa: E402

from diag_perplayer_grounding import load_games, player_table, Board  # noqa: E402
from diag_perplayer_phaseA import (  # noqa: E402
    extract_f73, candidate_features, CANDIDATE_NAMES,
    ep_pred_std, within_ep_r2, within_ep_corr, between_ep_r2,
)

GAMMA = 0.99
LAM, D, D0 = 0.5, 0.6, 0.1  # one exact grid cell from diag_drive_target_diff.py
CACHE_PATH = ROOT / 'diag_perplayer_phaseA_drivetarget_cache_20260716.pkl'


# ---------------------------------------------------------------------------
# Drive-level target, reconstructed from turn-snapshot score deltas
# (adapted from diag_drive_target_diff.py's build_drive_target/drive_segments,
#  which operate on replay_buffer Transition.reward_step -- here we detect the
#  same "TD between state i and i+1" event from home_score/away_score deltas
#  across consecutive turns of the SAME perspective's own subsequence).
# ---------------------------------------------------------------------------

def reward_steps(my_scores: list[int], opp_scores: list[int]) -> list[float]:
    """+1 at state i if MY score increases between turn i and i+1 (own TD scored
    during/after that state's turn), -1 if opponent's does, else 0. Last state
    has no 'next' so its reward_step is 0 (matches replay_buffer convention:
    the terminal transition's reward is the episode-final reward, handled
    separately by episode_returns, not via reward_step)."""
    n = len(my_scores)
    r = [0.0] * n
    for i in range(n - 1):
        if my_scores[i + 1] > my_scores[i]:
            r[i] = 1.0
        elif opp_scores[i + 1] > opp_scores[i]:
            r[i] = -1.0
    return r


def drive_segments(r: list[float]) -> list[tuple[int, int, int]]:
    segs, start = [], 0
    n = len(r)
    for i, x in enumerate(r):
        if x != 0.0:
            segs.append((start, i, 1 if x > 0 else -1))
            start = i + 1
    if start < n:
        segs.append((start, n - 1, 0))
    return segs


def build_drive_target(G: np.ndarray, r_step: list[float]) -> np.ndarray:
    n = len(G)
    T = np.empty(n)
    for start, kd, sign in drive_segments(r_step):
        outcome = sign * D if sign != 0 else -D0
        for t in range(start, kd + 1):
            comp = (GAMMA ** (kd - t)) * outcome
            T[t] = min(1.0, max(-1.0, LAM * G[t] + (1.0 - LAM) * comp))
    return T


# ---------------------------------------------------------------------------
# Episode construction: same grouping as diag_perplayer_phaseA.build_episodes,
# plus the drive-level target T alongside G.
# ---------------------------------------------------------------------------

def build_episodes(games: list) -> list[dict]:
    episodes = []
    for gi, g in enumerate(games):
        table = player_table(g['home_race'], g['away_race'])
        groups: dict = {'home': [], 'away': []}
        for k, turn in enumerate(g['turns']):
            groups[turn['active_team']].append(k)

        for persp, idxs in groups.items():
            if not idxs:
                continue
            my_scores, opp_scores = [], []
            for k in idxs:
                t = g['turns'][k]
                if persp == 'home':
                    my_scores.append(t['home_score'])
                    opp_scores.append(t['away_score'])
                else:
                    my_scores.append(t['away_score'])
                    opp_scores.append(t['home_score'])
            reward = terminal_value(g['home_score'], g['away_score'], persp)
            G = np.array(episode_returns(my_scores, opp_scores, reward, GAMMA))
            r_step = reward_steps(my_scores, opp_scores)
            T = build_drive_target(G, r_step)

            X73 = np.zeros((len(idxs), 73))
            Xc = np.zeros((len(idxs), len(CANDIDATE_NAMES)))
            for i, k in enumerate(idxs):
                turn = g['turns'][k]
                X73[i] = extract_f73(g, turn)
                b = Board(turn)
                Xc[i] = candidate_features(b, table, persp)

            episodes.append(dict(
                game=g['_file'], perspective=persp, G=G, T=T,
                X73=X73, Xc=Xc, n=len(idxs),
            ))
        if (gi + 1) % 25 == 0:
            print(f'  ...{gi + 1}/{len(games)} games reconstructed', flush=True)
    return episodes


def cmd_run(label: str) -> None:
    games = list(load_games(label))
    print(f'reconstructing f73 + candidates + drive-target for {len(games)} games '
          f'from label={label!r} (lam={LAM}, D={D}, d0={D0})...')
    episodes = build_episodes(games)
    n_states = sum(e['n'] for e in episodes)
    print(f'DONE: {len(episodes)} episodes, {n_states} total states')
    with open(CACHE_PATH, 'wb') as f:
        pickle.dump(episodes, f)
    print(f'cached -> {CACHE_PATH}')


# ---------------------------------------------------------------------------
# Ridge fit: T ~ f73 (baseline) vs T ~ f73+candidates -- same methodology as
# diag_perplayer_phaseA.one_fit, generalized to a configurable target key.
# ---------------------------------------------------------------------------

def one_fit(episodes: list, seed: int, target_key: str, verbose: bool = True) -> dict:
    from sklearn.linear_model import RidgeCV
    from sklearn.preprocessing import StandardScaler

    n_ep = len(episodes)
    rng = np.random.default_rng(seed)
    perm = rng.permutation(n_ep)
    n_te = int(round(0.2 * n_ep))
    te_set = set(perm[:n_te].tolist())
    tr_ids = [i for i in range(n_ep) if i not in te_set]
    te_ids = [i for i in range(n_ep) if i in te_set]

    def stack(ids, key):
        return np.vstack([episodes[i][key] for i in ids])

    def stack_y(ids):
        return np.concatenate([episodes[i][target_key] for i in ids])

    X73_tr, X73_te = stack(tr_ids, 'X73'), stack(te_ids, 'X73')
    Xc_tr, Xc_te = stack(tr_ids, 'Xc'), stack(te_ids, 'Xc')
    ytr, yte = stack_y(tr_ids), stack_y(te_ids)
    Xcomb_tr = np.hstack([X73_tr, Xc_tr])
    Xcomb_te = np.hstack([X73_te, Xc_te])

    te_slices, k = [], 0
    for i in te_ids:
        m = episodes[i]['n']
        te_slices.append((k, k + m))
        k += m
    te_y_labels = [yte[a:b] for a, b in te_slices]

    scaler73 = StandardScaler().fit(X73_tr)
    scalerC = StandardScaler().fit(Xcomb_tr)
    X73_tr_s, X73_te_s = scaler73.transform(X73_tr), scaler73.transform(X73_te)
    Xc_tr_s, Xc_te_s = scalerC.transform(Xcomb_tr), scalerC.transform(Xcomb_te)

    alphas = np.logspace(-4, 3, 15)
    ridge_base = RidgeCV(alphas=alphas).fit(X73_tr_s, ytr)
    ridge_comb = RidgeCV(alphas=alphas).fit(Xc_tr_s, ytr)

    p_base = ridge_base.predict(X73_te_s)
    p_comb = ridge_comb.predict(Xc_te_s)

    def report(pred):
        mse_te = float(np.mean((pred - yte) ** 2))
        r2_te = 1.0 - mse_te / float(np.var(yte))
        preds_per_ep = [pred[a:b] for a, b in te_slices]
        eps = ep_pred_std(preds_per_ep)
        wr2 = within_ep_r2(te_y_labels, preds_per_ep)
        wcorr = within_ep_corr(te_y_labels, preds_per_ep)
        br2 = between_ep_r2(te_y_labels, preds_per_ep)
        return dict(mse_te=mse_te, r2_te=r2_te, ep_std=eps, wr2=wr2,
                    wcorr=wcorr, br2=br2)

    r_base = report(p_base)
    r_comb = report(p_comb)

    dv = float(np.abs(p_comb - p_base).mean())
    corr_bc = float(np.corrcoef(p_base, p_comb)[0, 1])
    label_ep_std = ep_pred_std(te_y_labels)

    if verbose:
        print(f'  seed={seed}  episodes tr/te={len(tr_ids)}/{len(te_ids)}  '
              f'states tr/te={len(ytr)}/{len(yte)}  '
              f'ridge_alpha base/comb={ridge_base.alpha_:g}/{ridge_comb.alpha_:g}')
        print(f'    label {target_key}: ep-std(test) = {label_ep_std:.4f}')
        print(f'    baseline  (f73):  MSE={r_base["mse_te"]:.4f} R2={r_base["r2_te"]:+.4f} '
              f'ep-std(pred)={r_base["ep_std"]:.4f} within-ep-R2={r_base["wr2"]:+.4f} '
              f'(corr {r_base["wcorr"]:+.3f}) between-ep-R2={r_base["br2"]:+.4f}')
        print(f'    combined  (f80):  MSE={r_comb["mse_te"]:.4f} R2={r_comb["r2_te"]:+.4f} '
              f'ep-std(pred)={r_comb["ep_std"]:.4f} within-ep-R2={r_comb["wr2"]:+.4f} '
              f'(corr {r_comb["wcorr"]:+.3f}) between-ep-R2={r_comb["br2"]:+.4f}')
        print(f'    delta:  dMSE={r_comb["mse_te"]-r_base["mse_te"]:+.4f}  '
              f'dR2={r_comb["r2_te"]-r_base["r2_te"]:+.4f}  '
              f'd(ep-std)={r_comb["ep_std"]-r_base["ep_std"]:+.4f}  '
              f'd(within-ep-R2)={r_comb["wr2"]-r_base["wr2"]:+.4f}  '
              f'mean|dV|(base,comb)={dv:.4f}  corr(base,comb)={corr_bc:.4f}')
        cand_coef = ridge_comb.coef_[-len(CANDIDATE_NAMES):]
        print('    candidate coefficients (standardized, combined fit): ' +
              ', '.join(f'{n}={c:+.4f}' for n, c in zip(CANDIDATE_NAMES, cand_coef)))

    return dict(base=r_base, comb=r_comb, dv=dv, corr_bc=corr_bc,
                label_ep_std=label_ep_std, n_tr=len(tr_ids), n_te=len(te_ids))


def cmd_fit(seeds: list[int]) -> None:
    with open(CACHE_PATH, 'rb') as f:
        episodes = pickle.load(f)
    n_ep = len(episodes)
    n_states = sum(e['n'] for e in episodes)
    print(f'{n_ep} episodes, {n_states} states, f73 baseline + '
          f'{len(CANDIDATE_NAMES)} candidates = {73 + len(CANDIDATE_NAMES)}-dim combined')
    print(f'drive-target params: lam={LAM} D={D} d0={D0}\n')

    uniq_fracs = []
    for e in episodes:
        if e['n'] < 3:
            continue
        X = np.hstack([e['X73'], e['Xc']])
        uniq = len(np.unique(np.round(X, 9), axis=0))
        uniq_fracs.append(uniq / e['n'])
    print(f'feature distinctness: unique-rows/episode-len mean = {np.mean(uniq_fracs):.3f}\n')

    Xc_all = np.vstack([e['Xc'] for e in episodes])
    print('candidate feature summary (all episodes):')
    for name, col in zip(CANDIDATE_NAMES, Xc_all.T):
        nz = (col != 0).mean()
        print(f'  {name:30s} mean={col.mean():.3f} nonzero={nz:.1%}')
    print()

    # Reference: report G's own within-episode std for comparison to T's.
    G_stds = []
    for e in episodes:
        if e['n'] >= 3:
            G_stds.append(e['G'].std())
    print(f'reference: G ep-std (whole dataset) = {np.mean(G_stds):.4f}')
    T_stds = []
    for e in episodes:
        if e['n'] >= 3:
            T_stds.append(e['T'].std())
    print(f'drive-target T ep-std (whole dataset) = {np.mean(T_stds):.4f} '
          f'({np.mean(T_stds) / np.mean(G_stds):.1f}x richer than G)\n')

    print('=== fitting against G (same as original 2026-07-15 Phase A, for reference) ===')
    results_G = [one_fit(episodes, seed, 'G') for seed in seeds]

    print('\n=== fitting against drive-level target T (the fair retest) ===')
    results_T = [one_fit(episodes, seed, 'T') for seed in seeds]

    for label, results in (('G', results_G), ('T (drive-target)', results_T)):
        print(f'\n=== summary over {len(seeds)} seeds, target={label} ===')
        metrics = {
            'dMSE (comb-base)': [r['comb']['mse_te'] - r['base']['mse_te'] for r in results],
            'dR2 (comb-base)': [r['comb']['r2_te'] - r['base']['r2_te'] for r in results],
            'd(ep-std)': [r['comb']['ep_std'] - r['base']['ep_std'] for r in results],
            'd(within-ep-R2)': [r['comb']['wr2'] - r['base']['wr2'] for r in results],
            'mean|dV|': [r['dv'] for r in results],
            'corr(base,comb)': [r['corr_bc'] for r in results],
        }
        for m_label, vals in metrics.items():
            m, s = float(np.mean(vals)), float(np.std(vals))
            print(f'  {m_label:20s} mean={m:+.4f}  std={s:.4f}  '
                  f'seeds={[round(v, 4) for v in vals]}')
        label_stds = [r['label_ep_std'] for r in results]
        base_stds = [r['base']['ep_std'] for r in results]
        comb_stds = [r['comb']['ep_std'] for r in results]
        print(f'  label ep-std (ceiling): {np.mean(label_stds):.4f}  '
              f'baseline ep-std: {np.mean(base_stds):.4f}  '
              f'combined ep-std: {np.mean(comb_stds):.4f}')
        print(f'  ep-std recovered by baseline: {np.mean(base_stds)/np.mean(label_stds):.1%} of ceiling; '
              f'by combined: {np.mean(comb_stds)/np.mean(label_stds):.1%} of ceiling')


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'run'
    if cmd == 'run':
        cmd_run(sys.argv[2] if len(sys.argv) > 2 else 'main_postfix')
    elif cmd == 'fit':
        seeds = [int(x) for x in sys.argv[2:]] if len(sys.argv) > 2 else \
            [20260715, 20260716, 20260717, 20260718, 20260719]
        cmd_fit(seeds)
    else:
        print(f'unknown command {cmd}; use "run" or "fit"')
