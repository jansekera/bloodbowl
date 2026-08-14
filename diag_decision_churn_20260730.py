#!/usr/bin/env python3
"""Track-2: situacni parove srovnani dvou sad vah na urovni ROZHODNUTI.

Motivace (2026-07-30): HtH ~50 % je ocekavana signatura maleho realneho
zlepseni -- agregat nic nedokaze. Tento nastroj meri "decision churn":
jak casto kandidat na IDENTICKE pozici (stejny stav, stejny RNG stream)
zvoli jine makro nez sampion.

METODA -- parovy common-random-numbers divergence probing:
  Engine nema binding "vyhodnot libovolnou pozici danymi vahami", ale hry
  jsou pri fixnim seedu plne deterministicke (overeno: 2 behy tehoz configu
  daji bit-identicke sekvence stavu i skore). Pro kazdy seed se hraji:
    REF : sampion(home) vs sampion(away)
    CH  : kandidat(home) vs sampion(away)   -- stejny seed
    CA  : sampion(home) vs kandidat(away)   -- stejny seed
  Dokud kandidat voli stejne jako sampion, trajektorie CH je bit-identicka
  s REF (druha strana ma stejne vahy i RNG). Prvni rozdil ve stavove
  sekvenci kandidatovy strany = prvni zmenene rozhodnuti; stav i RNG byly
  do te chvile IDENTICKE, takze rozdil je 100% atribuovatelny vaham
  (zadny sum -- common random numbers). Kazde kandidatovo rozhodnuti pred
  divergenci je Bernoulli trial "zmenil by kandidat volbu?", divergence je
  event -> per-decision churn hazard + binomialni CI.

  Inference KONFIGURACE ZRCADLI GATE (run_iteration.py _gate_game):
  mcts=100, vf_blend=GATE_VF_BLEND=0.15, epsilon=0, dirichlet_alpha=0.0,
  exploration_c=1.0, policy_weights_path=weights_policy.json (sdilena
  policy-prior sit pro OBE strany, policy_blend=0 -> jen heuristicke prior
  floors), TV=1200, race pary (i, i+1) z _RACES, side-swap (CH i CA).

POZOR: nastroj NIC netrenuje a NEZAPISUJE do zadnych produkcnich souboru;
vystupy jdou do diag_decision_churn_data/.

Pouziti:
  python3 diag_decision_churn_20260730.py run \
      --champ weights_best.json --cand weights_snap_e2_100pct_+2.9.json \
      --label pair1_e2 [--seeds 24] [--base-seed 20260730] [--workers 4]
  python3 diag_decision_churn_20260730.py analyze --label pair1_e2
  (run po dobehnuti spousti analyze automaticky; run je resumovatelny --
   hotove hry se preskakuji)
"""
import argparse
import gzip
import hashlib
import json
import math
import os
import sys
import time
from collections import Counter, defaultdict
from multiprocessing import Pool
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent
ENGINE_BUILD = str(PROJECT_ROOT / 'engine' / 'build')
DATA_ROOT = PROJECT_ROOT / 'diag_decision_churn_data'

# --- gate-mirror konstanty (run_iteration.py) ---
RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']
TV = 1200
GATE_VF_BLEND = 0.15
GATE_DIRICHLET_ALPHA = 0.0
GATE_EXPLORATION_C = 1.0
DEFAULT_MCTS = 100
DEFAULT_POLICY = 'weights_policy.json'

# extractMacroFeatures: [0-9] one-hot MacroType (BLITZ_AND_SCORE->BLITZ,
# {HAND_OFF,PASS,CHAIN}_SCORE->SCORE), [10] scoring_potential,
# [11] block_dice_quality, [12] player_strength/7, [13] risk_level
MACRO_NAMES = ['SCORE', 'ADVANCE', 'CAGE', 'BLITZ', 'BLOCK', 'PICKUP',
               'PASS', 'FOUL', 'REPOSITION', 'END_TURN']


def _decode_visit(v):
    af = v['action_features']
    one_hot = [float(af[i]) for i in range(10)]
    mi = max(range(10), key=lambda i: one_hot[i]) if max(one_hot) > 0.5 else -1
    return {
        'macro': MACRO_NAMES[mi] if mi >= 0 else 'UNKNOWN',
        'vf': round(float(v['visit_fraction']), 4),
        'scoring': round(float(af[10]), 3),
        'dice': round(float(af[11]), 3),
        'st': round(float(af[12]), 3),
        'risk': round(float(af[13]), 3),
    }


