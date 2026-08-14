#!/usr/bin/env python3
"""Offline pre-filter for the drive-level value target (roadmap c1, section 2.4).

Zero engine, zero self-play: reads the live replay_buffer.pkl and runs the same
S-E controlled A/B retrain methodology as diag_td_mix_target_diff.py (same init
weights_best.json, same seed/order, 3 passes, lr=3e-4), swapping in the
drive-level candidate target from proposals_value_signal_roadmap_20260714.md
section 2.1:

    drive_outcome(d) = +D   if drive d ends in own TD
                       -D   if it ends in opponent TD
                       -d0  if it fizzles (episode segment ends without a TD)
    T_t = clip( lam*G_t + (1-lam)*gamma^(k_d - t)*drive_outcome(drive of t), -1, 1 )

with terminal states keeping the plain G anchor (same convention as the td_mix
script / train_transition_td_mix: the terminal state is never re-targeted).

Episode reconstruction: maximal contiguous runs ending at is_terminal=True
(replay_buffer.add_game appends each perspective contiguously; a trailing
partial episode cut by the deque is dropped).  Drive reconstruction within an
episode: reward_step[i] != 0 means a TD between state i and i+1 (sign = whose),
so state i is the LAST state of the scoring drive (k_d = i) and state i+1
starts the next drive.  A trailing segment with no TD is a fizzled drive with
k_d = last state of the episode.  Half boundaries without a TD are NOT visible
in the buffer, so an H1 no-score drive merges with the first H2 drive of the
same kind -- acceptable for this divergence verdict per roadmap section 2.3.

Grid: lam in {0.7, 0.5, 0.3} x D in {0.4, 0.6} x d0 in {0, 0.1}.
Reference arm: identical retrain with target = G (bit-exact alpha=1.0 arm of
diag_td_mix_target_diff.py, via train_transition_return with no shaping).

Metrics per roadmap section 1.2: mean|V_T - V_ref|, corr(V_T, V_ref),
within-episode V std, MSE vs G, and the outcome-controlled pre-TD ramp
(pre-TD vs other states WITHIN own-TD episodes only -- the uncontrolled
pre_td_value_ramp is intentionally not reported for verdicts).
"""
from __future__ import annotations

import pickle
import random
import sys
from copy import deepcopy
from itertools import product
from pathlib import Path

import numpy as np

ROOT = Path('/home/jan/claude/bloodbowl')
sys.path.insert(0, str(ROOT / 'python'))

from blood_bowl.trainer import load_trainer  # noqa: E402
from blood_bowl import replay_buffer as rb  # noqa: E402  (Transition unpickling)

GAMMA = 0.99          # matches rewards.episode_returns default
WINDOW = 3            # pre-TD window, mirrors evaluate.pre_td_value_ramp
LR = 0.0003           # matches Stage-1 / null run config (S-E convention)
AB_PASSES = 3
AB_SEED = 20260714    # same seed as diag_td_mix_target_diff.py for comparability

LAM_GRID = (0.7, 0.5, 0.3)
D_GRID = (0.4, 0.6)
D0_GRID = (0.0, 0.1)


def load_buffer(path: Path) -> list:
    with open(path, 'rb') as f:
        return pickle.load(f)


def split_episodes(transitions: list) -> list[list]:
    """Maximal contiguous runs ending at is_terminal=True."""
    episodes, cur = [], []
    for tr in transitions:
        cur.append(tr)
        if tr.is_terminal:
            episodes.append(cur)
            cur = []
    return episodes  # trailing partial episode (deque tail cut) dropped


def drive_segments(ep_rewards: list[float]) -> list[tuple[int, int, int]]:
    """Return (start, k_d, outcome_sign) per drive; sign 0 = fizzled.

    reward_step[i] != 0  ->  TD between state i and i+1: drive ends AT state i.
    """
    segs, start = [], 0
    n = len(ep_rewards)
    for i, r in enumerate(ep_rewards):
        if r != 0.0:
            segs.append((start, i, 1 if r > 0 else -1))
            start = i + 1
    if start < n:
        segs.append((start, n - 1, 0))
    return segs


def build_drive_target(episodes: list[list], G: np.ndarray, term: np.ndarray,
                       lam: float, D: float, d0: float) -> np.ndarray:
    T = np.empty_like(G)
    k = 0
    for ep in episodes:
        n = len(ep)
        r = [tr.reward_step if tr.reward_step is not None else 0.0 for tr in ep]
        for start, kd, sign in drive_segments(r):
            outcome = sign * D if sign != 0 else -d0
            for t in range(start, kd + 1):
                comp = (GAMMA ** (kd - t)) * outcome
                T[k + t] = min(1.0, max(-1.0, lam * G[k + t] + (1.0 - lam) * comp))
        k += n
    T[term] = G[term]  # terminal anchor, same convention as td_mix S-E
    return T


def retrain(base, flat: list, target: np.ndarray, term: np.ndarray,
            order: list[int]):
    t = deepcopy(base)
    for _ in range(AB_PASSES):
        for i in order:
            tr = flat[i]
            t.train_transition_return(
                tr.features, tr.next_features,
                mc_return=float(target[i]), is_terminal=bool(term[i]),
                gamma=GAMMA)
    return t


