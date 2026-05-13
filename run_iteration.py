#!/usr/bin/env python3
"""AlphaZero iteration: freeze → self-play → gate → promote.

Equivalent to Colab Cell 3, runs locally or on server without Colab.

Usage:
    python3 run_iteration.py              # 1 iteration
    python3 run_iteration.py --loop 5    # 5 iterations
    python3 run_iteration.py --no-push   # skip git push
"""
from __future__ import annotations

import argparse
import json
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

# ─── Configuration ────────────────────────────────────────────────────────────
EPOCHS = 10
GAMES_PER_EPOCH = 40
HOME_RACE = 'human'
AWAY_RACE = 'orc,skaven,dwarf,wood-elf'
MCTS_ITERATIONS = 100
EPSILON_START = 0.35
EPSILON_END = 0.10
BENCHMARK_INTERVAL = 10
BENCHMARK_MATCHES = 100
LR = 0.0001
VF_BLEND = 0.0
VF_RAMP_EPOCHS = 10
GATING_MATCHES = 50
BM_DROP_LIMIT = 0.10
BM_FLOOR = 0.80
ANTI_REGRESSION = 0.35
OPPONENT_MIX_RATIO = 0.3
MODEL = 'neural'
HIDDEN_SIZE = 64
# ──────────────────────────────────────────────────────────────────────────────

PROJECT_ROOT = Path(__file__).parent.resolve()


