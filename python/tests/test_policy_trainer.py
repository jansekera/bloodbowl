"""Tests for PolicyTrainer."""
import json
import tempfile
from pathlib import Path

import numpy as np
import pytest

from blood_bowl.policy_trainer import (
    PolicyTrainer,
    save_combined_weights,
    load_combined_weights,
    NUM_ACTION_FEATURES,
    POLICY_INPUT_SIZE,
)
from blood_bowl.trainer import LinearTrainer, NeuralTrainer
from blood_bowl.features import NUM_FEATURES


def _make_decision(n_actions=3, seed=42):
    """Create a synthetic policy decision."""
    rng = np.random.RandomState(seed)
    state_feats = rng.randn(NUM_FEATURES).tolist()

    visits = []
    fractions = rng.dirichlet(np.ones(n_actions))
    for i in range(n_actions):
        visits.append({
            'action_features': rng.randn(NUM_ACTION_FEATURES).tolist(),
            'visit_fraction': float(fractions[i]),
        })

    return {
        'state_features': state_feats,
        'perspective': 'home',
        'visits': visits,
    }


class TestPolicyTrainer:
    def test_init(self):
        pt = PolicyTrainer()
        assert len(pt.weights) == POLICY_INPUT_SIZE
        assert pt.bias == 0.0
        assert np.all(pt.weights == 0.0)

    def test_train_reduces_loss(self):
        pt = PolicyTrainer(learning_rate=0.1)
        decisions = [_make_decision(seed=i) for i in range(10)]

        loss1 = pt.train_on_decisions(decisions)
        loss2 = pt.train_on_decisions(decisions)
        loss3 = pt.train_on_decisions(decisions)

        # Loss should decrease over iterations
        assert loss3 < loss1

    def test_train_empty(self):
        pt = PolicyTrainer()
        loss = pt.train_on_decisions([])
        assert loss == 0.0

    def test_gradient_changes_weights(self):
        pt = PolicyTrainer(learning_rate=0.01)
        initial_weights = pt.weights.copy()

        decisions = [_make_decision()]
        pt.train_on_decisions(decisions)

        # Weights should have changed
        assert not np.allclose(pt.weights, initial_weights)

    def test_save_load_roundtrip(self):
        pt = PolicyTrainer(learning_rate=0.05)
        decisions = [_make_decision(seed=i) for i in range(5)]
        pt.train_on_decisions(decisions)

        with tempfile.NamedTemporaryFile(suffix='.json', mode='w', delete=False) as f:
            path = f.name

        pt.save_weights(path)

        pt2 = PolicyTrainer()
        pt2.load_weights(path)

        np.testing.assert_allclose(pt.weights, pt2.weights, atol=1e-10)
        assert abs(pt.bias - pt2.bias) < 1e-10

        Path(path).unlink()


class TestCombinedWeights:
    def test_save_load_linear(self):
        vt = LinearTrainer(learning_rate=0.01)
        vt.weights = np.random.randn(NUM_FEATURES)
        pt = PolicyTrainer(learning_rate=0.02)
        pt.weights = np.random.randn(POLICY_INPUT_SIZE)
        pt.bias = 0.5

        with tempfile.NamedTemporaryFile(suffix='.json', mode='w', delete=False) as f:
            path = f.name

        save_combined_weights(vt, pt, path)

        vt2, pt2 = load_combined_weights(path, value_lr=0.01, policy_lr=0.02)

        assert isinstance(vt2, LinearTrainer)
        np.testing.assert_allclose(vt.weights, vt2.weights, atol=1e-10)
        np.testing.assert_allclose(pt.weights, pt2.weights, atol=1e-10)
        assert abs(pt.bias - pt2.bias) < 1e-10

        Path(path).unlink()

    def test_save_load_neural(self):
        vt = NeuralTrainer(n_features=NUM_FEATURES, hidden_size=16, learning_rate=0.01)
        pt = PolicyTrainer(learning_rate=0.02)
        pt.weights = np.random.randn(POLICY_INPUT_SIZE)
        pt.bias = -0.3

        with tempfile.NamedTemporaryFile(suffix='.json', mode='w', delete=False) as f:
            path = f.name

        save_combined_weights(vt, pt, path)

        vt2, pt2 = load_combined_weights(path, value_lr=0.01, policy_lr=0.02)

        assert isinstance(vt2, NeuralTrainer)
        np.testing.assert_allclose(vt.W1, vt2.W1, atol=1e-10)
        np.testing.assert_allclose(vt.b1, vt2.b1, atol=1e-10)
        np.testing.assert_allclose(pt.weights, pt2.weights, atol=1e-10)
        assert abs(pt.bias - pt2.bias) < 1e-10

        Path(path).unlink()

    def test_load_legacy_format(self):
        """Loading old-format weights should return fresh policy trainer."""
        weights = np.random.randn(NUM_FEATURES).tolist()

        with tempfile.NamedTemporaryFile(suffix='.json', mode='w', delete=False) as f:
            json.dump(weights, f)
            path = f.name

        vt, pt = load_combined_weights(path, value_lr=0.01, policy_lr=0.02)

        assert isinstance(vt, LinearTrainer)
        np.testing.assert_allclose(vt.weights, np.array(weights), atol=1e-10)
        # Policy trainer should be fresh (zero weights)
        assert np.all(pt.weights == 0.0)

        Path(path).unlink()

    def test_combined_format_json_structure(self):
        vt = LinearTrainer(learning_rate=0.01)
        vt.weights = np.ones(NUM_FEATURES)
        pt = PolicyTrainer()
        pt.weights = np.ones(POLICY_INPUT_SIZE) * 0.5

        with tempfile.NamedTemporaryFile(suffix='.json', mode='w', delete=False) as f:
            path = f.name

        save_combined_weights(vt, pt, path)

        with open(path) as f:
            data = json.load(f)

        assert data['type'] == 'alphazero_linear'
        assert 'value_weights' in data
        assert 'policy_weights' in data
        assert 'policy_bias' in data
        assert len(data['value_weights']) == NUM_FEATURES
        assert len(data['policy_weights']) == POLICY_INPUT_SIZE

        Path(path).unlink()
