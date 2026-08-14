#!/usr/bin/env python3
"""A2-3 oracle judge: jsou divergentni volby kandidata LEPSI/HORSI/EKVIVALENTNI?

Vstup: divergence z diag_decision_churn_data/ (events.json.gz + ulozene hry).
Pro divergenci na indexu ci je stav ci identicky v obou vetvich; stav ci+1 je
dusledek sampionovy volby (REF hra) resp. kandidatovy volby (CH/CA hra).
Makra nejsou pres bindingy spustitelna a macro-MCTS nejde pustit z libovolneho
stavu, proto oracle = parove rollouty z REKONSTRUOVANYCH post-stavu ci+1:

  - GameState() + setup_half(rostery race-paru) -> hraci vc. skillu (ID mapovani
    shodne s puvodnimi hrami), pak prepis pozic/stavu dle board snapshotu,
    mic, active_team, phase=PLAY; imposed clock half=2, aktivni strana turn=5
    (soft horizon ~4 tahy/strana), skore 0:0, fresh-turn reset -- STEJNE pro
    obe vetve (parovy rozdil sdilene neznamé neutralizuje).
  - K parovych rolloutu se STEJNYMI dice seedy v obou vetvich (CRN);
    rollout politika = presna replika greedyPolicy (policies.cpp) pro obe
    strany obou vetvi -- fixni, na sampionovi NEZAVISLY playout (VF-greedy
    pilot nikdy neskoroval v horizontu -> z bez signalu; greedy skoruje).
    Terminalni stav se hodnoti sampionovou VF (sekundarni metrika).
  - Metriky per rollout: z = skore-diff z pohledu kandidatovy strany (stop na
    prvnim TD -> "kdo skoruje prvni"), terminalni VF, turnovery vlastni strany.
  - Verdikt: parovy t-test Delta z; prah kalibrovan A/A testem (2 nezavisle
    sady rollout seedu na TEMZE stavu).

NIC netrenuje, NEZAPISUJE do produkcnich souboru, engine se NEREBUILDUJE.
Vystup: diag_oracle_judge_20260730_results.json (inkrementalni, resumovatelne).

Pouziti:
  python3 diag_oracle_judge_20260730.py run [--workers 4] [--k 16] [--pilot N]
  python3 diag_oracle_judge_20260730.py analyze
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

import numpy as np

PROJECT_ROOT = Path(__file__).resolve().parent
ENGINE_BUILD = str(PROJECT_ROOT / 'engine' / 'build')
DATA_ROOT = PROJECT_ROOT / 'diag_decision_churn_data'
RESULTS = PROJECT_ROOT / 'diag_oracle_judge_20260730_results.json'

RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']
TV = 1200
LABELS = ('pair1_e2', 'pair2_e16')
MAX_STEPS = 700
AA_EVENTS = 16          # na kolika eventech spustit A/A kalibraci (champ vetev)
ROLL_SEED_BASE = 977001  # sada A; sada B = +K


def _ref_path(champ, seed, race_idx, mcts, vf_blend, policy):
    key = hashlib.md5('|'.join(map(str, (
        Path(champ).name, seed, race_idx, mcts, vf_blend, Path(policy).name
    ))).encode()).hexdigest()[:12]
    return DATA_ROOT / 'ref' / f'ref_{Path(champ).stem}_s{seed}_r{race_idx}_{key}.json.gz'


def _load_gz(p):
    with gzip.open(p, 'rt') as f:
        return json.load(f)


class VF:
    """Numpy replika NeuralValueFunction (ReLU hidden, tanh out)."""

    def __init__(self, path):
        w = json.load(open(path))
        self.W1 = np.asarray(w['value_W1'], dtype=np.float32)          # [F][H]
        self.b1 = np.asarray(w['value_b1'], dtype=np.float32)
        self.W2 = np.asarray(w['value_W2'], dtype=np.float32).ravel()
        b2 = w['value_b2']
        self.b2 = float(b2[0] if isinstance(b2, list) else b2)

    def eval(self, f):
        f = np.asarray(f, dtype=np.float32)[:self.W1.shape[0]]
        h = np.maximum(f @ self.W1 + self.b1, 0.0)
        return float(np.tanh(h @ self.W2 + self.b2))


_ENGINE = {}


def _engine():
    if 'bb' not in _ENGINE:
        sys.path.insert(0, ENGINE_BUILD)
        import bb_engine
        _ENGINE['bb'] = bb_engine
        _ENGINE['vf'] = VF(str(PROJECT_ROOT / 'weights_best.json'))
    return _ENGINE['bb'], _ENGINE['vf']


def build_state(bb, board, ball, side, race_idx):
    hr = bb.get_developed_roster(RACES[race_idx % 5], TV)
    ar = bb.get_developed_roster(RACES[(race_idx + 1) % 5], TV)
    gs = bb.GameState()
    bb.setup_half(gs, hr, ar, bb.TeamSide.AWAY)
    st_enum = [bb.PlayerState.STANDING, bb.PlayerState.PRONE, bb.PlayerState.STUNNED]
    for pid in range(1, 23):
        p = gs.get_player(pid)
        p.state = bb.PlayerState.OFF_PITCH
        p.position = bb.Position(-1, -1)
        p.has_moved = False
        p.has_acted = False
    for tk in ('home', 'away'):
        for pid, x, y, st, _hb in board[tk]:
            p = gs.get_player(pid)
            p.position = bb.Position(x, y)
            p.state = st_enum[st] if 0 <= st <= 2 else bb.PlayerState.OFF_PITCH
            p.movement_remaining = p.stats.movement
            p.has_moved = False
            p.has_acted = False
    bx, by, held, carrier = ball
    if bx >= 0:
        gs.ball.position = bb.Position(bx, by)
        gs.ball.is_held = bool(held)
        gs.ball.carrier_id = int(carrier) if held else -1
    active = bb.TeamSide.HOME if side == 'home' else bb.TeamSide.AWAY
    gs.active_team = active
    gs.phase = bb.GamePhase.PLAY
    gs.half = 2
    my = gs.home_team if side == 'home' else gs.away_team
    opp = gs.away_team if side == 'home' else gs.home_team
    my.turn_number = 5
    opp.turn_number = 4   # ++ na 5 pri prevzeti tahu (konvence advanceTurn)
    gs.home_team.score = 0
    gs.away_team.score = 0
    gs.kicking_team = bb.TeamSide.AWAY if side == 'home' else bb.TeamSide.HOME
    return gs


def greedy_action(bb, gs, actions, dice):
    """Presna replika bb::greedyPolicy (policies.cpp)."""
    my = gs.active_team
    ball = gs.ball
    # P1: carrier smerem k endzone
    if ball.is_held and ball.carrier_id > 0:
        carrier = gs.get_player(ball.carrier_id)
        if carrier.team_side == my and carrier.state == bb.PlayerState.STANDING:
            target_x = 25 if my == bb.TeamSide.HOME else 0
            dx = 1 if target_x > carrier.position.x else -1
            for a in actions:
                if a.type == bb.ActionType.MOVE and a.player_id == carrier.id:
                    if ((dx > 0 and a.target.x > carrier.position.x)
                            or (dx < 0 and a.target.x < carrier.position.x)):
                        return a
    # P2: k volnemu micu
    if not ball.is_held and ball.is_on_pitch():
        bp = ball.position
        for a in actions:
            if (a.type == bb.ActionType.MOVE
                    and a.target.x == bp.x and a.target.y == bp.y):
                return a
        best, bestd = None, 999
        for a in actions:
            if a.type == bb.ActionType.MOVE:
                dist = a.target.distance_to(bp)
                if dist < bestd:
                    bestd, best = dist, a
        if best is not None:
            return best
    # P3: blocky (nahodne)
    blocks = [a for a in actions if a.type == bb.ActionType.BLOCK]
    if blocks:
        r = (dice.roll_d6() - 1) % len(blocks) if len(blocks) > 1 else 0
        return blocks[r]
    # P4: prvni blitz
    for a in actions:
        if a.type == bb.ActionType.BLITZ:
            return a
    # P5: nahodny move
    moves = [a for a in actions if a.type == bb.ActionType.MOVE]
    if moves:
        r = (dice.roll_d6() - 1) % len(moves) if len(moves) > 1 else 0
        return moves[r]
    return actions[0]


def rollout(bb, vf, gs0, side, seed):
    """Jeden greedy rollout; vraci (z, terminal_vf, my_turnovers, steps)."""
    gs = gs0.clone()
    dice = bb.DiceRoller(seed)
    my_side = bb.TeamSide.HOME if side == 'home' else bb.TeamSide.AWAY
    end = bb.Action()
    end.type = bb.ActionType.END_TURN
    end.player_id = -1
    my_to = 0
    steps = 0
    while gs.phase == bb.GamePhase.PLAY and steps < MAX_STEPS:
        actions = bb.get_available_actions(gs)
        persp = gs.active_team
        if not actions:
            bb.execute_action(gs, end, dice)
            steps += 1
            continue
        best = greedy_action(bb, gs, actions, dice)
        res = bb.execute_action(gs, best, dice)
        if res.turnover and persp == my_side:
            my_to += 1
        steps += 1
    sd = gs.home_team.score - gs.away_team.score
    z = sd if side == 'home' else -sd
    tvf = vf.eval(bb.extract_features(gs, my_side))
    return z, tvf, my_to, steps


def _judge_worker(task):
    key, label, meta, ev, k, do_aa = task
    bb, vf = _engine()
    seed, kind, side, ci = ev['seed'], ev['kind'], ev['side'], ev['decision_index']
    race_idx = seed - meta['base_seed']
    rp = _ref_path(meta['champ'], seed, race_idx, meta['mcts'],
                   meta['vf_blend'], meta['policy'])
    gp = DATA_ROOT / label / f'{kind}_s{seed}_r{race_idx}.json.gz'
    ref, g = _load_gz(rp), _load_gz(gp)
    d = ci + 1
    if d >= len(ref[side]) or d >= len(g[side]):
        return {'key': key, 'skip': 'no_post_state'}
    t0 = time.time()
    out = {'key': key, 'label': label, 'seed': seed, 'kind': kind, 'side': side,
           'decision_index': ci, 'class': ev['class'], 'context': ev['context'],
           'champ_macro': ev['champ_top'][0]['macro'],
           'cand_macro': ev['cand_top'][0]['macro']}
    branches = {}
    for name, rec in (('champ', ref[side][d]), ('cand', g[side][d])):
        gs0 = build_state(bb, rec['board'], rec['ball'], side, race_idx)
        zs, tvfs, tos, stps = [], [], [], []
        for r in range(k):
            z, tvf, to, stp = rollout(bb, vf, gs0, side, ROLL_SEED_BASE + r)
            zs.append(z)
            tvfs.append(round(tvf, 4))
            tos.append(to)
            stps.append(stp)
        branches[name] = {'z': zs, 'tvf': tvfs, 'to': tos, 'steps_mean': round(sum(stps) / k, 1)}
        if name == 'champ' and do_aa:
            zsB = [rollout(bb, vf, gs0, side, ROLL_SEED_BASE + k + r)[0] for r in range(k)]
            out['aa_zB'] = zsB
    out['branches'] = branches
    out['seconds'] = round(time.time() - t0, 1)
    return out


def _load_results():
    if RESULTS.exists():
        return json.load(open(RESULTS))
    return {'meta': {'k': None, 'roll_seed_base': ROLL_SEED_BASE,
                     'max_steps': MAX_STEPS, 'started': time.strftime('%F %T')},
            'events': {}}


def _save_results(res):
    tmp = RESULTS.with_suffix('.tmp')
    with open(tmp, 'w') as f:
        json.dump(res, f)
    os.replace(tmp, RESULTS)


def cmd_run(a):
    res = _load_results()
    res['meta']['k'] = a.k
    tasks = []
    aa_budget = AA_EVENTS
    for label in LABELS:
        meta = json.load(open(DATA_ROOT / label / 'meta.json'))
        events = _load_gz(DATA_ROOT / label / 'events.json.gz')
        for ev in events:
            key = f"{label}:{ev['kind']}:s{ev['seed']}"
            if key in res['events']:
                continue
            do_aa = aa_budget > 0
            if do_aa:
                aa_budget -= 1
            tasks.append((key, label, meta, ev, a.k, do_aa))
    if a.pilot:
        tasks = tasks[:a.pilot]
    print(f'{len(tasks)} eventu ke zpracovani (k={a.k}, workers={a.workers})', flush=True)
    done = 0
    t0 = time.time()
    with Pool(a.workers) as pool:
        for out in pool.imap_unordered(_judge_worker, tasks):
            res['events'][out['key']] = out
            _save_results(res)
            done += 1
            print(f"  {done}/{len(tasks)} {out['key']} "
                  f"{out.get('seconds', '?')}s {out.get('skip', '')} "
                  f"[celkem {time.time() - t0:.0f}s]", flush=True)
    print('RUN DONE', flush=True)
    cmd_analyze(a)


def _verdict(e, thr):
    zc = np.array(e['branches']['cand']['z'], dtype=float)
    zh = np.array(e['branches']['champ']['z'], dtype=float)
    d = zc - zh
    m = d.mean()
    sd = d.std(ddof=1) if len(d) > 1 else 0.0
    se = sd / math.sqrt(len(d)) if len(d) else 0.0
    t = m / se if se > 0 else (0.0 if m == 0 else math.inf * np.sign(m))
    if m > thr and (se == 0 or t > 2):
        return 'better', m
    if m < -thr and (se == 0 or t < -2):
        return 'worse', m
    return 'equivalent', m


def cmd_analyze(a):
    res = _load_results()
    evs = [e for e in res['events'].values() if 'skip' not in e]
    skipped = [e for e in res['events'].values() if 'skip' in e]
    # A/A kalibrace prahu
    aa = []
    for e in evs:
        if 'aa_zB' in e:
            zA = np.mean(e['branches']['champ']['z'])
            zB = np.mean(e['aa_zB'])
            aa.append(abs(zB - zA))
    thr = max(0.125, (max(aa) if aa else 0.0))
    rep = {'n_judged': len(evs), 'n_skipped': len(skipped),
           'k': res['meta'].get('k'),
           'aa_abs_dz': {'n': len(aa),
                         'mean': round(float(np.mean(aa)), 3) if aa else None,
                         'max': round(float(max(aa)), 3) if aa else None},
           'threshold': round(thr, 3)}
    for scope, sel in [('pair1_e2', lambda e: e['label'] == 'pair1_e2'),
                       ('pair2_e16', lambda e: e['label'] == 'pair2_e16')]:
        sub = [e for e in evs if sel(e)]
        vc = Counter()
        by_class = defaultdict(Counter)
        dzs = []
        dto = []
        dtvf = []
        for e in sub:
            v, m = _verdict(e, thr)
            vc[v] += 1
            by_class[e['class']][v] += 1
            dzs.append(m)
            dto.append(np.mean(e['branches']['cand']['to'])
                       - np.mean(e['branches']['champ']['to']))
            dtvf.append(np.mean(e['branches']['cand']['tvf'])
                        - np.mean(e['branches']['champ']['tvf']))
        rep[scope] = {
            'n': len(sub), 'verdicts': dict(vc),
            'mean_dz': round(float(np.mean(dzs)), 4) if dzs else None,
            'mean_dz_se': round(float(np.std(dzs, ddof=1) / math.sqrt(len(dzs))), 4)
                if len(dzs) > 1 else None,
            'mean_d_turnovers': round(float(np.mean(dto)), 4) if dto else None,
            'mean_d_terminal_vf': round(float(np.mean(dtvf)), 4) if dtvf else None,
            'by_class': {c: dict(v) for c, v in by_class.items()},
        }
        # detail safer_same_scoring: realna bezpecnost?
        saf = [e for e in sub if e['class'] == 'safer_same_scoring']
        if saf:
            rep[scope]['safer_same_scoring_detail'] = {
                'n': len(saf),
                'mean_dz': round(float(np.mean([_verdict(e, thr)[1] for e in saf])), 4),
                'mean_d_turnovers': round(float(np.mean(
                    [np.mean(e['branches']['cand']['to'])
                     - np.mean(e['branches']['champ']['to']) for e in saf])), 4),
                'mean_d_terminal_vf': round(float(np.mean(
                    [np.mean(e['branches']['cand']['tvf'])
                     - np.mean(e['branches']['champ']['tvf']) for e in saf])), 4),
            }
    out = PROJECT_ROOT / 'diag_oracle_judge_20260730_report.json'
    with open(out, 'w') as f:
        json.dump(rep, f, indent=1)
    print(json.dumps(rep, indent=1, ensure_ascii=False))
    print(f'-> {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    sub = ap.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run')
    r.add_argument('--workers', type=int, default=4)
    r.add_argument('--k', type=int, default=48)
    r.add_argument('--pilot', type=int, default=0)
    sub.add_parser('analyze')
    a = ap.parse_args()
    os.chdir(PROJECT_ROOT)
    if a.cmd == 'run':
        cmd_run(a)
    else:
        cmd_analyze(a)


if __name__ == '__main__':
    main()
