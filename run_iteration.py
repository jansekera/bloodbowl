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
from multiprocessing import Pool
from pathlib import Path

# ─── Configuration ────────────────────────────────────────────────────────────
EPOCHS = 16
GAMES_PER_EPOCH = 40
HOME_RACE = 'human'
AWAY_RACE = 'orc,skaven,dwarf,wood-elf'
MCTS_ITERATIONS = 100
EPSILON_START = 0.35
EPSILON_END = 0.10
BENCHMARK_INTERVAL = 16
BENCHMARK_MATCHES = 400
LR = 0.0003
# VF_BLEND: experiment s 0.3 (commit caa99da) selhal — benchmark klesl na 76.0% a VF inversion
# v epochách 6, 7, 10 (oba avg VF pozitivní = MCTS dostával špatný signál). Vráceno na 0.0.
VF_BLEND = 0.0
VF_RAMP_EPOCHS = 10
GATING_MATCHES = 400
BM_DROP_LIMIT = 0.05
BM_FLOOR = 0.77
ANTI_REGRESSION = 0.51
OPPONENT_MIX_RATIO = 0.5
MODEL = 'neural'
HIDDEN_SIZE = 64
# TV=1200 fields developed (skilled) rosters: goblins removed from Orc, Guard density,
# Strip Ball ball-hunter blitzer, Sure Feet gutter runners. tv<1200 = base rosters.
TV = 1200
WORKERS = min(12, os.cpu_count() or 1)
# ──────────────────────────────────────────────────────────────────────────────

PROJECT_ROOT = Path(__file__).parent.resolve()

_RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']


def _pool_init(engine_build: str, python_src: str) -> None:
    if engine_build not in sys.path:
        sys.path.insert(0, engine_build)
    if python_src not in sys.path:
        sys.path.insert(0, python_src)


def _benchmark_game(args: tuple) -> bool:
    seed, race_idx, gate_path, mcts_iterations, vf_blend, tv = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='random',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, epsilon=0.0, vf_blend=vf_blend,
    ).result
    return result.home_score > result.away_score


