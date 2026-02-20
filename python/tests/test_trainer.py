"""Tests for LinearTrainer and NeuralTrainer."""
from __future__ import annotations

import json
import tempfile
from pathlib import Path

import numpy as np
import pytest

from blood_bowl.trainer import LinearTrainer, NeuralTrainer, create_trainer, load_trainer


def _make_alternating_game_log() -> list[dict]:
    """Game log with alternating home/away states (realistic scenario)."""
    return [
        {'type': 'state', 'features': [1.0, 0.5, 0.0, 0.0, 1.0], 'perspective': 'home'},
        {'type': 'state', 'features': [0.0, 0.0, 0.5, 1.0, 0.5], 'perspective': 'away'},
        {'type': 'state', 'features': [1.0, 0.6, 0.0, 0.0, 1.0], 'perspective': 'home'},
        {'type': 'state', 'features': [0.0, 0.0, 0.6, 1.0, 0.5], 'perspective': 'away'},
        {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
    ]


class TestLinearTrainer:
    def test_train_monte_carlo_win_updates_toward_features(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.6, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        old_weights = trainer.weights.copy()
        trainer.train_monte_carlo(game_log)

        # Weights should change in direction of features (reward=+1)
        assert not np.array_equal(trainer.weights, old_weights)
        # Feature 0 has value 1.0 in both states, so weight[0] should increase
        assert trainer.weights[0] > old_weights[0]

    def test_train_monte_carlo_loss_updates_against_features(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 0, 'away_score': 1, 'winner': 'away'},
        ]

        old_weights = trainer.weights.copy()
        trainer.train_monte_carlo(game_log)

        # Weights should move away from features (reward=-1)
        assert trainer.weights[0] < old_weights[0]

    def test_evaluate_is_dot_product(self):
        trainer = LinearTrainer(n_features=3)
        trainer.weights = np.array([2.0, 3.0, 1.0])

        result = trainer.evaluate([1.0, 2.0, 3.0])
        # 2*1 + 3*2 + 1*3 = 11
        assert abs(result - 11.0) < 0.001

    def test_save_load_weights_roundtrip(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.01)
        trainer.weights = np.array([1.5, -0.3, 0.7, 0.0, 2.1])

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'weights.json')
            trainer.save_weights(path)

            # Verify JSON format
            with open(path) as f:
                data = json.load(f)
            assert len(data) == 5

            # Load into new trainer
            trainer2 = LinearTrainer(n_features=5)
            trainer2.load_weights(path)

            np.testing.assert_array_almost_equal(trainer.weights, trainer2.weights)


