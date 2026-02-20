"""Auto-benchmark: evaluate trained weights against various opponents.

Usage: python -m blood_bowl.benchmark --weights=weights.json --matches=50
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Dict, List, Optional

from .cli_runner import CLIRunner


def _make_runner(project_root: Optional[str], use_cpp: Optional[bool] = None):
    """Create a runner, preferring C++ engine if available."""
    if use_cpp is not False:
        try:
            from .cpp_runner import CPPRunner
            return CPPRunner(project_root)
        except ImportError:
            pass
    return CLIRunner(project_root)


def run_benchmark(
    weights_file: str,
    opponents: Optional[List[str]] = None,
    matches_per_opponent: int = 50,
    project_root: Optional[str] = None,
    timeout: int = 120,
    skip_greedy: bool = False,
    tv: Optional[int] = None,
    use_cpp: Optional[bool] = None,
    mcts_iterations: int = 0,
    policy_weights: Optional[str] = None,
) -> Dict[str, dict]:
    """Run benchmark matches against each opponent, return results.

    Returns dict mapping opponent name -> {win_rate, avg_score_diff, matches}.
    """
    if opponents is None:
        opponents = ['random', 'greedy']

    if skip_greedy:
        opponents = [o for o in opponents if o != 'greedy']

    runner = _make_runner(project_root, use_cpp)
    results = {}

    home_ai_type = 'macro_mcts' if mcts_iterations > 0 else 'learning'

    for opponent in opponents:
        try:
            result = runner.simulate(
                home_ai=home_ai_type,
                away_ai=opponent,
                matches=matches_per_opponent,
                timeout=timeout,
                timeout_per_game=True,
                weights=weights_file,
                epsilon=0.0,  # Greedy evaluation
                tv=tv,
                mcts_iterations=mcts_iterations,
                policy_weights=policy_weights if mcts_iterations > 0 else None,
            )

            avg_score_diff = sum(
                r.home_score - r.away_score for r in result.results
            ) / max(len(result.results), 1)

            results[opponent] = {
                'win_rate': result.home_win_rate,
                'avg_score_diff': avg_score_diff,
                'matches': len(result.results),
            }
        except Exception as e:
            print(f'  Benchmark vs {opponent} failed: {e}', file=sys.stderr)
            results[opponent] = {
                'win_rate': 0.0,
                'avg_score_diff': 0.0,
                'matches': 0,
            }

    return results


def main():
    parser = argparse.ArgumentParser(description='Benchmark Blood Bowl AI weights')
    parser.add_argument('--weights', required=True, help='Path to weights file')
    parser.add_argument('--opponents', nargs='+', default=['random', 'greedy'],
                        help='Opponents to benchmark against')
    parser.add_argument('--matches', type=int, default=50,
                        help='Matches per opponent')
    parser.add_argument('--project-root', default=None, help='Project root directory')
    parser.add_argument('--timeout', type=int, default=120,
                        help='Timeout per game (seconds)')
    parser.add_argument('--skip-greedy', action='store_true',
                        help='Skip greedy opponent in benchmark')
    parser.add_argument('--tv', type=int, default=None,
                        help='Team value level (e.g. 1500 for developed rosters)')
    args = parser.parse_args()

    project_root = args.project_root
    if project_root is None:
        project_root = str(Path(__file__).parent.parent.parent)

    weights_path = args.weights
    if not Path(weights_path).is_absolute():
        weights_path = str(Path(project_root) / weights_path)

    print(f'Benchmarking {weights_path}')
    print(f'Opponents: {", ".join(args.opponents)}')
    print(f'Matches per opponent: {args.matches}')
    print()

    results = run_benchmark(
        weights_file=weights_path,
        opponents=args.opponents,
        matches_per_opponent=args.matches,
        project_root=project_root,
        timeout=args.timeout,
        skip_greedy=args.skip_greedy,
        tv=args.tv,
    )

    for opponent, stats in results.items():
        print(f'vs {opponent:>8}: win_rate={stats["win_rate"]:.1%}, '
              f'avg_score_diff={stats["avg_score_diff"]:+.2f}, '
              f'matches={stats["matches"]}')


if __name__ == '__main__':
    main()