def _gate_game(args: tuple) -> tuple[int, int]:
    seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, away_weights_path=frozen_path,
        epsilon=0.0, vf_blend=vf_blend,
    ).result
    return result.home_score, result.away_score


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

    # baseline_reset: set when the TV (roster set) changed since the frozen
    # model was benchmarked — its win rate is on a different game and not comparable.
    baseline_reset = False
    # Step 1: Freeze current best
    if best_path.exists():
        shutil.copy2(str(best_path), str(frozen_path))
        print('Frozen: weights_best.json → weights_frozen.json')
        try:
            meta_path = PROJECT_ROOT / 'weights_best_meta.json'
            if meta_path.exists():
                with open(meta_path) as f:
                    meta = json.load(f)
                frozen_bm = meta.get('benchmark_win_rate', 0.0)
                all_time_best_bm = meta.get('all_time_best_benchmark', frozen_bm)
                meta_tv = meta.get('tv', 1000)
            else:
                with open(best_path) as f:
                    data = json.load(f)
                frozen_bm = data.get('benchmark_win_rate', 0.0) if isinstance(data, dict) else 0.0
                all_time_best_bm = frozen_bm
                meta_tv = 1000
            if meta_tv != TV:
                print(f'⚠ Změna TV {meta_tv}→{TV}: gating baseline RESET — předchozí '
                      f'benchmark {frozen_bm:.1%} byl na jiných rosterech a neplatí. '
                      f'První TV{TV} model se promotne jako nový baseline.', flush=True)
                frozen_bm = 0.0
                all_time_best_bm = 0.0
                baseline_reset = True
            print(f'Frozen benchmark: {frozen_bm:.1%}')
        except Exception:
            frozen_bm = 0.0
            all_time_best_bm = 0.0
    else:
        with open(best_path, 'w') as f:
            json.dump({'type': 'alphazero_neural', 'value_weights': [0.0] * 70}, f)
        shutil.copy2(str(best_path), str(frozen_path))
        frozen_bm = 0.0
        all_time_best_bm = 0.0
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
        f'--tv={TV}',
        '--training-method=mc_shaped',
        f'--epsilon-start={EPSILON_START}', f'--epsilon-end={EPSILON_END}',
        f'--benchmark-interval={BENCHMARK_INTERVAL}', f'--benchmark-matches={BENCHMARK_MATCHES}',
        '--skip-greedy-benchmark', '--timeout=300',
        f'--opponent-mix-ratio={OPPONENT_MIX_RATIO}',
        f'--workers={WORKERS}',
    ]
    subprocess.run(cmd, env=env, cwd=str(PROJECT_ROOT), check=True)

    # Step 3: Benchmark az_train (final epoch) AND train_best (best self-play epoch),
    # then gate on whichever scores higher vs random. Each gets BENCHMARK_MATCHES/2 games.
    print('\n=== Gating ===', flush=True)
    init_args = (str(engine_build), str(python_src))
    half_bm = BENCHMARK_MATCHES // 2

    def _run_benchmark(path: Path, label: str) -> float:
        tasks = [
            (random.randint(1, 999999), i, str(path), MCTS_ITERATIONS, VF_BLEND, TV)
            for i in range(half_bm)
        ]
        print(f'Benchmark {label}: {half_bm} games ({WORKERS} workers)...', flush=True)
        results: list[bool] = []
        with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
            for result in pool.imap_unordered(_benchmark_game, tasks):
                results.append(result)
                done = len(results)
                if done % 20 == 0 or done == half_bm:
                    print(f'  {label} {done}/{half_bm}: {sum(results)/done:.1%}', flush=True)
        score = sum(results) / half_bm
        print(f'  {label} final: {score:.1%} ({sum(results)}/{half_bm})', flush=True)
        return score

    bm_az = _run_benchmark(az_train_path, 'az_train')
    bm_tb = _run_benchmark(train_best_path, 'train_best') if train_best_path.exists() else 0.0
    if not train_best_path.exists():
        print('train_best not found, using az_train', flush=True)

    if bm_tb > bm_az:
        gate_path = train_best_path
        new_bm = bm_tb
        gate_label = f'train_best ({new_bm:.1%}) > az_train ({bm_az:.1%})'
    else:
        gate_path = az_train_path
        new_bm = bm_az
        gate_label = f'az_train ({new_bm:.1%}) >= train_best ({bm_tb:.1%})'

    print(f'Gating on: {gate_label}', flush=True)
    print(f'Benchmark: new={new_bm:.1%}  best={frozen_bm:.1%}  all_time_best={all_time_best_bm:.1%}  (max pokles {BM_DROP_LIMIT:.0%})', flush=True)

    # Step 4: Anti-regression gating games (winner vs frozen)
    gate_tasks = [
        (random.randint(1, 999999), i, str(gate_path), str(frozen_path), MCTS_ITERATIONS, VF_BLEND, TV)
        for i in range(GATING_MATCHES)
    ]
    print(f'Anti-regression: {GATING_MATCHES} games ({WORKERS} workers)...', flush=True)
    gate_results: list[tuple[int, int]] = []
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for hs, as_ in pool.imap_unordered(_gate_game, gate_tasks):
            gate_results.append((hs, as_))
            done = len(gate_results)
            if done % 10 == 0 or done == GATING_MATCHES:
                print(f'  Anti-regression {done}/{GATING_MATCHES}', flush=True)

    wins = draws = losses = 0
    for i, (hs, as_) in enumerate(gate_results):
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

    if all_time_best_bm > 0 and new_bm < all_time_best_bm - BM_DROP_LIMIT:
        promote = False
        reasons.append(f'benchmark klesl {all_time_best_bm:.1%}→{new_bm:.1%} (>{BM_DROP_LIMIT:.0%})')

    if frozen_bm > 0 and new_bm < frozen_bm - 0.02:
        promote = False
        reasons.append(f'benchmark pod frozen ({new_bm:.1%} < {frozen_bm:.1%} - 2%)')

    if new_bm < BM_FLOOR and not baseline_reset:
        promote = False
        reasons.append(f'benchmark pod minimem ({new_bm:.1%} < {BM_FLOOR:.0%})')

    if chess_score < ANTI_REGRESSION and not baseline_reset:
        promote = False
        reasons.append(f'horší než frozen ({chess_score:.1%} < {ANTI_REGRESSION:.0%})')

    if baseline_reset:
        promote = True  # force-establish the new TV baseline regardless of absolute score
        print(f'Baseline reset: TV{TV} model promoted as new baseline '
              f'(benchmark={new_bm:.1%}, chess vs old={chess_score:.1%})', flush=True)

    label = 'promoted' if promote else 'rejected'

    if promote:
        shutil.copy2(str(gate_path), str(best_path))
        new_all_time = max(all_time_best_bm, new_bm)
        with open(PROJECT_ROOT / 'weights_best_meta.json', 'w') as f:
            json.dump({'benchmark_win_rate': new_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS,
                       'all_time_best_benchmark': new_all_time, 'tv': TV}, f)
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
            'weights_train_best.json', 'epoch_metrics.csv',
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
