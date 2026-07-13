"""Evaluate a trained learning AI agent.

Usage: python -m blood_bowl.evaluate --weights=weights.json --opponent=random --matches=100
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Callable, Iterable, Optional

from .cli_runner import CLIRunner


def pre_td_value_ramp(game_logs: Iterable[list], value_fn: Callable[[list], float],
                      window: int = 3) -> Optional[float]:
    """Pre-TD value ramp: mean V over states 1..window own-turns before an own
    TD, minus mean V over all other states.

    Primary Stage-1 judgment metric for the mc_td_mix value target
    (research_fable_20260709.md section 7): a value head with within-game
    signal must rise as its side approaches scoring; a head trained on a pure
    terminal broadcast cannot, whatever its cross-game calibration. One logged
    state == one turn of its perspective (engine pushes states/turn_logs 1:1),
    so "turns before" == per-perspective state indices. A TD is registered at
    the first state whose own running score increased. Returns None when the
    metric is undefined (no own TD anywhere, or no non-window states, or logs
    predate per-state scores).
    """
    pre_vals: list[float] = []
    other_vals: list[float] = []
    for game_log in game_logs:
        groups: dict = {}
        for record in game_log:
            if record.get('type') != 'state' or 'features' not in record:
                continue
            groups.setdefault(record.get('perspective', 'home'), []).append(record)
        for perspective, states in groups.items():
            if not all('home_score' in s for s in states):
                continue  # old logs without running scores: TD timing unknown
            my_scores = [s['away_score'] if perspective == 'away'
                         else s['home_score'] for s in states]
            pre_idx: set = set()
            for j in range(1, len(states)):
                if my_scores[j] > my_scores[j - 1]:
                    pre_idx.update(range(max(0, j - window), j))
            for i, s in enumerate(states):
                (pre_vals if i in pre_idx else other_vals).append(value_fn(s['features']))
    if not pre_vals or not other_vals:
        return None
    return sum(pre_vals) / len(pre_vals) - sum(other_vals) / len(other_vals)


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
