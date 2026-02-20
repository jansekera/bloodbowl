"""Tests for benchmark module."""
from __future__ import annotations

import csv
import tempfile
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from blood_bowl.benchmark import run_benchmark
from blood_bowl.cli_runner import MatchResult, TournamentResult


def _mock_tournament_result(home_wins: int, away_wins: int, draws: int) -> TournamentResult:
    """Create a mock TournamentResult."""
    total = home_wins + away_wins + draws
    results = []
    for _ in range(home_wins):
        results.append(MatchResult(home_score=2, away_score=1, total_actions=100, phase='game', half=2))
    for _ in range(away_wins):
        results.append(MatchResult(home_score=0, away_score=1, total_actions=100, phase='game', half=2))
    for _ in range(draws):
        results.append(MatchResult(home_score=1, away_score=1, total_actions=100, phase='game', half=2))
    return TournamentResult(
        home_ai='learning',
        away_ai='random',
        matches=total,
        home_wins=home_wins,
        away_wins=away_wins,
        draws=draws,
        results=results,
    )


class TestRunBenchmark:
    @patch('blood_bowl.benchmark.CLIRunner')
    def test_returns_results_for_each_opponent(self, mock_runner_cls):
        mock_runner = MagicMock()
        mock_runner_cls.return_value = mock_runner
        mock_runner.simulate.return_value = _mock_tournament_result(3, 1, 1)

        results = run_benchmark(
            weights_file='weights.json',
            opponents=['random', 'greedy'],
            matches_per_opponent=5,
            use_cpp=False,
        )

        assert 'random' in results
        assert 'greedy' in results
        assert results['random']['matches'] == 5
        assert results['random']['win_rate'] == 0.6

    @patch('blood_bowl.benchmark.CLIRunner')
    def test_win_rate_calculation(self, mock_runner_cls):
        mock_runner = MagicMock()
        mock_runner_cls.return_value = mock_runner
        mock_runner.simulate.return_value = _mock_tournament_result(5, 0, 0)

        results = run_benchmark(
            weights_file='w.json',
            opponents=['random'],
            matches_per_opponent=5,
            use_cpp=False,
        )

        assert results['random']['win_rate'] == 1.0
        assert results['random']['avg_score_diff'] == 1.0

    @patch('blood_bowl.benchmark.CLIRunner')
    def test_score_diff_calculation(self, mock_runner_cls):
        mock_runner = MagicMock()
        mock_runner_cls.return_value = mock_runner
        # 3 wins (2-1) + 2 losses (0-1) = avg diff = (3*1 + 2*(-1))/5 = 0.2
        mock_runner.simulate.return_value = _mock_tournament_result(3, 2, 0)

        results = run_benchmark(
            weights_file='w.json',
            opponents=['random'],
            matches_per_opponent=5,
            use_cpp=False,
        )

        assert abs(results['random']['avg_score_diff'] - 0.2) < 0.01

    @patch('blood_bowl.benchmark.CLIRunner')
    def test_handles_simulate_failure(self, mock_runner_cls):
        mock_runner = MagicMock()
        mock_runner_cls.return_value = mock_runner
        mock_runner.simulate.side_effect = RuntimeError('timeout')

        results = run_benchmark(
            weights_file='w.json',
            opponents=['random'],
            matches_per_opponent=5,
            use_cpp=False,
        )

        assert results['random']['win_rate'] == 0.0
        assert results['random']['matches'] == 0

    @patch('blood_bowl.benchmark.CLIRunner')
    def test_uses_epsilon_zero(self, mock_runner_cls):
        """Benchmark should use epsilon=0 (greedy evaluation)."""
        mock_runner = MagicMock()
        mock_runner_cls.return_value = mock_runner
        mock_runner.simulate.return_value = _mock_tournament_result(1, 0, 0)

        run_benchmark(
            weights_file='w.json',
            opponents=['random'],
            matches_per_opponent=1,
            use_cpp=False,
        )

        call_kwargs = mock_runner.simulate.call_args[1]
        assert call_kwargs['epsilon'] == 0.0