def run_iteration(no_push: bool = False) -> tuple[bool, float | None, float]:
    os.chdir(str(PROJECT_ROOT))

    # Ensure bb_engine is importable
    engine_build = PROJECT_ROOT / 'engine' / 'build'
    python_src = PROJECT_ROOT / 'python'
    for p in (str(engine_build), str(python_src)):
        if p not in sys.path:
            sys.path.insert(0, p)

    try:
        import bb_engine  # noqa: F401
    except ImportError:
        print('ERROR: bb_engine not found. Run ./setup.sh first to build the C++ engine.')
        sys.exit(1)
    import bb_engine

    best_path = PROJECT_ROOT / 'weights_best.json'
    frozen_path = PROJECT_ROOT / 'weights_frozen.json'
    az_train_path = PROJECT_ROOT / 'weights_az_train.json'
    train_best_path = PROJECT_ROOT / 'weights_train_best.json'

    # Step 1: Freeze current best
    if best_path.exists():
        shutil.copy2(str(best_path), str(frozen_path))
        print('Frozen: weights_best.json → weights_frozen.json')
        try:
            meta_path = PROJECT_ROOT / 'weights_best_meta.json'
            if meta_path.exists():
                with open(meta_path) as f:
                    frozen_bm = json.load(f).get('benchmark_win_rate', 0.0)
            else:
                with open(best_path) as f:
                    data = json.load(f)
                frozen_bm = data.get('benchmark_win_rate', 0.0) if isinstance(data, dict) else 0.0
            print(f'Frozen benchmark: {frozen_bm:.1%}')
        except Exception:
            frozen_bm = 0.0
    else:
        with open(best_path, 'w') as f:
            json.dump({'type': 'alphazero_neural', 'value_weights': [0.0] * 70}, f)
        shutil.copy2(str(best_path), str(frozen_path))
        frozen_bm = 0.0
        print('First run: created fresh weights')

    shutil.copy2(str(best_path), str(az_train_path))

    # Step 2: Self-play training
    print(f'\n=== Self-play training ({EPOCHS} epochs x {GAMES_PER_EPOCH} games) ===')
    env = os.environ.copy()
    env['PYTHONPATH'] = f'{engine_build}:{python_src}'

    cmd = [
        sys.executable, '-m', 'blood_bowl.train_cli',
        f'--epochs={EPOCHS}',
        f'--games={GAMES_PER_EPOCH}',
        '--use-cpp',
        '--opponent=learning', '--self-play',
        f'--home-race={HOME_RACE}', f'--away-race={AWAY_RACE}',
        f'--mcts-iterations={MCTS_ITERATIONS}',
        f'--lr={LR}', f'--model={MODEL}', f'--hidden-size={HIDDEN_SIZE}',
        f'--vf-blend={VF_BLEND}', f'--vf-ramp-epochs={VF_RAMP_EPOCHS}',
        '--policy-lr=0',
        '--weights=weights_az_train.json',
        '--training-method=mc_shaped',
        f'--epsilon-start={EPSILON_START}', f'--epsilon-end={EPSILON_END}',
        f'--benchmark-interval={BENCHMARK_INTERVAL}', f'--benchmark-matches={BENCHMARK_MATCHES}',
        '--skip-greedy-benchmark', '--timeout=300',
        f'--opponent-mix-ratio={OPPONENT_MIX_RATIO}',
    ]
    subprocess.run(cmd, env=env, cwd=str(PROJECT_ROOT), check=True)

    # Step 3: Pre-select better candidate (train_best vs final epoch), then benchmark vs random
    print('\n=== Gating ===', flush=True)
    races = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']

    PRE_SELECT = 20
    gate_path = az_train_path
    gate_label = 'az_train'
    best_pre = -1
    for label, path in [('train_best', train_best_path), ('az_train', az_train_path)]:
        if not path.exists():
            continue
        wins = 0
        for i in range(PRE_SELECT):
            seed = random.randint(1, 999999)
            r = bb_engine.simulate_game_logged(
                bb_engine.get_roster(races[i % len(races)]),
                bb_engine.get_roster(races[(i + 1) % len(races)]),
                home_ai='macro_mcts', away_ai='random',
                seed=seed, mcts_iterations=MCTS_ITERATIONS,
                weights_path=str(path), epsilon=0.0, vf_blend=VF_BLEND,
            ).result
            if r.home_score > r.away_score:
                wins += 1
        print(f'  Pre-select {label}: {wins}/{PRE_SELECT} = {wins/PRE_SELECT:.1%}', flush=True)
        if wins > best_pre:
            best_pre = wins
            gate_path = path
            gate_label = label
    print(f'Selected for gating: {gate_label}', flush=True)

    bm_wins = 0
    for i in range(BENCHMARK_MATCHES):
        seed = random.randint(1, 999999)
        hr = bb_engine.get_roster(races[i % len(races)])
        ar = bb_engine.get_roster(races[(i + 1) % len(races)])
        result = bb_engine.simulate_game_logged(
            hr, ar,
            home_ai='macro_mcts', away_ai='random',
            seed=seed, mcts_iterations=MCTS_ITERATIONS,
            weights_path=str(gate_path),
            epsilon=0.0, vf_blend=VF_BLEND,
        ).result
        if result.home_score > result.away_score:
            bm_wins += 1

    new_bm: float = bm_wins / BENCHMARK_MATCHES
    print(f'Benchmark (train_best vs random): {new_bm:.1%} ({bm_wins}/{BENCHMARK_MATCHES})', flush=True)
    print(f'Benchmark: new={new_bm:.1%}  best={frozen_bm:.1%}  (max pokles {BM_DROP_LIMIT:.0%})', flush=True)

    # Step 4: Anti-regression gating games (train_best vs frozen)
    wins = draws = losses = 0
    for i in range(GATING_MATCHES):
        seed = random.randint(1, 999999)
        hr = bb_engine.get_roster(races[i % len(races)])
        ar = bb_engine.get_roster(races[(i + 1) % len(races)])
        result = bb_engine.simulate_game_logged(
            hr, ar,
            home_ai='macro_mcts', away_ai='macro_mcts',
            seed=seed, mcts_iterations=MCTS_ITERATIONS,
            weights_path=str(gate_path),
            away_weights_path=str(frozen_path),
            epsilon=0.0, vf_blend=VF_BLEND,
        ).result
        hs, as_ = result.home_score, result.away_score
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        print(f'  Game {i + 1}: {hs}-{as_}', flush=True)

    total = wins + draws + losses
    chess_score = (wins + 0.5 * draws) / total
    print(f'New vs Frozen: {wins}W {draws}D {losses}L = {chess_score:.1%}', flush=True)

    # Step 5: Gate decision
    promote = True
    reasons: list[str] = []

    if frozen_bm > 0 and new_bm < frozen_bm - BM_DROP_LIMIT:
        promote = False
        reasons.append(f'benchmark klesl {frozen_bm:.1%}→{new_bm:.1%} (>{BM_DROP_LIMIT:.0%})')

    if new_bm < BM_FLOOR:
        promote = False
        reasons.append(f'benchmark pod minimem ({new_bm:.1%} < {BM_FLOOR:.0%})')

    if chess_score < ANTI_REGRESSION:
        promote = False
        reasons.append(f'horší než frozen ({chess_score:.1%} < {ANTI_REGRESSION:.0%})')

    label = 'promoted' if promote else 'rejected'

    if promote:
        shutil.copy2(str(gate_path), str(best_path))
        with open(PROJECT_ROOT / 'weights_best_meta.json', 'w') as f:
            json.dump({'benchmark_win_rate': new_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS}, f)
        print(f'PROMOTED (benchmark={new_bm:.1%}, chess={chess_score:.1%}) → weights_best.json updated', flush=True)
    else:
        shutil.copy2(str(frozen_path), str(best_path))
        print(f'REJECTED: {"; ".join(reasons)}', flush=True)

    # Step 6: Git push
    if no_push:
        print('(git push přeskočen — --no-push)')
    else:
        _git_push(PROJECT_ROOT, promote, frozen_path, gate_path,
                  frozen_bm, new_bm, chess_score, label)

    return promote, new_bm, chess_score


