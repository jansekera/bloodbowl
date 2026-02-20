"""Policy network trainer for AlphaZero-style MCTS policy improvement.

Trains a linear policy network P(a|s) from MCTS visit distributions.
The policy learns to predict which actions MCTS would explore most,
enabling more focused search in future games.
"""
from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from .features import NUM_FEATURES

NUM_ACTION_FEATURES = 15
POLICY_INPUT_SIZE = NUM_FEATURES + NUM_ACTION_FEATURES  # 85


class PolicyTrainer:
    """Linear policy network: logit = weights @ concat(state, action) + bias."""

    def __init__(self, n_features: int = POLICY_INPUT_SIZE, learning_rate: float = 0.01,
                 temperature: float = 0.3):
        self.weights = np.zeros(n_features, dtype=np.float64)
        self.bias = 0.0
        self.lr = learning_rate
        self.temperature = temperature  # Inference-time softmax temperature (C++ side)

    def train_on_decisions(self, decisions: list[dict]) -> float:
        """Train on MCTS visit distributions using cross-entropy loss.

        Each decision contains:
        - state_features: np.array of shape (70,)
        - visits: list of {action_features: np.array(15,), visit_fraction: float}

        Returns average cross-entropy loss over all decisions.
        """
        if not decisions:
            return 0.0

        total_loss = 0.0
        n_decisions = 0

        for dec in decisions:
            state_feats = np.array(dec['state_features'], dtype=np.float64)
            visits = dec.get('visits', [])
            if not visits:
                continue

            n_actions = len(visits)

            # Build input matrix: each row is concat(state, action)
            inputs = np.zeros((n_actions, len(self.weights)), dtype=np.float64)
            targets = np.zeros(n_actions, dtype=np.float64)

            for i, v in enumerate(visits):
                action_feats = np.array(v['action_features'], dtype=np.float64)
                # Concat state + action features
                n_state = min(len(state_feats), NUM_FEATURES)
                n_action = min(len(action_feats), NUM_ACTION_FEATURES)
                inputs[i, :n_state] = state_feats[:n_state]
                inputs[i, NUM_FEATURES:NUM_FEATURES + n_action] = action_feats[:n_action]
                targets[i] = v['visit_fraction']

            # Normalize targets to sum to 1 (they should already, but safety)
            target_sum = targets.sum()
            if target_sum > 0:
                targets /= target_sum

            # Forward: logits = inputs @ weights + bias
            logits = inputs @ self.weights + self.bias

            # Softmax (numerically stable)
            logits_shifted = logits - logits.max()
            exp_logits = np.exp(logits_shifted)
            probs = exp_logits / exp_logits.sum()

            # Cross-entropy loss: -sum(targets * log(probs))
            loss = -np.sum(targets * np.log(probs + 1e-8))
            total_loss += loss

            # Gradient: d_loss/d_logits = probs - targets (softmax + CE)
            grad_logits = probs - targets  # shape (n_actions,)

            # d_loss/d_weights = sum_i grad_logits[i] * inputs[i]
            grad_weights = grad_logits @ inputs  # shape (n_features,)
            grad_bias = grad_logits.sum()

            # Update with gradient clipping
            self.weights -= self.lr * grad_weights
            self.bias -= self.lr * grad_bias

            # Clip weights to prevent extreme values
            np.clip(self.weights, -5.0, 5.0, out=self.weights)
            self.bias = max(-5.0, min(5.0, self.bias))

            n_decisions += 1

        return total_loss / max(n_decisions, 1)

    def save_weights(self, path: str) -> None:
        """Save policy weights as JSON."""
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        data = {
            'policy_weights': self.weights.tolist(),
            'policy_bias': self.bias,
        }
        with open(path, 'w') as f:
            json.dump(data, f)

    def load_weights(self, path: str) -> None:
        """Load policy weights from JSON."""
        with open(path) as f:
            data = json.load(f)

        if isinstance(data, dict):
            if 'policy_weights' in data:
                loaded = np.array(data['policy_weights'], dtype=np.float64)
                if len(loaded) < len(self.weights):
                    padded = np.zeros(len(self.weights), dtype=np.float64)
                    padded[:len(loaded)] = loaded
                    self.weights = padded
                else:
                    self.weights = loaded[:len(self.weights)]
                self.bias = data.get('policy_bias', 0.0)