def _game_worker(args):
    label, kind, seed, race_idx, home_w, away_w, mcts, vf_blend, policy, out_path = args
    out_path = Path(out_path)
    if out_path.exists():
        return (kind, seed, 'skip', 0.0)
    sys.path.insert(0, ENGINE_BUILD)
    import bb_engine
    hr = bb_engine.get_developed_roster(RACES[race_idx % len(RACES)], TV)
    ar = bb_engine.get_developed_roster(RACES[(race_idx + 1) % len(RACES)], TV)
    t0 = time.time()
    lgr = bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=vf_blend,
        policy_weights_path=policy,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    )
    dt = time.time() - t0
    decs = lgr.get_policy_decisions()

    def compact(d):
        return {
            'h': hashlib.md5(d['state_features'].tobytes()).hexdigest(),
            'ball': [int(d['ball_x']), int(d['ball_y']), bool(d['ball_held']),
                     int(d['ball_carrier_id'])],
            'visits': [_decode_visit(v) for v in d['visits'][:8]],
            'board': {
                'home': [[p['id'], p['x'], p['y'], p['state'], int(p['has_ball'])]
                         for p in d['home_players']],
                'away': [[p['id'], p['x'], p['y'], p['state'], int(p['has_ball'])]
                         for p in d['away_players']],
            },
        }

    rec = {
        'meta': {'label': label, 'kind': kind, 'seed': seed, 'race_idx': race_idx,
                 'races': [RACES[race_idx % 5], RACES[(race_idx + 1) % 5]],
                 'home_w': home_w, 'away_w': away_w, 'mcts': mcts,
                 'vf_blend': vf_blend, 'policy': policy,
                 'score': [int(lgr.result.home_score), int(lgr.result.away_score)],
                 'game_seconds': round(dt, 1)},
        'home': [compact(d) for d in decs if d['perspective'] == 'home'],
        'away': [compact(d) for d in decs if d['perspective'] == 'away'],
    }
    tmp = out_path.with_suffix('.tmp')
    with gzip.open(tmp, 'wt') as f:
        json.dump(rec, f)
    os.replace(tmp, out_path)
    return (kind, seed, 'done', dt)


def _ref_path(champ, seed, race_idx, mcts, vf_blend, policy):
    key = hashlib.md5('|'.join(map(str, (
        Path(champ).name, seed, race_idx, mcts, vf_blend, Path(policy).name
    ))).encode()).hexdigest()[:12]
    d = DATA_ROOT / 'ref'
    return d / f'ref_{Path(champ).stem}_s{seed}_r{race_idx}_{key}.json.gz'


def cmd_run(a):
    label_dir = DATA_ROOT / a.label
    (DATA_ROOT / 'ref').mkdir(parents=True, exist_ok=True)
    label_dir.mkdir(parents=True, exist_ok=True)
    tasks = []
    for i in range(a.seeds):
        seed = a.base_seed + i
        race_idx = i
        rp = _ref_path(a.champ, seed, race_idx, a.mcts, a.vf_blend, a.policy)
        tasks.append((a.label, 'ref', seed, race_idx, a.champ, a.champ,
                      a.mcts, a.vf_blend, a.policy, str(rp)))
        tasks.append((a.label, 'ch', seed, race_idx, a.cand, a.champ,
                      a.mcts, a.vf_blend, a.policy,
                      str(label_dir / f'ch_s{seed}_r{race_idx}.json.gz')))
        tasks.append((a.label, 'ca', seed, race_idx, a.champ, a.cand,
                      a.mcts, a.vf_blend, a.policy,
                      str(label_dir / f'ca_s{seed}_r{race_idx}.json.gz')))
    todo = [t for t in tasks if not Path(t[9]).exists()]
    print(f'[{a.label}] {len(tasks)} her, {len(todo)} ke spusteni '
          f'({a.workers} workeru, mcts={a.mcts}, vf_blend={a.vf_blend})', flush=True)
    if todo:
        done = 0
        with Pool(a.workers) as pool:
            for kind, seed, st, dt in pool.imap_unordered(_game_worker, todo):
                done += 1
                print(f'  [{a.label}] {done}/{len(todo)} {kind} seed={seed} '
                      f'{st} {dt:.0f}s', flush=True)
    meta = {'label': a.label, 'champ': a.champ, 'cand': a.cand,
            'seeds': a.seeds, 'base_seed': a.base_seed, 'mcts': a.mcts,
            'vf_blend': a.vf_blend, 'policy': a.policy}
    with open(label_dir / 'meta.json', 'w') as f:
        json.dump(meta, f, indent=1)
    cmd_analyze(a)


def _load(path):
    with gzip.open(path, 'rt') as f:
        return json.load(f)


