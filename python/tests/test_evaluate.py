"""Tests for evaluation and visualization."""
import csv
import tempfile
from pathlib import Path
from unittest.mock import patch, MagicMock

import pytest

from blood_bowl.evaluate import evaluate_agent
from blood_bowl.cli_runner import TournamentResult, MatchResult


class TestEvaluateAgent:
    def test_returns_dict_with_correct_keys(self):
        """evaluate_agent should return a dict with expected keys."""
        mock_result = TournamentResult(
            home_ai='learning', away_ai='random',
            matches=5, home_wins=3, away_wins=1, draws=1,
            results=[
                MatchResult(home_score=2, away_score=1, total_actions=100, phase='game_over', half=2),
                MatchResult(home_score=1, away_score=0, total_actions=120, phase='game_over', half=2),
                MatchResult(home_score=0, away_score=2, total_actions=110, phase='game_over', half=2),
                MatchResult(home_score=1, away_score=1, total_actions=90, phase='game_over', half=2),
                MatchResult(home_score=3, away_score=0, total_actions=130, phase='game_over', half=2),
            ],
        )

        with patch('blood_bowl.evaluate.CLIRunner') as MockRunner:
            instance = MockRunner.return_value
            instance.simulate.return_value = mock_result

            stats = evaluate_agent(
                weights_file='test_weights.json',
                opponent='random',
                matches=5,
            )

        expected_keys = {'win_rate', 'draw_rate', 'loss_rate', 'avg_score', 'avg_opp_score',
                         'matches', 'wins', 'draws', 'losses'}
        assert set(stats.keys()) == expected_keys
        assert stats['win_rate'] == 3 / 5
        assert stats['matches'] == 5


class TestPlotTraining:
    def test_creates_png_file(self):
        """plot_training should create a PNG file."""
        try:
            import matplotlib
        except ImportError:
            pytest.skip('matplotlib not installed')

        from blood_bowl.visualize import plot_training

        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'results.csv'
            png_path = Path(tmpdir) / 'curve.png'

            with open(csv_path, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(['epoch', 'win_rate', 'avg_score_diff', 'epsilon'])
                for i in range(1, 6):
                    writer.writerow([i, f'{0.3 + i * 0.05:.3f}', f'{-0.5 + i * 0.2:.2f}', f'{0.3 - i * 0.05:.3f}'])

            plot_training(str(csv_path), str(png_path))
            assert png_path.exists()
            assert png_path.stat().st_size > 0
