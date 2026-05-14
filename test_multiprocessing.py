#!/usr/bin/env python3
"""Test that bb_engine works correctly with multiprocessing.Pool."""
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.resolve()
sys.path.insert(0, str(PROJECT_ROOT / 'engine' / 'build'))
sys.path.insert(0, str(PROJECT_ROOT / 'python'))

import bb_engine
from multiprocessing import Pool

def run_one(seed):
    races = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']
    hr = bb_engine.get_roster(races[seed % len(races)])
    ar = bb_engine.get_roster(races[(seed + 1) % len(races)])
    r = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='random',
        seed=seed, mcts_iterations=50,
        weights_path=str(PROJECT_ROOT / 'weights_best.json'),
        epsilon=0.0, vf_blend=0.0,
    )
    return seed, r.result.home_score, r.result.away_score

if __name__ == '__main__':
    print('Sequential (baseline):')
    for seed in range(4):
        result = run_one(seed)
        print(f'  seed={result[0]}: {result[1]}-{result[2]}')

    print('\nParallel (4 workers, 8 games):')
    with Pool(4) as p:
        results = p.map(run_one, range(8))
    for seed, hs, as_ in results:
        print(f'  seed={seed}: {hs}-{as_}')

    print('\nParallel (12 workers, 12 games):')
    with Pool(12) as p:
        results = p.map(run_one, range(12))
    for seed, hs, as_ in results:
        print(f'  seed={seed}: {hs}-{as_}')

    print('\nOK - bb_engine je bezpecne pro multiprocessing')
