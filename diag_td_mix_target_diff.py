#!/usr/bin/env python3
"""Offline post-mortem of the pre_td_ramp metric + mc_td_mix Stage-2 discriminator.

Zero engine, zero self-play: reads the Stage-1 backup snapshot
(stage1_mc_td_mix_backup_20260713/replay_buffer.pkl + weight jsons) and answers:

  A. Does the pure-MC target (mc_return, Lever-B fold-in) ALREADY ramp before an
     own TD?  -> hypothesis (a): the metric's "positive ramp" needs no new target.
  B. Does the pre-existing value head (weights_best.json, the init both runs
     copy into az_train) already show a positive V-ramp on the same states?
     -> hypothesis (b): cross-game feature correlation, head ramped before Stage 1.
  C. Bit-level target diff on identical transitions:
     delta_t = (1-alpha)*(r_t + gamma*V(s') - G_t), alpha=0.7 -> how much and
     WHERE do the two targets actually differ.
  D. Within-episode target structure: std of MC target vs mixed target inside
     one episode (the quantity the flat-value-target root cause says is missing).
  E. Controlled A/B retrain: SAME init head, SAME transition order, alpha=1.0 vs
     alpha=0.7 -> do the two targets even push the head to different functions,
     and does any pre_td_ramp-style metric separate them?

Episode reconstruction: replay_buffer.add_game appends each perspective's states
contiguously, ending with is_terminal=True, so episodes are the maximal runs
ending at a terminal transition (the deque may truncate the head of the oldest
episode only).  Pre-TD window mirrors evaluate.pre_td_value_ramp (window=3):
reward_step[i] > 0 means an own TD between state i and i+1 (registered at state
i+1), so pre indices are {i-2, i-1, i}.
"""
from __future__ import annotations

import json
import pickle
import random
import sys
from copy import deepcopy
from pathlib import Path

import numpy as np

ROOT = Path('/home/jan/claude/bloodbowl')
BACKUP = ROOT / 'stage1_mc_td_mix_backup_20260713'
sys.path.insert(0, str(ROOT / 'python'))

from blood_bowl.trainer import load_trainer  # noqa: E402
from blood_bowl import replay_buffer as rb  # noqa: E402  (Transition unpickling)

GAMMA = 0.99
ALPHA = 0.7
WINDOW = 3
LR = 0.0003          # matches the Stage-1 / null run config
AB_PASSES = 3
AB_SEED = 20260714


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
    # trailing partial episode (game cut by deque tail) is dropped
    return episodes


def categorize(episodes: list[list]) -> tuple[list, list, list]:
    """Return (pre_idx_flags, own_td_episode_flags, flat transitions) aligned."""
    flat, pre_flags, tdep_flags = [], [], []
    for ep in episodes:
        n = len(ep)
        pre = set()
        has_td = False
        for i, tr in enumerate(ep):
            r = tr.reward_step if tr.reward_step is not None else 0.0
            if r > 0:  # own TD between i and i+1  -> registered at state i+1
                has_td = True
                j = i + 1
                pre.update(range(max(0, j - WINDOW), j))
        for i, tr in enumerate(ep):
            flat.append(tr)
            pre_flags.append(i in pre)
            tdep_flags.append(has_td)
    return pre_flags, tdep_flags, flat


def ramp(values: np.ndarray, pre: np.ndarray) -> float:
    return float(values[pre].mean() - values[~pre].mean())


def head_values(trainer, feats: list) -> np.ndarray:
    return np.array([trainer.evaluate(f) for f in feats])