def save_combined_weights(value_trainer, policy_trainer: PolicyTrainer, path: str) -> None:
    """Save value + policy weights into a single JSON file.

    Format: {"type": "alphazero_linear", "value_weights": [...],
             "policy_weights": [...], "policy_bias": 0.0}
    """
    Path(path).parent.mkdir(parents=True, exist_ok=True)

    # Determine value trainer type
    from .trainer import LinearTrainer, NeuralTrainer

    if isinstance(value_trainer, NeuralTrainer):
        data = {
            'type': 'alphazero_neural',
            'hidden_size': value_trainer.hidden_size,
            'n_features': value_trainer.n_features,
            'value_W1': value_trainer.W1.tolist(),
            'value_b1': value_trainer.b1.tolist(),
            'value_W2': value_trainer.W2.tolist(),
            'value_b2': value_trainer.b2.tolist(),
            'policy_weights': policy_trainer.weights.tolist(),
            'policy_bias': policy_trainer.bias,
            'policy_temperature': policy_trainer.temperature,
        }
    else:
        data = {
            'type': 'alphazero_linear',
            'value_weights': value_trainer.weights.tolist(),
            'policy_weights': policy_trainer.weights.tolist(),
            'policy_bias': policy_trainer.bias,
            'policy_temperature': policy_trainer.temperature,
        }

    with open(path, 'w') as f:
        json.dump(data, f)


def load_combined_weights(
    path: str,
    value_lr: float = 0.01,
    policy_lr: float = 0.01,
):
    """Load both value and policy trainers from a combined weights file.

    Returns (value_trainer, policy_trainer).
    """
    from .trainer import LinearTrainer, NeuralTrainer, load_trainer

    with open(path) as f:
        data = json.load(f)

    policy_trainer = PolicyTrainer(learning_rate=policy_lr)

    if isinstance(data, dict) and data.get('type', '').startswith('alphazero'):
        # Combined format
        if 'policy_weights' in data:
            policy_trainer.weights = np.array(data['policy_weights'], dtype=np.float64)
            if len(policy_trainer.weights) < POLICY_INPUT_SIZE:
                padded = np.zeros(POLICY_INPUT_SIZE, dtype=np.float64)
                padded[:len(policy_trainer.weights)] = policy_trainer.weights
                policy_trainer.weights = padded
            policy_trainer.bias = data.get('policy_bias', 0.0)
            policy_trainer.temperature = data.get('policy_temperature', 0.3)

        if data['type'] == 'alphazero_neural':
            n_features = data.get('n_features', len(data['value_W1']))
            hidden_size = data.get('hidden_size', len(data['value_b1']))
            value_trainer = NeuralTrainer(
                n_features=n_features,
                hidden_size=hidden_size,
                learning_rate=value_lr,
            )
            value_trainer.W1 = np.array(data['value_W1'], dtype=np.float64)
            value_trainer.b1 = np.array(data['value_b1'], dtype=np.float64)
            value_trainer.W2 = np.array(data['value_W2'], dtype=np.float64)
            value_trainer.b2 = np.array(data['value_b2'], dtype=np.float64)
        else:
            # alphazero_linear
            value_trainer = LinearTrainer(learning_rate=value_lr)
            value_trainer.weights = np.array(data['value_weights'], dtype=np.float64)
            if len(value_trainer.weights) < NUM_FEATURES:
                padded = np.zeros(NUM_FEATURES, dtype=np.float64)
                padded[:len(value_trainer.weights)] = value_trainer.weights
                value_trainer.weights = padded

        return value_trainer, policy_trainer
    else:
        # Legacy format: just value weights, policy starts from scratch
        value_trainer = load_trainer(path, learning_rate=value_lr)
        return value_trainer, policy_trainer
