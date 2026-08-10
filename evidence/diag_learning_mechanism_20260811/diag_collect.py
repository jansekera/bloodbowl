"""Diag pro paku 1: sber rozhodnuti (visit distribuce + board snapshoty).

Run A: dwarf vs skaven, policy OFF (blend 0)  - 16 her
Run B: dwarf vs skaven, policy ON  (blend 0.2, weights_best_policy) - 16 her
Run C: wood-elf vs skaven, policy OFF - 8 her (kontrast pro shodu policy s elfim searchem)
Vsechno nice -19, Pool(2). Jen cteni produkce, zapis do scratchpadu.
"""
import os, sys, pickle
import numpy as np
from multiprocessing import Pool

sys.path.insert(0, '/home/jan/claude/bloodbowl/engine/build')
os.chdir('/home/jan/claude/bloodbowl')

SCR = '/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad'
SEED0 = 91_000_000

def compact_players(pl):
    return [(p['id'], p['x'], p['y'], p['state'], p['has_ball'], p['name'],
             p['ma'], p['st'], p['ag'], p['av']) for p in pl]

def game(args):
    import bb_engine
    tag, ra, rb, seed, blend = args
    hr = bb_engine.get_developed_roster(ra, 1200)
    ar = bb_engine.get_developed_roster(rb, 1200)
    kw = dict(home_ai='macro_mcts', away_ai='macro_mcts', seed=seed,
              mcts_iterations=100, weights_path='weights_best.json',
              away_weights_path='weights_best.json', epsilon=0.0, vf_blend=0.15)
    if blend > 0:
        kw.update(policy_weights_path='weights_best_policy.json', policy_blend=blend,
                  away_policy_weights_path='weights_best_policy.json',
                  away_policy_blend=blend)
    lgr = bb_engine.simulate_game_logged(hr, ar, **kw)
    decs = []
    for d in lgr.get_policy_decisions():
        decs.append(dict(
            sf=np.asarray(d['state_features'], dtype=np.float32),
            persp=d['perspective'],
            av=[(np.asarray(v['action_features'], dtype=np.float32),
                 float(v['visit_fraction'])) for v in d['visits']],
            home=compact_players(d['home_players']),
            away=compact_players(d['away_players']),
            ball=(d['ball_x'], d['ball_y'], d['ball_held'], d['ball_carrier_id'])))
    return dict(tag=tag, ra=ra, rb=rb, seed=seed, blend=blend,
                score=(lgr.result.home_score, lgr.result.away_score), decs=decs)

def main():
    jobs = []
    for i in range(16):
        jobs.append(('A', 'dwarf', 'skaven', SEED0 + i, 0.0))
    for i in range(16):
        jobs.append(('B', 'dwarf', 'skaven', SEED0 + i, 0.2))  # stejne seedy jako A
    for i in range(8):
        jobs.append(('C', 'wood-elf', 'skaven', SEED0 + 100 + i, 0.0))
    with Pool(2) as pool:
        results = []
        for k, r in enumerate(pool.imap_unordered(game, jobs)):
            results.append(r)
            print(f"done {k+1}/{len(jobs)} tag={r['tag']} score={r['score']}", flush=True)
    with open(os.path.join(SCR, 'decisions.pkl'), 'wb') as f:
        pickle.dump(results, f, protocol=4)
    print('saved')

if __name__ == '__main__':
    main()