def main() -> None:
    buf = load_buffer(BACKUP / 'replay_buffer.pkl')
    episodes = split_episodes(buf)
    pre_flags, tdep_flags, flat = categorize(episodes)
    pre = np.array(pre_flags)
    tdep = np.array(tdep_flags)
    term = np.array([tr.is_terminal for tr in flat])
    G = np.array([tr.mc_return if tr.mc_return is not None else tr.reward
                  for tr in flat])
    r_step = np.array([tr.reward_step if tr.reward_step is not None else 0.0
                       for tr in flat])
    persp_ok = all(len({t.perspective for t in ep}) == 1 for ep in episodes)

    print(f'buffer transitions: {len(buf)}  episodes: {len(episodes)} '
          f'(len min/med/max: {min(map(len, episodes))}/'
          f'{int(np.median([len(e) for e in episodes]))}/{max(map(len, episodes))})')
    print(f'perspective constant within episodes: {persp_ok}')
    n = len(flat)
    print(f'states: {n}  pre-TD: {pre.sum()} ({pre.mean():.1%})  '
          f'in own-TD episodes: {tdep.sum()} ({tdep.mean():.1%})  '
          f'terminal: {term.sum()}')

    # ---- A. does the PURE MC target already ramp? --------------------------
    print('\n[A] Ramp of the raw MC target (mc_return, Lever-B fold-in) itself:')
    print(f'    mean G | pre-TD           = {G[pre].mean():+.4f}')
    print(f'    mean G | elsewhere        = {G[~pre].mean():+.4f}')
    print(f'    TARGET ramp               = {ramp(G, pre):+.4f}')
    other_same = (~pre) & tdep
    other_noTD = (~pre) & (~tdep)
    print(f'    decomposition of "elsewhere":')
    print(f'      mean G | own-TD episode, non-pre = {G[other_same].mean():+.4f} (n={other_same.sum()})')
    print(f'      mean G | no-own-TD episode       = {G[other_noTD].mean():+.4f} (n={other_noTD.sum()})')
    print(f'    ramp vs same-episode-only         = {G[pre].mean() - G[other_same].mean():+.4f}')

    # ---- B. did the INIT head (old mc_return training) already ramp? -------
    feats = [tr.features for tr in flat]
    heads = {}
    for label, path in [('init (weights_best.json, pre-Stage1)', ROOT / 'weights_best.json'),
                        ('stage1 final (backup az_train)', BACKUP / 'weights_az_train.json'),
                        ('stage1 best (backup train_best)', BACKUP / 'weights_train_best.json')]:
        try:
            heads[label] = load_trainer(str(path), learning_rate=LR)
        except Exception as e:  # noqa: BLE001
            print(f'    [skip] {label}: {e!r}')
    # null-run head: root file, written by the (possibly still running) null run;
    # read defensively, snapshot provenance noted in the report.
    try:
        heads['null a=1.0 best (root train_best, 08:08)'] = load_trainer(
            str(ROOT / 'weights_train_best.json'), learning_rate=LR)
    except Exception as e:  # noqa: BLE001
        print(f'    [skip] null head: {e!r}')

    print('\n[B] V-ramp of each head on the SAME buffer states (window=3):')
    V = {}
    for label, tr in heads.items():
        V[label] = head_values(tr, feats)
        print(f'    {label:47s} ramp = {ramp(V[label], pre):+.4f}  '
              f'(mean|V|={np.abs(V[label]).mean():.3f})')

    # ---- C. bit-level target diff on identical transitions -----------------
    print(f'\n[C] Target diff alpha={ALPHA} vs alpha=1.0 on identical transitions')
    for label in ['init (weights_best.json, pre-Stage1)',
                  'stage1 final (backup az_train)']:
        if label not in heads:
            continue
        trn = heads[label]
        v_next = np.array([trn.evaluate(t.next_features) for t in flat])
        boot = r_step + GAMMA * v_next
        mix = np.clip(ALPHA * G + (1 - ALPHA) * boot, -1, 1)
        mix[term] = G[term]                      # terminal anchor identical
        delta = mix - G
        nt = ~term
        print(f'    V(s\') from head: {label}')
        print(f'      mean|delta| (non-term)      = {np.abs(delta[nt]).mean():.4f}')
        print(f'      frac |delta|>0.05           = {(np.abs(delta[nt]) > 0.05).mean():.1%}')
        print(f'      frac |delta|>0.10           = {(np.abs(delta[nt]) > 0.10).mean():.1%}')
        print(f'      mean delta | pre-TD         = {delta[pre & nt].mean():+.4f}')
        print(f'      mean delta | ownTD-ep other = {delta[other_same & nt].mean():+.4f}')
        print(f'      mean delta | no-TD episode  = {delta[other_noTD & nt].mean():+.4f}')
        print(f'      corr(delta, pre-TD flag)    = {np.corrcoef(delta[nt], pre[nt])[0, 1]:+.3f}')
        # D. within-episode structure
        stds_mc, stds_mix = [], []
        k = 0
        for ep in episodes:
            m = len(ep)
            idx = slice(k, k + m)
            if m >= 3:
                stds_mc.append(G[idx].std())
                stds_mix.append(mix[idx].std())
            k += m
        print(f'      [D] mean within-episode std: MC target = {np.mean(stds_mc):.4f}, '
              f'mixed target = {np.mean(stds_mix):.4f} '
              f'(ratio {np.mean(stds_mix) / max(np.mean(stds_mc), 1e-9):.2f}x)')
        # calibration of this head vs realized MC return
        v_cur = V[label]
        print(f'      corr(V, G) = {np.corrcoef(v_cur, G)[0, 1]:+.3f}   '
              f'MSE(V, G) = {np.mean((v_cur - G) ** 2):.4f}')

    # ---- E. controlled A/B: same init, same data order, alpha 1.0 vs 0.7 ---
    print(f'\n[E] Controlled offline A/B retrain: init=weights_best.json head, '
          f'{AB_PASSES} passes over the buffer, lr={LR}, identical shuffled order')
    base = heads.get('init (weights_best.json, pre-Stage1)')
    if base is None:
        print('    [skip] init head unavailable')
        return
    order = list(range(n))
    random.Random(AB_SEED).shuffle(order)
    results = {}
    for alpha in (1.0, 0.7, 0.5, 0.3, 0.1):
        t = deepcopy(base)
        for p in range(AB_PASSES):
            for i in order:
                tr = flat[i]
                t.train_transition_td_mix(
                    tr.features, tr.next_features,
                    mc_return=G[i], reward_step=r_step[i],
                    is_terminal=bool(term[i]), gamma=GAMMA, alpha=alpha)
        results[alpha] = head_values(t, feats)
    v10 = results[1.0]

    def ep_std(v: np.ndarray) -> float:
        stds, k = [], 0
        for ep in episodes:
            m = len(ep)
            if m >= 3:
                stds.append(v[k:k + m].std())
            k += m
        return float(np.mean(stds))

    print('    alpha  mean|V_a-V_1.0|  corr(V_a,V_1.0)  pre-TD ramp  '
          'within-ep V std  MSE vs G  mean|V|')
    for alpha, v in results.items():
        dv = v - v10
        print(f'    {alpha:4.1f}   {np.abs(dv).mean():14.4f}  '
              f'{np.corrcoef(v, v10)[0, 1]:15.4f}  {ramp(v, pre):+10.4f}  '
              f'{ep_std(v):14.4f}  {np.mean((v - G) ** 2):8.4f}  '
              f'{np.abs(v).mean():7.3f}')
    dv07 = results[0.7] - v10
    print(f'    alpha=0.7 detail: dV(pre-TD) = {dv07[pre].mean():+.4f}, '
          f'dV(elsewhere) = {dv07[~pre].mean():+.4f}')


if __name__ == '__main__':
    main()
