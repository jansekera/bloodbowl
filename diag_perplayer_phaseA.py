#!/usr/bin/env python3
"""Phase A offline ridge-fit validation (2026-07-15), per
proposals_value_signal_roadmap_20260714.md section 4.2.

Question: do today's grounded/corrected per-player candidate features lift a
ridge fit's within-episode prediction structure for the standard MC-return
target G, over the baseline 73-dim aggregate feature vector alone? This is
the go/no-go gate the roadmap sets BEFORE any full C++ per-player build
(~492 features, new network head, ~12 engine-side change points per the
Opus audit) is attempted.

DATA: the 150-game dataset already collected today for the replay-grounding
pass (diag_perplayer_grounding_data/main/g****.json.gz, full per-turn
board snapshots) -- this already satisfies the roadmap's "persist board
snapshots" prerequisite (cpp_runner.py itself still does NOT persist
per-player snapshots in production; today's collection is the first time
this has been done at all, via a dedicated script rather than a production
change). No new games collected for this test.

RECONSTRUCTION (both pieces are re-derived from the raw board snapshots,
NOT copied from the production replay_buffer.pkl, so the candidate features
below can be computed against the exact same board state as f73):

1. Baseline f73: reconstruct a bb_engine.GameState per snapshot (roster via
   bb_engine.get_developed_roster + bb_engine.setup_half for correct
   stats/skills, then override position/state/score/ball/turn/active_team
   from the logged snapshot) and call bb_engine.extract_features() -- the
   actual production C++ extractor, zero drift by construction (it *is* the
   production code, not a Python reimplementation). Known approximations,
   disclosed: (a) rerolls fixed at 3/3 (not logged -- affects features
   10-11), (b) weather fixed at NICE (not logged -- affects one-hot features
   24-26), (c) players not present in a snapshot (KO/injured/dead/off-pitch,
   all collapsed to "absent" by the engine's own captureTurnSnapshot) are
   uniformly reconstructed as KO (collapses the my_ko/my_injured features
   6-9 split -- their SUM is unaffected, only the split within it).

2. Target G: rewards.episode_returns()/terminal_value() (SSOT for the
   project's standard mc_shaped target, GAMMA=0.99), applied EXACTLY as
   replay_buffer.ReplayBuffer.add_game() does it -- group each game's turn
   sequence by perspective (active_team), compute my/opp running scores
   within that perspective's own subsequence, discount from the game's
   final result. This reproduces the production mc_return field without
   needing the production buffer (which lacks per-player positions).

3. Candidate features (7 scalars, all reusing BFS/Board machinery already
   built in diag_perplayer_grounding.py for today's grounding pass -- no new
   feature-extraction machinery, per the roadmap's Phase A framing):
     c1 carrier_blitzable_bfs      -- bfs_can_blitz, binary, replaces f63's
                                       Chebyshev approximation
     c2 net_st_bad_exposure_frac   -- fraction of my standing players
                                       adjacent to an opponent in a net<0
                                       (opponent dice-choice) block AND
                                       lacking Wrestle (today's Task-2
                                       correction), i.e. genuine downside
                                       risk currently on the board -- new
                                       information vs f65's favorable_blocks
                                       (which only counts the upside)
     c3 cage_worst_corner_tier     -- 0/1/2 (solid/soft/severe ST), 0 if no
                                       cage; from today's cage-corner section
     c4 cage_worst_corner_dodge    -- 1 if that worst corner has Dodge
                                       (today's Dodge x Tackle follow-up)
     c5 mobility_advantage_progress-- best teammate's safe-BFS progress
                                       toward the endzone minus the
                                       carrier's, continuous (today's
                                       reframed is_free_receiver, the
                                       strongest single grounding result:
                                       30.8% prevalence vs 0.5% originally)
     c6 mobility_advantage_flag    -- c5 >= 3 squares, binary version
     c7 carrier_adjacent_sideline  -- carrier on y in {0,1,13,14}, binary

USAGE: python3 diag_perplayer_phaseA.py run          # build+cache dataset
       python3 diag_perplayer_phaseA.py fit [seed]   # ridge fit + report
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

import bb_engine as bb  # noqa: E402
from blood_bowl.rewards import episode_returns, terminal_value  # noqa: E402

from diag_perplayer_grounding import (  # noqa: E402
    Board, player_table, cheb, DIAG, bfs_can_blitz, bfs_safe_reachable,
    block_dice, load_games, TV,
)

GAMMA = 0.99
CACHE_PATH = ROOT / 'diag_perplayer_phaseA_cache.pkl'
MOBILITY_THRESHOLD = 3


# ---------------------------------------------------------------------------
# 1. GameState reconstruction -> production extract_features()
# ---------------------------------------------------------------------------

_STATE_MAP = {0: bb.PlayerState.STANDING, 1: bb.PlayerState.PRONE,
              2: bb.PlayerState.STUNNED}
_ROSTER_CACHE: dict = {}


def _roster(race: str):
    if race not in _ROSTER_CACHE:
        _ROSTER_CACHE[race] = bb.get_developed_roster(race, TV)
    return _ROSTER_CACHE[race]


def build_gamestate(g: dict, turn: dict):
    hr, ar = _roster(g['home_race']), _roster(g['away_race'])
    gs = bb.GameState()
    bb.setup_half(gs, hr, ar)  # correct stats/skills for all 22 ids

    present = {}
    for key in ('home_players', 'away_players'):
        for p in turn[key]:
            present[p['id']] = p

    for pid in range(1, 23):
        pl = gs.get_player(pid)
        if pid in present:
            p = present[pid]
            pl.position = bb.Position(int(p['x']), int(p['y']))
            pl.state = _STATE_MAP.get(p['state'], bb.PlayerState.KO)
            pl.movement_remaining = pl.stats.movement
            pl.has_moved = False
            pl.has_acted = False
        else:
            pl.position = bb.Position(-1, -1)
            pl.state = bb.PlayerState.KO  # approximation: KO/injured/dead/off-pitch collapsed

    gs.half = turn['half']
    gs.active_team = bb.TeamSide.HOME if turn['active_team'] == 'home' else bb.TeamSide.AWAY
    gs.home_team.score = turn['home_score']
    gs.away_team.score = turn['away_score']
    gs.home_team.turn_number = turn['turn']  # approximation: both sides set to the
    gs.away_team.turn_number = turn['turn']  # active side's own turn count
    gs.home_team.rerolls = 3  # approximation: not logged, assume full rerolls
    gs.away_team.rerolls = 3
    gs.weather = bb.Weather.NICE  # approximation: not logged
    gs.ball.is_held = bool(turn['ball_held'])
    gs.ball.carrier_id = turn['ball_carrier_id'] if turn['ball_held'] else -1
    bx = int(turn['ball_x']) if 0 <= turn['ball_x'] <= 25 else 0
    by = int(turn['ball_y']) if 0 <= turn['ball_y'] <= 14 else 0
    gs.ball.position = bb.Position(bx, by)
    return gs


def extract_f73(g: dict, turn: dict) -> np.ndarray:
    gs = build_gamestate(g, turn)
    persp = bb.TeamSide.HOME if turn['active_team'] == 'home' else bb.TeamSide.AWAY
    return np.array(bb.extract_features(gs, persp), dtype=float)


# ---------------------------------------------------------------------------
# 2. Candidate features (reuse Board/BFS from diag_perplayer_grounding.py)
# ---------------------------------------------------------------------------

def corner_tier(t: dict) -> int:
    if t['st'] <= 2:
        return 2
    if 'Guard' not in t['skills'] and 'Block' not in t['skills']:
        return 1
    return 0


def candidate_features(b: Board, table: dict, att: str) -> np.ndarray:
    defn = 'away' if att == 'home' else 'home'
    ez = 25 if att == 'home' else 0
    opp_tz = b.tz_squares(defn)
    occ_all = b.occupied()

    c1 = c2 = c3 = c4 = c5 = c6 = c7 = 0.0

    if b.carrier >= 0 and b.side.get(b.carrier) == att:
        cpos = b.pos[b.carrier]
        c_ma = table[b.carrier]['ma']
        # c1: BFS carrier_blitzable
        c1 = 1.0 if any(bfs_can_blitz(b, pid, cpos, table[pid]['ma'])
                        for pid in b.standing(defn)) else 0.0
        # c5/c6: relative mobility advantage
        c_reach = bfs_safe_reachable(cpos, c_ma, opp_tz, occ_all - {cpos})
        c_progress = abs(cpos[0] - ez) - min(abs(s[0] - ez) for s in c_reach)
        best_prog = -999
        for pid in b.standing(att):
            if pid == b.carrier:
                continue
            pos = b.pos[pid]
            reach = bfs_safe_reachable(pos, table[pid]['ma'], opp_tz, occ_all - {pos})
            prog = abs(pos[0] - ez) - min(abs(s[0] - ez) for s in reach)
            best_prog = max(best_prog, prog)
        if best_prog > -999:
            c5 = best_prog - c_progress
            c6 = 1.0 if c5 >= MOBILITY_THRESHOLD else 0.0
        # c7: carrier adjacent to sideline
        c7 = 1.0 if cpos[1] in (0, 1, 13, 14) else 0.0
        # c3/c4: cage worst-corner tier + Dodge shield (my cage protecting my carrier)
        occ_map = {v: k for k, v in b.pos.items()}
        corners = []
        for dx, dy in DIAG:
            sq = (cpos[0] + dx, cpos[1] + dy)
            pid = occ_map.get(sq)
            if pid is not None and b.side.get(pid) == att and b.state.get(pid) == 0:
                corners.append(pid)
        if len(corners) >= 3:
            tiers = [(corner_tier(table[pid]), pid) for pid in corners]
            worst_tier, worst_pid = max(tiers, key=lambda x: x[0])
            c3 = float(worst_tier)
            c4 = 1.0 if 'Dodge' in table[worst_pid]['skills'] else 0.0

    # c2: net_st bad-exposure fraction (Wrestle-corrected), from ATT's own
    # standing players -- risk currently on the board regardless of carrier
    my_standing = b.standing(att)
    if my_standing:
        bad = 0
        for pid in my_standing:
            for opp in b.standing(defn):
                if cheb(b.pos[pid], b.pos[opp]) == 1:
                    dc = block_dice(b, table, pid, opp)
                    if dc < 0 and 'Wrestle' not in table[pid]['skills']:
                        bad += 1
                    break
        c2 = bad / len(my_standing)

    return np.array([c1, c2, c3, c4, c5, c6, c7], dtype=float)


CANDIDATE_NAMES = ['carrier_blitzable_bfs', 'net_st_bad_exposure_frac',
                   'cage_worst_corner_tier', 'cage_worst_corner_dodge',
                   'mobility_advantage_progress', 'mobility_advantage_flag',
                   'carrier_adjacent_sideline']


# ---------------------------------------------------------------------------
# 3. Episode construction (mirrors replay_buffer.ReplayBuffer.add_game)
# ---------------------------------------------------------------------------

def build_episodes(games: list) -> list[dict]:
    """One 'episode' per (game, perspective) -- exactly the granularity
    replay_buffer.add_game groups by, and diag_capacity_probe.py's
    split_episodes() operates on for the production buffer."""
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
            returns = episode_returns(my_scores, opp_scores, reward, GAMMA)

            X73 = np.zeros((len(idxs), 73))
            Xc = np.zeros((len(idxs), len(CANDIDATE_NAMES)))
            for i, k in enumerate(idxs):
                turn = g['turns'][k]
                X73[i] = extract_f73(g, turn)
                b = Board(turn)
                Xc[i] = candidate_features(b, table, persp)

            episodes.append(dict(
                game=g['_file'], perspective=persp, G=np.array(returns),
                X73=X73, Xc=Xc, n=len(idxs),
            ))
        if (gi + 1) % 25 == 0:
            print(f'  ...{gi + 1}/{len(games)} games reconstructed', flush=True)
    return episodes


def cmd_run(label: str) -> None:
    games = list(load_games(label))
    print(f'reconstructing f73 + candidates for {len(games)} games '
          f'(this calls bb_engine.extract_features per snapshot, ~4800 calls)...')
    episodes = build_episodes(games)
    n_states = sum(e['n'] for e in episodes)
    print(f'DONE: {len(episodes)} episodes, {n_states} total states')
    with open(CACHE_PATH, 'wb') as f:
        pickle.dump(episodes, f)
    print(f'cached -> {CACHE_PATH}')



# ---------------------------------------------------------------------------
# 4. Ridge fit: G ~ f73 (baseline) vs G ~ f73+candidates
#    Same methodology/thresholds as diag_capacity_probe.py (2026-07-15,
#    other agent thread): episode-level split, RidgeCV, held-out MSE/R2,
#    within-episode std of predictions, within-episode R2 (both pred and
#    label centered per episode -- the sharpest test of "captures within-
#    game structure vs just per-episode offsets"), between-episode R2.
# ---------------------------------------------------------------------------

def ep_pred_std(preds_per_ep) -> float:
    stds = [p.std() for p in preds_per_ep if len(p) >= 3]
    return float(np.mean(stds)) if stds else 0.0


def within_ep_r2(labels_per_ep, preds_per_ep) -> float:
    num = den = 0.0
    for y, p in zip(labels_per_ep, preds_per_ep):
        if len(y) < 3:
            continue
        yc, pc = y - y.mean(), p - p.mean()
        num += float(((yc - pc) ** 2).sum())
        den += float((yc ** 2).sum())
    return 1.0 - num / den if den > 0 else float('nan')


def within_ep_corr(labels_per_ep, preds_per_ep) -> float:
    ys, ps = [], []
    for y, p in zip(labels_per_ep, preds_per_ep):
        if len(y) < 3:
            continue
        ys.append(y - y.mean())
        ps.append(p - p.mean())
    if not ys:
        return float('nan')
    return float(np.corrcoef(np.concatenate(ys), np.concatenate(ps))[0, 1])


def between_ep_r2(labels_per_ep, preds_per_ep) -> float:
    ym = np.array([y.mean() for y in labels_per_ep])
    pm = np.array([p.mean() for p in preds_per_ep])
    return 1.0 - float(np.mean((ym - pm) ** 2)) / float(np.var(ym))


def one_fit(episodes: list, seed: int, verbose: bool = True) -> dict:
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

    def stack_G(ids):
        return np.concatenate([episodes[i]['G'] for i in ids])

    X73_tr, X73_te = stack(tr_ids, 'X73'), stack(te_ids, 'X73')
    Xc_tr, Xc_te = stack(tr_ids, 'Xc'), stack(te_ids, 'Xc')
    Gtr, Gte = stack_G(tr_ids), stack_G(te_ids)
    Xcomb_tr = np.hstack([X73_tr, Xc_tr])
    Xcomb_te = np.hstack([X73_te, Xc_te])

    te_slices, k = [], 0
    for i in te_ids:
        m = episodes[i]['n']
        te_slices.append((k, k + m))
        k += m
    te_G_labels = [Gte[a:b] for a, b in te_slices]

    scaler73 = StandardScaler().fit(X73_tr)
    scalerC = StandardScaler().fit(Xcomb_tr)
    X73_tr_s, X73_te_s = scaler73.transform(X73_tr), scaler73.transform(X73_te)
    Xc_tr_s, Xc_te_s = scalerC.transform(Xcomb_tr), scalerC.transform(Xcomb_te)

    alphas = np.logspace(-4, 3, 15)
    ridge_base = RidgeCV(alphas=alphas).fit(X73_tr_s, Gtr)
    ridge_comb = RidgeCV(alphas=alphas).fit(Xc_tr_s, Gtr)

    p_base = ridge_base.predict(X73_te_s)
    p_comb = ridge_comb.predict(Xc_te_s)

    def report(name, pred):
        mse_te = float(np.mean((pred - Gte) ** 2))
        r2_te = 1.0 - mse_te / float(np.var(Gte))
        preds_per_ep = [pred[a:b] for a, b in te_slices]
        eps = ep_pred_std(preds_per_ep)
        wr2 = within_ep_r2(te_G_labels, preds_per_ep)
        wcorr = within_ep_corr(te_G_labels, preds_per_ep)
        br2 = between_ep_r2(te_G_labels, preds_per_ep)
        return dict(mse_te=mse_te, r2_te=r2_te, ep_std=eps, wr2=wr2,
                   wcorr=wcorr, br2=br2, pred=pred)

    r_base = report('baseline (f73)', p_base)
    r_comb = report('baseline+candidates (f80)', p_comb)

    dv = float(np.abs(p_comb - p_base).mean())
    corr_bc = float(np.corrcoef(p_base, p_comb)[0, 1])
    label_ep_std = ep_pred_std(te_G_labels)

    if verbose:
        print(f'  seed={seed}  episodes tr/te={len(tr_ids)}/{len(te_ids)}  '
              f'states tr/te={len(Gtr)}/{len(Gte)}  '
              f'ridge_alpha base/comb={ridge_base.alpha_:g}/{ridge_comb.alpha_:g}')
        print(f'    label G: ep-std(test) = {label_ep_std:.4f}')
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

    # feature distinctness sanity (can any function separate within-episode states?)
    uniq_fracs = []
    for e in episodes:
        if e['n'] < 3:
            continue
        X = np.hstack([e['X73'], e['Xc']])
        uniq = len(np.unique(np.round(X, 9), axis=0))
        uniq_fracs.append(uniq / e['n'])
    print(f'feature distinctness: unique-rows/episode-len mean = {np.mean(uniq_fracs):.3f}\n')

    # candidate feature prevalence over the whole dataset (not just test fold)
    Xc_all = np.vstack([e['Xc'] for e in episodes])
    print('candidate feature summary (all 300 episodes):')
    for name, col in zip(CANDIDATE_NAMES, Xc_all.T):
        nz = (col != 0).mean()
        print(f'  {name:30s} mean={col.mean():.3f} nonzero={nz:.1%}')
    print()

    results = [one_fit(episodes, seed) for seed in seeds]

    print(f'\n=== summary over {len(seeds)} seeds ===')
    metrics = {
        'dMSE (comb-base)': [r['comb']['mse_te'] - r['base']['mse_te'] for r in results],
        'dR2 (comb-base)': [r['comb']['r2_te'] - r['base']['r2_te'] for r in results],
        'd(ep-std)': [r['comb']['ep_std'] - r['base']['ep_std'] for r in results],
        'd(within-ep-R2)': [r['comb']['wr2'] - r['base']['wr2'] for r in results],
        'mean|dV|': [r['dv'] for r in results],
        'corr(base,comb)': [r['corr_bc'] for r in results],
    }
    for label, vals in metrics.items():
        m, s = float(np.mean(vals)), float(np.std(vals))
        print(f'  {label:20s} mean={m:+.4f}  std={s:.4f}  '
              f'seeds={[round(v, 4) for v in vals]}')

    label_stds = [r['label_ep_std'] for r in results]
    base_stds = [r['base']['ep_std'] for r in results]
    comb_stds = [r['comb']['ep_std'] for r in results]
    print(f'\n  label ep-std (ceiling): {np.mean(label_stds):.4f}  '
          f'baseline ep-std: {np.mean(base_stds):.4f}  '
          f'combined ep-std: {np.mean(comb_stds):.4f}')
    print(f'  ep-std recovered by baseline: {np.mean(base_stds)/np.mean(label_stds):.1%} of ceiling; '
          f'by combined: {np.mean(comb_stds)/np.mean(label_stds):.1%} of ceiling')


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'run'
    if cmd == 'run':
        cmd_run(sys.argv[2] if len(sys.argv) > 2 else 'main')
    elif cmd == 'fit':
        seeds = [int(x) for x in sys.argv[2:]] if len(sys.argv) > 2 else \
            [20260715, 20260716, 20260717, 20260718, 20260719]
        cmd_fit(seeds)
    else:
        print(f'unknown command {cmd}; use "run" or "fit"')
