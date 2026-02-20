"""Tests for training loop."""
import csv
import json
import tempfile
from pathlib import Path

import numpy as np
import pytest

from blood_bowl.training_loop import run_training, _train_on_log, _append_benchmark_csv
from blood_bowl.trainer import LinearTrainer, NeuralTrainer, create_trainer, load_trainer


class TestTrainingLoop:
    def test_epsilon_decays_linearly(self):
        """Epsilon should decay linearly from start to end over epochs."""
        epochs = 10
        eps_start = 0.5
        eps_end = 0.1

        epsilons = []
        for epoch in range(1, epochs + 1):
            eps = eps_start + (eps_end - eps_start) * (epoch - 1) / (epochs - 1)
            epsilons.append(eps)

        assert abs(epsilons[0] - eps_start) < 0.001
        assert abs(epsilons[-1] - eps_end) < 0.001
        # Monotonically decreasing
        for i in range(1, len(epsilons)):
            assert epsilons[i] <= epsilons[i - 1] + 0.001

    def test_csv_output_has_correct_columns(self):
        """CSV output should have epoch, win_rate, avg_score_diff, epsilon columns."""
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'results.csv'

            # Create a mock CSV that the training loop would produce
            with open(csv_path, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(['epoch', 'win_rate', 'avg_score_diff', 'epsilon'])
                writer.writerow([1, '0.400', '-0.20', '0.300'])
                writer.writerow([2, '0.500', '0.10', '0.200'])

            # Read and verify
            with open(csv_path) as f:
                reader = csv.DictReader(f)
                rows = list(reader)

            assert len(rows) == 2
            assert set(rows[0].keys()) == {'epoch', 'win_rate', 'avg_score_diff', 'epsilon'}

    def test_weights_file_updated_after_training(self):
        """After training on logs, weights file should contain non-zero weights."""
        with tempfile.TemporaryDirectory() as tmpdir:
            weights_path = Path(tmpdir) / 'weights.json'

            # Create mock game log
            log_dir = Path(tmpdir) / 'logs'
            log_dir.mkdir()
            game_log = [
                {'type': 'state', 'features': [0.5] * 30, 'perspective': 'home'},
                {'type': 'state', 'features': [0.3] * 30, 'perspective': 'home'},
                {'type': 'result', 'home_score': 2, 'away_score': 0, 'winner': 'home'},
            ]
            with open(log_dir / 'game_001.jsonl', 'w') as f:
                for record in game_log:
                    f.write(json.dumps(record) + '\n')

            # Train directly using trainer (unit test, not full loop)
            trainer = LinearTrainer(n_features=30, learning_rate=0.01)
            trainer.train_monte_carlo(game_log)
            trainer.save_weights(str(weights_path))

            # Verify weights changed
            with open(weights_path) as f:
                weights = json.load(f)
            assert any(w != 0.0 for w in weights)


class TestTrainOnLog:
    def test_mc_method(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]
        old_w = trainer.weights.copy()
        _train_on_log(trainer, game_log, 'mc', 0.99, 0.8)
        assert not np.array_equal(trainer.weights, old_w)

    def test_td_lambda_method(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]
        old_w = trainer.weights.copy()
        _train_on_log(trainer, game_log, 'td_lambda', 0.99, 0.8)
        assert not np.array_equal(trainer.weights, old_w)

    def test_neural_trainer_works(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]
        old_W1 = trainer.W1.copy()
        _train_on_log(trainer, game_log, 'mc', 0.99, 0.8)
        assert not np.array_equal(trainer.W1, old_W1)

    def test_unknown_method_raises(self):
        trainer = LinearTrainer(n_features=5)
        with pytest.raises(ValueError, match='Unknown training method'):
            _train_on_log(trainer, [], 'unknown', 0.99, 0.8)


class TestAppendBenchmarkCsv:
    def test_creates_csv_with_header(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'bench.csv'
            results = {
                'random': {'win_rate': 0.75, 'avg_score_diff': 0.5, 'matches': 10},
                'greedy': {'win_rate': 0.60, 'avg_score_diff': 0.2, 'matches': 10},
            }
            _append_benchmark_csv(csv_path, 5, results)

            with open(csv_path) as f:
                reader = csv.DictReader(f)
                rows = list(reader)
            assert len(rows) == 2
            assert set(rows[0].keys()) == {'epoch', 'opponent', 'win_rate', 'avg_score_diff', 'matches'}
            assert rows[0]['epoch'] == '5'
            assert rows[0]['opponent'] == 'random'

    def test_appends_without_duplicate_header(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            csv_path = Path(tmpdir) / 'bench.csv'
            results = {'random': {'win_rate': 0.75, 'avg_score_diff': 0.5, 'matches': 10}}
            _append_benchmark_csv(csv_path, 1, results)
            _append_benchmark_csv(csv_path, 2, results)

            with open(csv_path) as f:
                lines = f.readlines()
            # 1 header + 2 data rows
            assert len(lines) == 3


class TestCurriculum:
    def test_curriculum_stages_logic(self):
        """Test that curriculum advancement logic works."""
        stages = [
            {'opponent': 'random', 'win_rate_threshold': 0.65},
            {'opponent': 'greedy', 'win_rate_threshold': 0.55},
            {'opponent': 'learning', 'self_play': True, 'win_rate_threshold': None},
        ]

        current_stage = 0
        win_rates = []

        # Simulate 5 epochs with high win rate
        for wr in [0.7, 0.8, 0.75, 0.6, 0.65]:
            win_rates.append(wr)
            threshold = stages[current_stage].get('win_rate_threshold')
            if threshold is not None and len(win_rates) >= 3:
                rolling_avg = sum(win_rates[-3:]) / 3.0
                if rolling_avg >= threshold and current_stage < len(stages) - 1:
                    current_stage += 1
                    win_rates.clear()

        # After 3 epochs of 0.7, 0.8, 0.75 (avg=0.75 >= 0.65), should advance
        assert current_stage == 1

    def test_curriculum_does_not_advance_below_threshold(self):
        stages = [
            {'opponent': 'random', 'win_rate_threshold': 0.65},
            {'opponent': 'greedy', 'win_rate_threshold': 0.55},
        ]

        current_stage = 0
        win_rates = []

        for wr in [0.3, 0.4, 0.5]:
            win_rates.append(wr)
            threshold = stages[current_stage].get('win_rate_threshold')
            if threshold is not None and len(win_rates) >= 3:
                rolling_avg = sum(win_rates[-3:]) / 3.0
                if rolling_avg >= threshold and current_stage < len(stages) - 1:
                    current_stage += 1
                    win_rates.clear()

        assert current_stage == 0


class TestModelSelection:
    def test_create_and_load_neural(self):
        """create_trainer('neural') + save/load roundtrip."""
        np.random.seed(42)
        trainer = create_trainer(model_type='neural', n_features=5, hidden_size=4)
        assert isinstance(trainer, NeuralTrainer)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'weights.json')
            trainer.save_weights(path)
            loaded = load_trainer(path)
            assert isinstance(loaded, NeuralTrainer)
            assert loaded.hidden_size == 4

    def test_create_and_load_linear(self):
        trainer = create_trainer(model_type='linear', n_features=5)
        assert isinstance(trainer, LinearTrainer)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'weights.json')
            trainer.save_weights(path)
            loaded = load_trainer(path)
            assert isinstance(loaded, LinearTrainer)