def head_values(trainer, feats: list) -> np.ndarray:
    return np.array([trainer.evaluate(f) for f in feats])


def main() -> None:
    buf = load_buffer(ROOT / 'replay_buffer.pkl')
    episodes = split_episodes(buf)
    flat = [tr for ep in episodes for tr in ep]
    n = len(flat)
    ep_lens = [len(e) for e in episodes]
    term = np.array([tr.is_terminal for tr in flat])
    G = np.array([tr.mc_return if tr.mc_return is not None else tr.reward
                  for tr in flat])
    r_step = np.array([tr.reward_step if tr.reward_step is not None else 0.0
                       for tr in flat])
    feats = [tr.features for tr in flat]

    # pre-TD flags + own-TD-episode flags (for the outcome-CONTROLLED ramp)
    pre_flags, tdep_flags = [], []
    for ep in episodes:
        m = len(ep)
        pre, has_td = set(), False
        for i, tr in enumerate(ep):
            r = tr.reward_step if tr.reward_step is not None else 0.0
            if r > 0:
                has_td = True
                j = i + 1
                pre.update(range(max(0, j - WINDOW), j))
        for i in range(m):
            pre_flags.append(i in pre)
            tdep_flags.append(has_td)
    pre = np.array(pre_flags)
    tdep = np.array(tdep_flags)
    other_same = (~pre) & tdep  # non-pre states inside own-TD episodes

    # drive census
    n_drives = n_own = n_opp = n_fizz = 0
    for ep in episodes:
        r = [tr.reward_step if tr.reward_step is not None else 0.0 for tr in ep]
        for _, _, sign in drive_segments(r):
            n_drives += 1
            n_own += sign > 0
            n_opp += sign < 0
            n_fizz += sign == 0

    print(f'buffer transitions: {len(buf)}  episodes: {len(episodes)} '
          f'(len min/med/max: {min(ep_lens)}/{int(np.median(ep_lens))}/{max(ep_lens)})')
    print(f'states used: {n}  pre-TD: {pre.sum()} ({pre.mean():.1%})  '
          f'in own-TD episodes: {tdep.sum()} ({tdep.mean():.1%})')
    print(f'drives: {n_drives}  (own-TD {n_own}, opp-TD {n_opp}, fizzled {n_fizz})')

    def ep_std(v: np.ndarray) -> float:
        stds, k = [], 0
        for m in ep_lens:
            if m >= 3:
                stds.append(v[k:k + m].std())
            k += m
        return float(np.mean(stds))

    def ctrl_ramp(v: np.ndarray) -> float:
        """Outcome-controlled pre-TD ramp: within own-TD episodes only."""
        return float(v[pre].mean() - v[other_same].mean())

    print(f'\nlabel structure: within-ep std G = {ep_std(G):.4f}, '
          f'controlled ramp of G itself = {ctrl_ramp(G):+.4f}')

    base = load_trainer(str(ROOT / 'weights_best.json'), learning_rate=LR)
    v_init = head_values(base, feats)
    print(f'init head: within-ep V std = {ep_std(v_init):.4f}, '
          f'controlled ramp = {ctrl_ramp(v_init):+.4f}, '
          f'MSE vs G = {np.mean((v_init - G) ** 2):.4f}, '
          f'mean|V| = {np.abs(v_init).mean():.3f}')

    order = list(range(n))
    random.Random(AB_SEED).shuffle(order)

    # reference arm: pure G target (bit-exact alpha=1.0 S-E arm)
    v_ref = head_values(retrain(base, flat, G, term, order), feats)
    mse_ref = float(np.mean((v_ref - G) ** 2))
    print(f'\nreference arm (target = G, {AB_PASSES} passes, lr={LR}):')
    print(f'  within-ep V std = {ep_std(v_ref):.4f}  controlled ramp = '
          f'{ctrl_ramp(v_ref):+.4f}  MSE vs G = {mse_ref:.4f}  '
          f'mean|V| = {np.abs(v_ref).mean():.3f}')

    print('\ngrid (thresholds: mean|dV|>0.1, corr<0.99, ep-std up vs ref, '
          'MSE rel. worsening <10%):')
    print('  lam   D    d0  | ep-std T | mean|dV|  corr    ep-std V  MSE vs G '
          ' (rel)   ctrl ramp  mean|V|')
    for lam, D, d0 in product(LAM_GRID, D_GRID, D0_GRID):
        T = build_drive_target(episodes, G, term, lam, D, d0)
        v = head_values(retrain(base, flat, T, term, order), feats)
        dv = float(np.abs(v - v_ref).mean())
        corr = float(np.corrcoef(v, v_ref)[0, 1])
        mse = float(np.mean((v - G) ** 2))
        rel = (mse - mse_ref) / mse_ref
        print(f'  {lam:.1f}  {D:.1f}  {d0:.1f} |  {ep_std(T):7.4f} | '
              f'{dv:8.4f}  {corr:6.4f}  {ep_std(v):8.4f}  {mse:8.4f} '
              f'({rel:+6.1%})  {ctrl_ramp(v):+9.4f}  {np.abs(v).mean():7.3f}')


if __name__ == '__main__':
    main()
