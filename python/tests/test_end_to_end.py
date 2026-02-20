"""End-to-end tests for the learning AI training pipeline.

NOTE: These tests run actual PHP simulations which take ~2-3 minutes per game.
Use `pytest -k "not end_to_end"` to skip them in fast test runs.
"""
from __future__ import annotations

import csv
import json
import tempfile
from pathlib import Path

import pytest

from blood_bowl.cli_runner import CLIRunner
from blood_bowl.features import NUM_FEATURES
from blood_bowl.trainer import LinearTrainer
from blood_bowl.training_loop import _read_jsonl


def get_project_root():
    return str(Path(__file__).parent.parent.parent)


class TestEndToEnd:
    """Integration tests that run real PHP simulations.

    Each match takes ~2-3 minutes, so timeouts are generous.
    """

    def test_learning_vs_random_produces_jsonl_logs(self):
        """Run 1 game learning vs random and verify JSONL logs are produced."""
        with tempfile.TemporaryDirectory() as tmpdir:
            log_dir = Path(tmpdir) / 'logs'
            weights_path = Path(tmpdir) / 'weights.json'

            # Initialize zero weights
            json.dump([0.0] * NUM_FEATURES, open(weights_path, 'w'))

            runner = CLIRunner(get_project_root())
            result = runner.simulate(
                home_ai='learning',
                away_ai='random',
                matches=1,
                weights=str(weights_path),
                epsilon=0.3,
                log_dir=str(log_dir),
                timeout=300,
            )

            assert result.matches == 1
            log_files = sorted(log_dir.glob('game_*.jsonl'))
            assert len(log_files) == 1

            # Log should have state entries and a result entry
            records = _read_jsonl(log_files[0])
            states = [r for r in records if r['type'] == 'state']
            results = [r for r in records if r['type'] == 'result']
            assert len(states) > 0, 'Should have state records'
            assert len(results) == 1, 'Should have exactly one result'
            assert len(states[0]['features']) == NUM_FEATURES

    def test_train_from_logs_changes_weights(self):
        """Train on game logs and verify weights change from zeros."""
        with tempfile.TemporaryDirectory() as tmpdir:
            log_dir = Path(tmpdir) / 'logs'
            weights_path = Path(tmpdir) / 'weights.json'

            json.dump([0.0] * NUM_FEATURES, open(weights_path, 'w'))

            runner = CLIRunner(get_project_root())
            runner.simulate(
                home_ai='learning',
                away_ai='random',
                matches=1,
                weights=str(weights_path),
                epsilon=0.5,
                log_dir=str(log_dir),
                timeout=300,
            )

            # Train
            trainer = LinearTrainer(n_features=NUM_FEATURES, learning_rate=0.01)
            log_files = sorted(log_dir.glob('game_*.jsonl'))
            for log_file in log_files:
                game_log = _read_jsonl(log_file)
                trainer.train_monte_carlo(game_log)

            trainer.save_weights(str(weights_path))

            with open(weights_path) as f:
                weights = json.load(f)
            assert any(w != 0.0 for w in weights), 'Weights should change after training'

    def test_trained_weights_work_in_new_games(self):
        """Use trained weights for new games without errors."""
        with tempfile.TemporaryDirectory() as tmpdir:
            weights_path = Path(tmpdir) / 'weights.json'

            import numpy as np
            weights = (np.random.randn(NUM_FEATURES) * 0.5).tolist()
            json.dump(weights, open(weights_path, 'w'))

            runner = CLIRunner(get_project_root())
            result = runner.simulate(
                home_ai='learning',
                away_ai='random',
                matches=1,
                weights=str(weights_path),
                epsilon=0.0,
                timeout=300,
            )

            assert result.matches == 1
            assert result.results[0].phase == 'game_over'

    def test_win_rate_is_valid(self):
        """After running a game, win rate should be a valid number (0-1)."""
        with tempfile.TemporaryDirectory() as tmpdir:
            weights_path = Path(tmpdir) / 'weights.json'
            json.dump([0.0] * NUM_FEATURES, open(weights_path, 'w'))

            runner = CLIRunner(get_project_root())
            result = runner.simulate(
                home_ai='learning',
                away_ai='random',
                matches=1,
                weights=str(weights_path),
                epsilon=0.3,
                timeout=300,
            )

            win_rate = result.home_win_rate
            assert 0.0 <= win_rate <= 1.0

    def test_mini_training_csv_output(self):
        """Run 1 epoch x 1 game and verify CSV has 1 row."""
        with tempfile.TemporaryDirectory() as tmpdir:
            weights_path = Path(tmpdir) / 'weights.json'
            csv_path = Path(tmpdir) / 'results.csv'
            log_dir = Path(tmpdir) / 'logs'

            json.dump([0.0] * NUM_FEATURES, open(weights_path, 'w'))

            runner = CLIRunner(get_project_root())
            trainer = LinearTrainer(n_features=NUM_FEATURES, learning_rate=0.01)

            with open(csv_path, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(['epoch', 'win_rate', 'avg_score_diff', 'epsilon'])

            epoch_log_dir = log_dir / 'epoch_001'
            epoch_log_dir.mkdir(parents=True, exist_ok=True)

            result = runner.simulate(
                home_ai='learning',
                away_ai='random',
                matches=1,
                weights=str(weights_path),
                epsilon=0.3,
                log_dir=str(epoch_log_dir),
                timeout=300,
            )

            for log_file in sorted(epoch_log_dir.glob('game_*.jsonl')):
                game_log = _read_jsonl(log_file)
                trainer.train_monte_carlo(game_log)
            trainer.save_weights(str(weights_path))

            win_rate = result.home_win_rate
            avg_diff = sum(r.home_score - r.away_score for r in result.results) / max(len(result.results), 1)

            with open(csv_path, 'a', newline='') as f:
                writer = csv.writer(f)
                writer.writerow([1, f'{win_rate:.3f}', f'{avg_diff:.2f}', '0.300'])

            with open(csv_path) as f:
                reader = csv.DictReader(f)
                rows = list(reader)
            assert len(rows) == 1
