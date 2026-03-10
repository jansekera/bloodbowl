"""Policy network trainer for AlphaZero-style MCTS policy improvement.

Trains a policy network P(a|s) from MCTS visit distributions.
The policy learns to predict which actions MCTS would explore most,
enabling more focused search in future games.

Supports linear (85 -> 1) and neural (85 -> H -> 1) architectures.
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

    def train_on_decisions(self, decisions: list[dict], passes: int = 1,
                           batch_size: int = 0) -> float:
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


class NeuralPolicyTrainer:
    """Neural policy network: input(85) -> hidden(ReLU) -> output(1)."""

    def __init__(self, n_features: int = POLICY_INPUT_SIZE, hidden_size: int = 32,
                 learning_rate: float = 0.01, temperature: float = 0.3):
        self.n_features = n_features
        self.hidden_size = hidden_size
        self.lr = learning_rate
        self.temperature = temperature

        # Xavier initialization
        scale1 = np.sqrt(2.0 / n_features)
        self.W1 = np.random.randn(n_features, hidden_size) * scale1
        self.b1 = np.zeros(hidden_size, dtype=np.float64)

        scale2 = np.sqrt(2.0 / hidden_size)
        self.W2 = np.random.randn(hidden_size) * scale2
        self.b2 = 0.0

    def train_on_decisions(self, decisions: list[dict], passes: int = 1,
                           batch_size: int = 32) -> float:
        """Train on MCTS visit distributions with mini-batch SGD.

        Args:
            decisions: list of {state_features, visits: [{action_features, visit_fraction}]}
            passes: number of passes over the data (default 1)
            batch_size: mini-batch size for gradient updates (default 32)
        """
        if not decisions:
            return 0.0

        total_loss = 0.0
        n_decisions = 0

        for pass_idx in range(passes):
            # Shuffle decisions each pass
            if passes > 1:
                indices = np.random.permutation(len(decisions))
            else:
                indices = np.arange(len(decisions))

            # Mini-batch accumulators
            dW1 = np.zeros_like(self.W1)
            db1 = np.zeros_like(self.b1)
            dW2 = np.zeros_like(self.W2)
            db2 = 0.0
            batch_count = 0

            for idx in indices:
                dec = decisions[idx]
                state_feats = np.array(dec['state_features'], dtype=np.float64)
                visits = dec.get('visits', [])
                if not visits:
                    continue

                n_actions = len(visits)
                inputs = np.zeros((n_actions, self.n_features), dtype=np.float64)
                targets = np.zeros(n_actions, dtype=np.float64)

                for i, v in enumerate(visits):
                    action_feats = np.array(v['action_features'], dtype=np.float64)
                    n_state = min(len(state_feats), NUM_FEATURES)
                    n_action = min(len(action_feats), NUM_ACTION_FEATURES)
                    inputs[i, :n_state] = state_feats[:n_state]
                    inputs[i, NUM_FEATURES:NUM_FEATURES + n_action] = action_feats[:n_action]
                    targets[i] = v['visit_fraction']

                target_sum = targets.sum()
                if target_sum > 0:
                    targets /= target_sum

                # Forward: hidden = ReLU(inputs @ W1 + b1), logits = hidden @ W2 + b2
                z1 = inputs @ self.W1 + self.b1     # (N, H)
                h1 = np.maximum(0, z1)               # ReLU
                logits = h1 @ self.W2 + self.b2      # (N,)

                # Softmax
                logits_shifted = logits - logits.max()
                exp_logits = np.exp(logits_shifted)
                probs = exp_logits / exp_logits.sum()

                # Cross-entropy loss (only count on last pass for reporting)
                if pass_idx == passes - 1:
                    loss = -np.sum(targets * np.log(probs + 1e-8))
                    total_loss += loss
                    n_decisions += 1

                # Backward: d_logits = probs - targets
                d_logits = probs - targets  # (N,)

                dW2 += h1.T @ d_logits
                db2 += d_logits.sum()
                d_h1 = np.outer(d_logits, self.W2)
                d_z1 = d_h1 * (z1 > 0)
                dW1 += inputs.T @ d_z1
                db1 += d_z1.sum(axis=0)

                batch_count += 1

                # Apply gradient update every batch_size decisions
                if batch_count >= batch_size:
                    scale = 1.0 / batch_count
                    self.W1 -= self.lr * dW1 * scale
                    self.b1 -= self.lr * db1 * scale
                    self.W2 -= self.lr * dW2 * scale
                    self.b2 -= self.lr * db2 * scale
                    # Reset accumulators
                    dW1[:] = 0
                    db1[:] = 0
                    dW2[:] = 0
                    db2 = 0.0
                    batch_count = 0

            # Apply remaining gradients
            if batch_count > 0:
                scale = 1.0 / batch_count
                self.W1 -= self.lr * dW1 * scale
                self.b1 -= self.lr * db1 * scale
                self.W2 -= self.lr * dW2 * scale
                self.b2 -= self.lr * db2 * scale

        # Clip weights
        np.clip(self.W1, -5.0, 5.0, out=self.W1)
        np.clip(self.W2, -5.0, 5.0, out=self.W2)

        return total_loss / max(n_decisions, 1)

    def save_weights(self, path: str) -> None:
        """Save neural policy weights as JSON."""
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        data = {
            'policy_type': 'neural',
            'policy_hidden_size': self.hidden_size,
            'policy_W1': self.W1.flatten().tolist(),  # row-major
            'policy_b1': self.b1.tolist(),
            'policy_W2': self.W2.tolist(),
            'policy_b2': float(self.b2),
        }
        with open(path, 'w') as f:
            json.dump(data, f)

    def load_weights(self, path: str) -> None:
        """Load neural policy weights from JSON."""
        with open(path) as f:
            data = json.load(f)

        if isinstance(data, dict) and data.get('policy_type') == 'neural':
            self.hidden_size = data['policy_hidden_size']
            W1_flat = np.array(data['policy_W1'], dtype=np.float64)
            self.W1 = W1_flat.reshape(self.n_features, self.hidden_size)
            self.b1 = np.array(data['policy_b1'], dtype=np.float64)
            self.W2 = np.array(data['policy_W2'], dtype=np.float64)
            self.b2 = data.get('policy_b2', 0.0)


def create_policy_trainer(policy_model: str = 'linear', hidden_size: int = 32,
                          learning_rate: float = 0.01) -> PolicyTrainer | NeuralPolicyTrainer:
    """Factory function to create the appropriate policy trainer."""
    if policy_model == 'neural':
        return NeuralPolicyTrainer(hidden_size=hidden_size, learning_rate=learning_rate)
    return PolicyTrainer(learning_rate=learning_rate)


def save_combined_weights(value_trainer, policy_trainer, path: str) -> None:
    """Save value + policy weights into a single JSON file."""
    Path(path).parent.mkdir(parents=True, exist_ok=True)

    from .trainer import LinearTrainer, NeuralTrainer

    # Start with value trainer data
    if isinstance(value_trainer, NeuralTrainer):
        data = {
            'type': 'alphazero_neural',
            'hidden_size': value_trainer.hidden_size,
            'n_features': value_trainer.n_features,
            'value_W1': value_trainer.W1.tolist(),
            'value_b1': value_trainer.b1.tolist(),
            'value_W2': value_trainer.W2.tolist(),
            'value_b2': value_trainer.b2.tolist(),
        }
    else:
        data = {
            'type': 'alphazero_linear',
            'value_weights': value_trainer.weights.tolist(),
        }

    # Add policy data (skip if no policy trainer)
    if policy_trainer is None:
        pass
    elif isinstance(policy_trainer, NeuralPolicyTrainer):
        data['policy_type'] = 'neural'
        data['policy_hidden_size'] = policy_trainer.hidden_size
        data['policy_W1'] = policy_trainer.W1.flatten().tolist()
        data['policy_b1'] = policy_trainer.b1.tolist()
        data['policy_W2'] = policy_trainer.W2.tolist()
        data['policy_b2'] = float(policy_trainer.b2)
        data['policy_temperature'] = policy_trainer.temperature
    else:
        data['policy_weights'] = policy_trainer.weights.tolist()
        data['policy_bias'] = policy_trainer.bias
        data['policy_temperature'] = policy_trainer.temperature

    with open(path, 'w') as f:
        json.dump(data, f)


def load_combined_weights(
    path: str,
    value_lr: float = 0.01,
    policy_lr: float = 0.01,
    policy_model: str = 'auto',
):
    """Load both value and policy trainers from a combined weights file.

    Returns (value_trainer, policy_trainer).
    """
    from .trainer import LinearTrainer, NeuralTrainer, load_trainer

    with open(path) as f:
        data = json.load(f)

    # Detect policy type from saved data
    saved_policy_type = data.get('policy_type', 'linear') if isinstance(data, dict) else 'linear'

    # Create appropriate policy trainer
    if saved_policy_type == 'neural' or (policy_model == 'neural' and saved_policy_type != 'neural'):
        policy_trainer = NeuralPolicyTrainer(learning_rate=policy_lr)
    else:
        policy_trainer = PolicyTrainer(learning_rate=policy_lr)

    if isinstance(data, dict) and data.get('type', '').startswith('alphazero'):
        # Load policy weights
        if saved_policy_type == 'neural' and 'policy_W1' in data:
            if isinstance(policy_trainer, NeuralPolicyTrainer):
                policy_trainer.hidden_size = data['policy_hidden_size']
                W1_flat = np.array(data['policy_W1'], dtype=np.float64)
                policy_trainer.W1 = W1_flat.reshape(policy_trainer.n_features,
                                                     policy_trainer.hidden_size)
                policy_trainer.b1 = np.array(data['policy_b1'], dtype=np.float64)
                policy_trainer.W2 = np.array(data['policy_W2'], dtype=np.float64)
                policy_trainer.b2 = data.get('policy_b2', 0.0)
                policy_trainer.temperature = data.get('policy_temperature', 0.3)
        elif 'policy_weights' in data:
            if isinstance(policy_trainer, PolicyTrainer) and not isinstance(policy_trainer, NeuralPolicyTrainer):
                policy_trainer.weights = np.array(data['policy_weights'], dtype=np.float64)
                if len(policy_trainer.weights) < POLICY_INPUT_SIZE:
                    padded = np.zeros(POLICY_INPUT_SIZE, dtype=np.float64)
                    padded[:len(policy_trainer.weights)] = policy_trainer.weights
                    policy_trainer.weights = padded
                policy_trainer.bias = data.get('policy_bias', 0.0)
                policy_trainer.temperature = data.get('policy_temperature', 0.3)

        # Load value trainer
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