def _dist_summary(xs):
    if not xs:
        return None
    s = sorted(xs)
    n = len(s)
    return {'n': n, 'mean': round(sum(s) / n, 4), 'median': round(s[n // 2], 4),
            'p90': round(s[int(0.9 * n)] if n > 1 else s[0], 4)}


def _wilson(k, n, z=1.96):
    if n == 0:
        return (0.0, 0.0, 1.0)
    p = k / n
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    hw = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return (p, (c - hw) / d, (c + hw) / d)


def _sig(v):
    """Podpis zvoleneho makra pro porovnani (typ + diskretni featury)."""
    return (v['macro'], v['scoring'], v['dice'], v['st'], v['risk'])


def _macro_marginal(visits):
    m = defaultdict(float)
    for v in visits:
        m[v['macro']] += v['vf']
    return m


def _classify(champ_v, cand_v):
    """better/same/worse heuristika: bezpecnost pri zachovanem skorovacim
    potencialu (objektivni jen ve smyslu deklarovanych risk/scoring featur)."""
    dr = cand_v['risk'] - champ_v['risk']
    ds = cand_v['scoring'] - champ_v['scoring']
    if abs(dr) < 0.01 and abs(ds) < 0.01:
        return 'equal_features'
    if dr < -0.01 and ds > -0.01:
        return 'safer_same_scoring'
    if dr > 0.01 and ds < 0.01:
        return 'riskier_no_gain'
    if ds > 0.01:
        return 'more_scoring_more_risk' if dr > 0.01 else 'more_scoring'
    return 'less_scoring'


def _compare_side(ref_seq, cand_seq):
    """Vrati (trials, event, divergence_index, detail) pro kandidatovu stranu.

    Hashe stavu jsou shodne na indexech 0..d-1 a lisi se na d
    => zmenene rozhodnuti je na indexu d-1 (stav d nese jeho dusledek).
    trials = d (kandidat ucinil d rozhodnuti na identickych stavech),
    event = 1 pokud divergence nastala.
    """
    n = min(len(ref_seq), len(cand_seq))
    d = None
    for i in range(n):
        if ref_seq[i]['h'] != cand_seq[i]['h']:
            d = i
            break
    if d is None:
        if len(ref_seq) != len(cand_seq):
            # stejny prefix, jina delka -> posledni spolecne rozhodnuti se lisilo
            d = n
        else:
            return (n, 0, None, None)
    if d == 0:
        return (0, 0, None, {'anomaly': 'diverged_at_state_0'})
    ci = d - 1  # index zmeneneho rozhodnuti
    a, b = ref_seq[ci], cand_seq[ci]
    detail = {
        'decision_index': ci,
        'ball': a['ball'],
        'champ_top': a['visits'][:5],
        'cand_top': b['visits'][:5],
        'sub_feature': _sig(a['visits'][0]) == _sig(b['visits'][0]),
        'board': a['board'],
    }
    return (d, 1, ci, detail)


def cmd_analyze(a):
    label_dir = DATA_ROOT / a.label
    meta = json.load(open(label_dir / 'meta.json'))
    champ, cand = meta['champ'], meta['cand']
    rows = []
    events = []
    trials_tot = ev_tot = 0
    identical_games = games = 0
    matched_tv = []          # soft churn: TV vzdalenost makro-marginal na shodnych pozicich
    matched_margins = []     # baseline: top1-top2 visit margin sampiona na shodnych pozicich
    div_margins = []         # top1-top2 margin sampiona na pozicich, kde kandidat volbu zmenil
    macro_trans = Counter()  # (champ_macro -> cand_macro) na divergencich
    classes = Counter()
    ctx = Counter()
    score_flips = Counter()
    for i in range(meta['seeds']):
        seed = meta['base_seed'] + i
        race_idx = i
        rp = _ref_path(champ, seed, race_idx, meta['mcts'], meta['vf_blend'], meta['policy'])
        for kind, side in (('ch', 'home'), ('ca', 'away')):
            gp = label_dir / f'{kind}_s{seed}_r{race_idx}.json.gz'
            if not rp.exists() or not gp.exists():
                continue
            ref, g = _load(rp), _load(gp)
            games += 1
            trials, ev, ci, detail = _compare_side(ref[side], g[side])
            trials_tot += trials
            ev_tot += ev
            # soft churn na pozicich pred divergenci
            upto = ci if ci is not None else trials
            for j in range(upto):
                ma = _macro_marginal(ref[side][j]['visits'])
                mb = _macro_marginal(g[side][j]['visits'])
                keys = set(ma) | set(mb)
                matched_tv.append(0.5 * sum(abs(ma[k] - mb[k]) for k in keys))
                vs = ref[side][j]['visits']
                if len(vs) >= 2:
                    matched_margins.append(vs[0]['vf'] - vs[1]['vf'])
            if detail and 'anomaly' not in detail and len(detail['champ_top']) >= 2:
                div_margins.append(detail['champ_top'][0]['vf'] - detail['champ_top'][1]['vf'])
            rs, gs = ref['meta']['score'], g['meta']['score']
            if ev == 0 and trials == len(ref[side]):
                identical_games += 1
            else:
                # zmena vysledku hry z pohledu kandidatovy strany (ne kauzalni per-decision, jen kontext)
                def outcome(sc, side_):
                    d_ = sc[0] - sc[1] if side_ == 'home' else sc[1] - sc[0]
                    return 'W' if d_ > 0 else ('L' if d_ < 0 else 'D')
                score_flips[(outcome(rs, side), outcome(gs, side))] += 1
            if detail and 'anomaly' not in detail:
                cm = detail['champ_top'][0]['macro']
                nm = detail['cand_top'][0]['macro']
                macro_trans[(cm, nm)] += 1
                cls = ('sub_feature' if detail['sub_feature']
                       else _classify(detail['champ_top'][0], detail['cand_top'][0]))
                classes[cls] += 1
                bx, by, held, carrier = detail['ball']
                mine = detail['board'][side]
                free_ball = (not held) and bx >= 0
                carrier_mine = held and any(p[0] == carrier for p in mine)
                tag = ('free_ball' if free_ball else
                       ('we_have_ball' if carrier_mine else 'opp_has_ball' if held else 'no_ball'))
                ctx[tag] += 1
                events.append({'seed': seed, 'kind': kind, 'side': side,
                               'races': ref['meta']['races'], 'class': cls,
                               'context': tag, 'ref_score': rs, 'cand_score': gs,
                               **detail})
            rows.append({'seed': seed, 'kind': kind, 'trials': trials, 'event': ev,
                         'div_index': ci,
                         'ref_len': len(ref[side]), 'cand_len': len(g[side])})
    p, lo, hi = _wilson(ev_tot, trials_tot)
    summary = {
        'label': a.label, 'champ': champ, 'cand': cand,
        'config': {k: meta[k] for k in ('mcts', 'vf_blend', 'policy', 'seeds', 'base_seed')},
        'games_compared': games,
        'identical_games': identical_games,
        'identical_games_pct': round(100 * identical_games / games, 1) if games else None,
        'paired_decisions': trials_tot,
        'churn_events': ev_tot,
        'churn_hazard_per_decision': {'p': round(p, 4), 'ci95': [round(lo, 4), round(hi, 4)]},
        'expected_decisions_to_first_change': round(1 / p, 1) if p > 0 else None,
        'soft_churn_tv_matched': {
            'n': len(matched_tv),
            'mean': round(sum(matched_tv) / len(matched_tv), 4) if matched_tv else None,
            'p90': round(sorted(matched_tv)[int(0.9 * len(matched_tv))], 4) if matched_tv else None,
        },
        'champ_top1_margin': {
            # margin = jistota sampiona (vf top1 - vf top2); nizky margin u divergenci
            # => kandidat preklapi jen "coin-toss" pozice, ne jista rozhodnuti
            'at_divergences': _dist_summary(div_margins),
            'baseline_matched': _dist_summary(matched_margins),
            'div_share_margin_lt_002': (round(sum(1 for m in div_margins if m < 0.02)
                                              / len(div_margins), 3) if div_margins else None),
            'baseline_share_margin_lt_002': (round(sum(1 for m in matched_margins if m < 0.02)
                                                   / len(matched_margins), 3)
                                             if matched_margins else None),
        },
        'divergence_classes': dict(classes),
        'context_tags': dict(ctx),
        'macro_transitions': {f'{k[0]}->{k[1]}': v for k, v in macro_trans.most_common()},
        'game_outcome_ref_vs_cand': {f'{k[0]}->{k[1]}': v for k, v in score_flips.most_common()},
        'per_game': rows,
    }
    out = label_dir / 'summary.json'
    with open(out, 'w') as f:
        json.dump(summary, f, indent=1)
    with gzip.open(label_dir / 'events.json.gz', 'wt') as f:
        json.dump(events, f)
    print(json.dumps({k: v for k, v in summary.items() if k not in ('per_game',)},
                     indent=1, ensure_ascii=False))
    print(f'-> {out} (+ events.json.gz s board snapshoty divergenci)')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='cmd', required=True)
    for name in ('run', 'analyze'):
        p = sub.add_parser(name)
        p.add_argument('--label', required=True)
        if name == 'run':
            p.add_argument('--champ', required=True)
            p.add_argument('--cand', required=True)
            p.add_argument('--seeds', type=int, default=24)
            p.add_argument('--base-seed', type=int, default=20260730)
            p.add_argument('--workers', type=int, default=4)
            p.add_argument('--mcts', type=int, default=DEFAULT_MCTS)
            p.add_argument('--vf-blend', type=float, default=GATE_VF_BLEND)
            p.add_argument('--policy', default=DEFAULT_POLICY)
    a = ap.parse_args()
    os.chdir(PROJECT_ROOT)
    if a.cmd == 'run':
        cmd_run(a)
    else:
        cmd_analyze(a)


if __name__ == '__main__':
    main()