class TestMonteCarloShaped:
    def test_shaped_updates_change_weights(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.5, 0.0, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.6, 0.0, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        old_weights = trainer.weights.copy()
        trainer.train_monte_carlo_shaped(game_log)
        assert not np.array_equal(trainer.weights, old_weights)

    def test_zero_shaping_similar_to_standard_mc(self):
        """With zero shaping weights, shaped MC should behave like standard MC."""
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.6, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        # Standard MC
        trainer_mc = LinearTrainer(n_features=5, learning_rate=0.1)
        trainer_mc.train_monte_carlo(game_log)

        # Shaped MC with zero shaping
        trainer_shaped = LinearTrainer(n_features=5, learning_rate=0.1)
        trainer_shaped.train_monte_carlo_shaped(game_log, shaping_weights=[])

        np.testing.assert_array_almost_equal(trainer_mc.weights, trainer_shaped.weights, decimal=5)

    def test_shaped_handles_alternating_perspectives(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = _make_alternating_game_log()

        old_weights = trainer.weights.copy()
        trainer.train_monte_carlo_shaped(game_log)
        assert not np.array_equal(trainer.weights, old_weights)

    def test_shaped_draw_with_potential(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [0.0, 0.5, 0.0, 0.0, 0.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 1, 'away_score': 1, 'winner': None},
        ]

        # Use shaping that rewards feature[1]
        trainer.train_monte_carlo_shaped(game_log, shaping_weights=[(1, 1.0)])
        # Shaped reward = 0 - 1.0*0.5 = -0.5 → weights should decrease for feature[1]
        assert trainer.weights[1] < 0.0


class TestTDLambda:
    def test_basic_update(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.6, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        old_weights = trainer.weights.copy()
        trainer.train_td_lambda(game_log, lambda_=0.8)
        assert not np.array_equal(trainer.weights, old_weights)

    def test_lambda_zero_matches_td0(self):
        """TD(λ=0) should produce same results as TD(0)."""
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.6, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.7, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        trainer_td0 = LinearTrainer(n_features=5, learning_rate=0.1)
        trainer_td0.train_td0(game_log, gamma=0.99)

        trainer_tdl = LinearTrainer(n_features=5, learning_rate=0.1)
        trainer_tdl.train_td_lambda(game_log, gamma=0.99, lambda_=0.0)

        np.testing.assert_array_almost_equal(trainer_td0.weights, trainer_tdl.weights, decimal=5)

    def test_perspective_isolation(self):
        """Home and away states should be trained independently."""
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = _make_alternating_game_log()

        old_weights = trainer.weights.copy()
        trainer.train_td_lambda(game_log, lambda_=0.8)

        # Should update — both perspectives contribute
        assert not np.array_equal(trainer.weights, old_weights)

    def test_loss_decreases_weights(self):
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.0, 0.0, 0.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 0, 'away_score': 1, 'winner': 'away'},
        ]

        trainer.train_td_lambda(game_log, lambda_=0.8)
        # Reward=-1 for home, only feature[0]=1.0 → weight[0] should decrease
        assert trainer.weights[0] < 0.0


class TestTD0Perspective:
    def test_td0_groups_by_perspective(self):
        """TD(0) should group states by perspective, not bootstrap across alternating sides."""
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = _make_alternating_game_log()

        old_weights = trainer.weights.copy()
        trainer.train_td0(game_log, gamma=0.99)
        assert not np.array_equal(trainer.weights, old_weights)

    def test_td0_single_perspective(self):
        """TD(0) with single perspective should update as before."""
        trainer = LinearTrainer(n_features=5, learning_rate=0.1)
        game_log = [
            {'type': 'state', 'features': [1.0, 0.0, 0.5, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'state', 'features': [1.0, 0.0, 0.6, 0.0, 1.0], 'perspective': 'home'},
            {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
        ]

        old_weights = trainer.weights.copy()
        trainer.train_td0(game_log)
        assert trainer.weights[0] > old_weights[0]


class TestComputePotential:
    def test_basic_potential(self):
        features = np.array([0.0, 0.5, 0.3, 0.0, 1.0])
        shaping = [(1, 3.0), (2, -1.0)]
        result = LinearTrainer._compute_potential(features, shaping)
        # 3.0 * 0.5 + (-1.0) * 0.3 = 1.5 - 0.3 = 1.2
        assert abs(result - 1.2) < 0.001

    def test_empty_shaping(self):
        features = np.array([1.0, 2.0, 3.0])
        result = LinearTrainer._compute_potential(features, [])
        assert result == 0.0

    def test_out_of_bounds_index_ignored(self):
        features = np.array([1.0, 2.0])
        result = LinearTrainer._compute_potential(features, [(0, 1.0), (5, 2.0)])
        assert abs(result - 1.0) < 0.001


# ---- NeuralTrainer tests ----

def _make_neural_game_log(n_features: int = 5) -> list[dict]:
    """Simple game log for neural trainer tests."""
    return [
        {'type': 'state', 'features': [0.5] * n_features, 'perspective': 'home'},
        {'type': 'state', 'features': [0.3] * n_features, 'perspective': 'home'},
        {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
    ]


class TestNeuralTrainer:
    def test_forward_output_in_tanh_range(self):
        """Output of forward pass should be in [-1, 1] due to tanh."""
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        features = np.array([0.5, -0.3, 0.7, 0.0, 1.0])
        value, hidden = trainer.forward(features)
        assert -1.0 <= value <= 1.0
        assert hidden.shape == (4,)
        # ReLU: all hidden activations >= 0
        assert np.all(hidden >= 0)

    def test_forward_deterministic(self):
        """Same input should produce same output."""
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        features = np.array([0.5, -0.3, 0.7, 0.0, 1.0])
        v1, _ = trainer.forward(features)
        v2, _ = trainer.forward(features)
        assert v1 == v2

    def test_evaluate_matches_forward(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        features = [0.5, -0.3, 0.7, 0.0, 1.0]
        v_eval = trainer.evaluate(features)
        v_fwd, _ = trainer.forward(np.array(features, dtype=np.float64))
        assert abs(v_eval - v_fwd) < 1e-10

    def test_train_mc_changes_weights(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        W1_before = trainer.W1.copy()
        trainer.train_monte_carlo(_make_neural_game_log())
        assert not np.array_equal(trainer.W1, W1_before)

    def test_train_mc_shaped_changes_weights(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        W1_before = trainer.W1.copy()
        trainer.train_monte_carlo_shaped(_make_neural_game_log())
        assert not np.array_equal(trainer.W1, W1_before)

    def test_train_td0_changes_weights(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        W1_before = trainer.W1.copy()
        trainer.train_td0(_make_neural_game_log())
        assert not np.array_equal(trainer.W1, W1_before)

    def test_train_td_lambda_changes_weights(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        W1_before = trainer.W1.copy()
        trainer.train_td_lambda(_make_neural_game_log())
        assert not np.array_equal(trainer.W1, W1_before)

    def test_save_load_roundtrip(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        features = [0.5, -0.3, 0.7, 0.0, 1.0]
        v_before = trainer.evaluate(features)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'neural_weights.json')
            trainer.save_weights(path)

            # Verify JSON format
            with open(path) as f:
                data = json.load(f)
            assert data['type'] == 'neural'
            assert data['hidden_size'] == 4
            assert len(data['W1']) == 5  # n_features rows
            assert len(data['W1'][0]) == 4  # hidden_size cols

            # Load into new trainer
            trainer2 = NeuralTrainer(n_features=5, hidden_size=4)
            trainer2.load_weights(path)
            v_after = trainer2.evaluate(features)
            assert abs(v_before - v_after) < 1e-10

    def test_handles_56_features(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=56, hidden_size=32)
        features = [float(i) / 56.0 for i in range(56)]
        value = trainer.evaluate(features)
        assert -1.0 <= value <= 1.0

    def test_feature_alignment_pads(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        # Only 3 features provided — should pad to 5
        short_features = np.array([0.5, -0.3, 0.7])
        padded = trainer._align_features(short_features)
        assert len(padded) == 5
        assert padded[3] == 0.0
        assert padded[4] == 0.0

    def test_feature_alignment_truncates(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4)
        long_features = np.array([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7])
        truncated = trainer._align_features(long_features)
        assert len(truncated) == 5

    def test_gradient_check(self):
        """Numerical gradient check: verify analytical gradients are correct."""
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.01)
        features = np.array([0.5, -0.3, 0.7, 0.0, 1.0])
        target = 0.7

        dW1, db1, dW2, db2 = trainer._backprop(features, target)

        eps = 1e-5

        # Check a few W1 elements
        for i in range(min(3, trainer.n_features)):
            for j in range(min(2, trainer.hidden_size)):
                original = trainer.W1[i, j]

                trainer.W1[i, j] = original + eps
                v_plus, _ = trainer.forward(features)
                loss_plus = 0.5 * (target - v_plus) ** 2

                trainer.W1[i, j] = original - eps
                v_minus, _ = trainer.forward(features)
                loss_minus = 0.5 * (target - v_minus) ** 2

                trainer.W1[i, j] = original
                numerical_grad = (loss_plus - loss_minus) / (2 * eps)
                assert abs(dW1[i, j] - numerical_grad) < 1e-4, \
                    f'W1[{i},{j}]: analytical={dW1[i,j]:.6f}, numerical={numerical_grad:.6f}'

        # Check a few W2 elements
        for j in range(min(3, trainer.hidden_size)):
            original = trainer.W2[j, 0]

            trainer.W2[j, 0] = original + eps
            v_plus, _ = trainer.forward(features)
            loss_plus = 0.5 * (target - v_plus) ** 2

            trainer.W2[j, 0] = original - eps
            v_minus, _ = trainer.forward(features)
            loss_minus = 0.5 * (target - v_minus) ** 2

            trainer.W2[j, 0] = original
            numerical_grad = (loss_plus - loss_minus) / (2 * eps)
            assert abs(dW2[j, 0] - numerical_grad) < 1e-4, \
                f'W2[{j},0]: analytical={dW2[j,0]:.6f}, numerical={numerical_grad:.6f}'

    def test_alternating_perspectives(self):
        np.random.seed(42)
        trainer = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=0.1)
        game_log = _make_alternating_game_log()
        W1_before = trainer.W1.copy()
        trainer.train_td_lambda(game_log, lambda_=0.8)
        assert not np.array_equal(trainer.W1, W1_before)


class TestCreateTrainer:
    def test_create_linear(self):
        trainer = create_trainer(model_type='linear', n_features=10)
        assert isinstance(trainer, LinearTrainer)
        assert len(trainer.weights) == 10

    def test_create_neural(self):
        trainer = create_trainer(model_type='neural', n_features=10, hidden_size=16)
        assert isinstance(trainer, NeuralTrainer)
        assert trainer.n_features == 10
        assert trainer.hidden_size == 16


class TestLoadTrainer:
    def test_load_linear_weights(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'linear.json')
            with open(path, 'w') as f:
                json.dump([1.0, 2.0, 3.0, 4.0, 5.0], f)

            trainer = load_trainer(path)
            assert isinstance(trainer, LinearTrainer)
            assert abs(trainer.evaluate([1.0, 0.0, 0.0, 0.0, 0.0] + [0.0] * 43) - 1.0) < 0.001

    def test_load_neural_weights(self):
        np.random.seed(42)
        original = NeuralTrainer(n_features=5, hidden_size=4)
        features = [0.5, -0.3, 0.7, 0.0, 1.0]
        v_original = original.evaluate(features)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'neural.json')
            original.save_weights(path)

            trainer = load_trainer(path)
            assert isinstance(trainer, NeuralTrainer)
            assert abs(trainer.evaluate(features) - v_original) < 1e-10
