"""Evaluate a trained learning AI agent.

Usage: python -m blood_bowl.evaluate --weights=weights.json --opponent=random --matches=100
"""
from __future__ import annotations

import argparse
from pathlib import Path

from .cli_runner import CLIRunner


def evaluate_agent(
    weights_file: str,
    opponent: str = 'random',
    matches: int = 100,
    project_root: str | None = None,
    timeout: int = 600,
) -> dict:
    """Run evaluation matches (epsilon=0) and return stats."""
    runner = CLIRunner(project_root)

    # Resolve weights path relative to project root (PHP cwd)
    root = Path(runner.project_root)
    weights_path = root / weights_file if not Path(weights_file).is_absolute() else Path(weights_file)

    result = runner.simulate(
        home_ai='learning',
        away_ai=opponent,
        matches=matches,
        weights=str(weights_path),
        epsilon=0.0,
        timeout=timeout,
    )

    total_home_score = sum(r.home_score for r in result.results)
    total_away_score = sum(r.away_score for r in result.results)

    return {
        'win_rate': result.home_win_rate,
        'draw_rate': result.draws / max(result.matches, 1),
        'loss_rate': result.away_win_rate,
        'avg_score': total_home_score / max(matches, 1),
        'avg_opp_score': total_away_score / max(matches, 1),
        'matches': matches,
        'wins': result.home_wins,
        'draws': result.draws,
        'losses': result.away_wins,
    }


def main():
    parser = argparse.ArgumentParser(description='Evaluate trained Blood Bowl AI')
    parser.add_argument('--weights', default='weights.json', help='Weights file')
    parser.add_argument('--opponent', default='random', help='Opponent type')
    parser.add_argument('--matches', type=int, default=100, help='Number of matches')
    parser.add_argument('--project-root', default=None, help='Project root')
    parser.add_argument('--timeout', type=int, default=600, help='Timeout (seconds)')
    args = parser.parse_args()

    project_root = args.project_root
    if project_root is None:
        project_root = str(Path(__file__).parent.parent.parent)

    stats = evaluate_agent(
        weights_file=args.weights,
        opponent=args.opponent,
        matches=args.matches,
        project_root=project_root,
        timeout=args.timeout,
    )

    print(f"\nEvaluation Results ({args.matches} matches vs {args.opponent}):")
    print(f"  Win rate:  {stats['win_rate']:.1%}")
    print(f"  Draw rate: {stats['draw_rate']:.1%}")
    print(f"  Loss rate: {stats['loss_rate']:.1%}")
    print(f"  Avg score: {stats['avg_score']:.2f} - {stats['avg_opp_score']:.2f}")
    print(f"  Record:    {stats['wins']}W / {stats['draws']}D / {stats['losses']}L")


if __name__ == '__main__':
    main()