def _git_push(root: Path, promote: bool, frozen_path: Path, gate_path: Path,
              frozen_bm: float, new_bm: float, chess_score: float, label: str) -> None:
    try:
        # Read weights into memory BEFORE git reset (reset overwrites working tree)
        with open(gate_path, 'rb') as f:
            gate_data = f.read()
        with open(frozen_path, 'rb') as f:
            frozen_data = f.read()

        # Pull latest to avoid conflict, then add our files
        subprocess.run(['git', 'fetch', 'origin'], cwd=str(root), capture_output=True)
        subprocess.run(['git', 'reset', '--hard', 'origin/main'], cwd=str(root), capture_output=True)

        # Re-apply weights after reset (reset overwrites working tree)
        best_path = root / 'weights_best.json'
        if not promote:
            best_path.write_bytes(frozen_data)
            meta = {'benchmark_win_rate': frozen_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS}
        else:
            best_path.write_bytes(gate_data)
            meta = {'benchmark_win_rate': new_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS}

        with open(root / 'weights_best_meta.json', 'w') as f:
            json.dump(meta, f)

        files = [
            'weights_best.json', 'weights_best_meta.json',
            'weights_frozen.json', 'weights_az_train.json', 'weights_az_train_meta.json',
            'weights_train_best.json',
        ]
        snaps = [f for f in os.listdir(str(root)) if f.startswith('weights_snap_')]
        subprocess.run(['git', 'add', '-f'] + files + snaps, cwd=str(root), capture_output=True)

        bm_str = f'{new_bm:.1%}'
        subprocess.run(
            ['git', 'commit', '-m', f'AlphaZero: bm={bm_str} chess={chess_score:.1%} ({label})'],
            cwd=str(root), capture_output=True,
        )
        r = subprocess.run(['git', 'push'], cwd=str(root), capture_output=True)
        if r.returncode == 0:
            print('Pushed!')
        else:
            print(f'Push failed: {r.stderr.decode()[:300]}')
            print('(Weights jsou uloženy lokálně)')
    except Exception as e:
        print(f'Git push failed: {e} — weights uloženy lokálně')


def main() -> None:
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description='AlphaZero iteration runner')
    parser.add_argument('--loop', type=int, default=1, metavar='N',
                        help='Počet iterací za sebou (default: 1)')
    parser.add_argument('--no-push', action='store_true',
                        help='Přeskoč git push po každé iteraci')
    args = parser.parse_args()

    for i in range(args.loop):
        if args.loop > 1:
            print(f'\n{"=" * 60}')
            print(f'ITERACE {i + 1}/{args.loop}')
            print(f'{"=" * 60}')
        run_iteration(no_push=args.no_push)

    if args.loop > 1:
        print(f'\nHotovo — {args.loop} iterací dokončeno.')


if __name__ == '__main__':
    main()
