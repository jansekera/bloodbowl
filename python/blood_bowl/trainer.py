"""Value function trainers for Blood Bowl RL (linear + neural)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Union

import numpy as np

from .features import NUM_FEATURES

# Default shaping weights: (feature_index, weight)
DEFAULT_SHAPING_WEIGHTS: list[tuple[int, float]] = [
    (1, 3.0),     # my_score advantage
    (2, -3.0),    # opp_score disadvantage
    (12, 0.5),    # having ball (zvýšeno — důležitější)
    (15, -1.5),   # carrier_dist_to_td (3× silnější signál pro postup k endzone)
    (8, -0.3),    # my injured (bad)
    (9, 0.3),     # opp injured (good)
    (34, 0.5),    # carrier_near_endzone (binary: ≤3 sq od endzone)
    (35, 2.5),    # stall_incentive (hold ball when leading/tied, turns remaining)
    (59, 0.1),    # carrier_can_score (MA+2 ≥ dist → can score this turn, reduced to avoid early scoring)
]


class LinearTrainer:
    """Trains weights for a linear value function using game logs."""

    def __init__(self, n_features: int = NUM_FEATURES, learning_rate: float = 0.01):
        self.weights = np.zeros(n_features, dtype=np.float64)
        self.lr = learning_rate

    @staticmethod
    def _get_reward(winner: str | None, perspective: str) -> float:
        if winner is None:
            return 0.0
        return 1.0 if winner == perspective else -1.0

    @staticmethod
    def _group_by_perspective(game_log: list[dict]) -> dict[str, list[dict]]:
        """Group state records by perspective (home/away independently)."""
        groups: dict[str, list[dict]] = {}
        for record in game_log:
            if record.get('type') != 'state':
                continue
            perspective = record.get('perspective', 'home')
            groups.setdefault(perspective, []).append(record)
        return groups

    @staticmethod
    def _compute_potential(features: np.ndarray, shaping_weights: list[tuple[int, float]]) -> float:
        """Compute potential Φ(s) from features using shaping weights."""
        potential = 0.0
        for idx, w in shaping_weights:
            if idx < len(features):
                potential += w * features[idx]
        return potential

    def train_monte_carlo(self, game_log: list[dict]) -> None:
        """Update weights using Monte Carlo: reward = final game result.

        Each state in the game gets reward based on outcome:
        +1 for win, -1 for loss, 0 for draw (from perspective of logged player).
        Update: w += lr * (reward - V(s)) * features(s)
        """
        # Find the result record
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break

        if result_record is None:
            return

        winner = result_record.get('winner')  # 'home', 'away', or None (draw)

        # Process each state record
        for record in game_log:
            if record.get('type') != 'state':
                continue

            features = self._align_features(np.array(record['features'], dtype=np.float64))
            perspective = record.get('perspective', 'home')
            reward = self._get_reward(winner, perspective)

            # Current value estimate
            v = float(np.dot(self.weights, features))

            # TD-like update: w += lr * (reward - V(s)) * features
            self.weights += self.lr * (reward - v) * features

    def train_monte_carlo_shaped(
        self,
        game_log: list[dict],
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """Update weights using MC with potential-based reward shaping.

        Shaped reward at state t: final_reward + γ·Φ(s_{t+1}) - Φ(s_t)
        For terminal state: final_reward - Φ(s_T)
        States are grouped by perspective (home/away independently).
        """
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break

        if result_record is None:
            return

        winner = result_record.get('winner')
        sw = shaping_weights if shaping_weights is not None else DEFAULT_SHAPING_WEIGHTS

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)

            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                phi_current = self._compute_potential(features, sw)

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    phi_next = self._compute_potential(next_features, sw)
                    shaped_reward = final_reward + gamma * phi_next - phi_current
                else:
                    # Terminal state
                    shaped_reward = final_reward - phi_current

                v = float(np.dot(self.weights, features))
                self.weights += self.lr * (shaped_reward - v) * features

    def train_td0(self, game_log: list[dict], gamma: float = 0.99) -> None:
        """Update weights using TD(0) per perspective.

        States are grouped by perspective (home/away independently) to avoid
        bootstrapping across alternating home/away states.
        """
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break

        if result_record is None:
            return

        winner = result_record.get('winner')

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)

            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                v = float(np.dot(self.weights, features))

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next = float(np.dot(self.weights, next_features))
                    td_target = 0.0 + gamma * v_next
                else:
                    td_target = final_reward

                td_error = td_target - v
                self.weights += self.lr * td_error * features

    def train_td_lambda(
        self,
        game_log: list[dict],
        gamma: float = 0.99,
        lambda_: float = 0.8,
    ) -> None:
        """Update weights using TD(λ) with eligibility traces.

        States are grouped by perspective (home/away independently).
        Each perspective gets its own eligibility trace.
        """
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break

        if result_record is None:
            return

        winner = result_record.get('winner')

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)
            e = np.zeros_like(self.weights)  # eligibility trace

            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                v = float(np.dot(self.weights, features))

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next = float(np.dot(self.weights, next_features))
                    td_error = 0.0 + gamma * v_next - v
                else:
                    td_error = final_reward - v

                e = gamma * lambda_ * e + features
                self.weights += self.lr * td_error * e

    def evaluate(self, features: list[float]) -> float:
        """Compute w . features (dot product)."""
        f = self._align_features(np.array(features, dtype=np.float64))
        return float(np.dot(self.weights, f))

    def _align_features(self, features: np.ndarray) -> np.ndarray:
        """Pad or truncate features to match weight vector length."""
        n = len(self.weights)
        if len(features) == n:
            return features
        if len(features) < n:
            padded = np.zeros(n, dtype=np.float64)
            padded[:len(features)] = features
            return padded
        return features[:n]

    def save_weights(self, path: str) -> None:
        """Save weights as JSON array (compatible with PHP LearningAICoach)."""
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'w') as f:
            json.dump(self.weights.tolist(), f)

    def load_weights(self, path: str) -> None:
        """Load weights from JSON array, padding with zeros if feature count increased."""
        with open(path) as f:
            data = json.load(f)
        loaded = np.array(data, dtype=np.float64)
        if len(loaded) < len(self.weights):
            # Pad with zeros for new features
            padded = np.zeros(len(self.weights), dtype=np.float64)
            padded[:len(loaded)] = loaded
            self.weights = padded
        else:
            self.weights = loaded[:len(self.weights)]


class NeuralTrainer:
    """Trains a 2-layer neural network value function using game logs.

    Architecture: n_features -> hidden_size (ReLU) -> 1 (tanh)
    """

    def __init__(
        self,
        n_features: int = NUM_FEATURES,
        hidden_size: int = 32,
        learning_rate: float = 0.01,
    ):
        self.n_features = n_features
        self.hidden_size = hidden_size
        self.lr = learning_rate

        # Xavier initialization
        limit1 = np.sqrt(6.0 / (n_features + hidden_size))
        self.W1 = np.random.uniform(-limit1, limit1, (n_features, hidden_size))
        limit2 = np.sqrt(6.0 / (hidden_size + 1))
        self.W2 = np.random.uniform(-limit2, limit2, (hidden_size, 1))
        self.b1 = np.zeros(hidden_size)
        self.b2 = np.zeros(1)

    @staticmethod
    def _get_reward(winner: str | None, perspective: str) -> float:
        if winner is None:
            return 0.0
        return 1.0 if winner == perspective else -1.0

    @staticmethod
    def _group_by_perspective(game_log: list[dict]) -> dict[str, list[dict]]:
        groups: dict[str, list[dict]] = {}
        for record in game_log:
            if record.get('type') != 'state':
                continue
            perspective = record.get('perspective', 'home')
            groups.setdefault(perspective, []).append(record)
        return groups

    @staticmethod
    def _compute_potential(features: np.ndarray, shaping_weights: list[tuple[int, float]]) -> float:
        potential = 0.0
        for idx, w in shaping_weights:
            if idx < len(features):
                potential += w * features[idx]
        return potential

    def forward(self, features: np.ndarray) -> tuple[float, np.ndarray]:
        """Forward pass. Returns (output_value, hidden_activations)."""
        features = self._align_features(features)
        h = features @ self.W1 + self.b1  # (hidden_size,)
        h = np.maximum(h, 0.0)  # ReLU
        out = h @ self.W2 + self.b2  # (1,)
        out = np.tanh(out)  # tanh
        return float(out[0]), h

    def evaluate(self, features: list[float]) -> float:
        """Compute value for a feature vector."""
        f = np.array(features, dtype=np.float64)
        value, _ = self.forward(f)
        return value

    def _backprop(self, features: np.ndarray, target: float) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        """Compute gradients for a single sample.

        Returns (dW1, db1, dW2, db2).
        """
        features = self._align_features(features)
        # Forward
        z1 = features @ self.W1 + self.b1
        h = np.maximum(z1, 0.0)  # ReLU
        z2 = h @ self.W2 + self.b2
        y = np.tanh(z2)  # (1,)

        # Backward: loss = 0.5 * (target - y)^2
        # dL/dy = -(target - y)
        # dy/dz2 = 1 - y^2 (tanh derivative)
        dz2 = -(target - y[0]) * (1.0 - y[0] ** 2)  # scalar

        dW2 = h.reshape(-1, 1) * dz2  # (hidden_size, 1)
        db2 = np.array([dz2])  # (1,)

        dh = self.W2.flatten() * dz2  # (hidden_size,)
        dh = dh * (z1 > 0).astype(np.float64)  # ReLU derivative

        dW1 = features.reshape(-1, 1) @ dh.reshape(1, -1)  # (n_features, hidden_size)
        db1 = dh  # (hidden_size,)

        return dW1, db1, dW2, db2

    def _update(self, dW1: np.ndarray, db1: np.ndarray, dW2: np.ndarray, db2: np.ndarray) -> None:
        """Apply gradient updates."""
        self.W1 -= self.lr * dW1
        self.b1 -= self.lr * db1
        self.W2 -= self.lr * dW2
        self.b2 -= self.lr * db2

    def train_monte_carlo(self, game_log: list[dict]) -> None:
        """Update weights using Monte Carlo."""
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        winner = result_record.get('winner')

        for record in game_log:
            if record.get('type') != 'state':
                continue
            features = self._align_features(np.array(record['features'], dtype=np.float64))
            perspective = record.get('perspective', 'home')
            reward = self._get_reward(winner, perspective)
            dW1, db1, dW2, db2 = self._backprop(features, reward)
            self._update(dW1, db1, dW2, db2)

    def train_monte_carlo_shaped(
        self,
        game_log: list[dict],
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """Update weights using MC with potential-based reward shaping."""
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        winner = result_record.get('winner')
        sw = shaping_weights if shaping_weights is not None else DEFAULT_SHAPING_WEIGHTS

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                phi_current = self._compute_potential(features, sw)

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    phi_next = self._compute_potential(next_features, sw)
                    shaped_reward = final_reward + gamma * phi_next - phi_current
                else:
                    shaped_reward = final_reward - phi_current

                dW1, db1, dW2, db2 = self._backprop(features, shaped_reward)
                self._update(dW1, db1, dW2, db2)

    def train_td0(self, game_log: list[dict], gamma: float = 0.99) -> None:
        """Update weights using TD(0) per perspective."""
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        winner = result_record.get('winner')
        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                v, _ = self.forward(features)

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next, _ = self.forward(next_features)
                    td_target = gamma * v_next
                else:
                    td_target = final_reward

                # Semi-gradient: backprop toward td_target
                dW1, db1, dW2, db2 = self._backprop(features, td_target)
                self._update(dW1, db1, dW2, db2)

    def train_td_lambda(
        self,
        game_log: list[dict],
        gamma: float = 0.99,
        lambda_: float = 0.8,
    ) -> None:
        """Update weights using TD(λ) with eligibility traces per perspective."""
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        winner = result_record.get('winner')
        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(winner, perspective)

            # Eligibility traces for each parameter
            eW1 = np.zeros_like(self.W1)
            eb1 = np.zeros_like(self.b1)
            eW2 = np.zeros_like(self.W2)
            eb2 = np.zeros_like(self.b2)

            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                v, h = self.forward(features)

                if i < len(states) - 1:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next, _ = self.forward(next_features)
                    td_error = gamma * v_next - v
                else:
                    td_error = final_reward - v

                # Compute gradient of V w.r.t. parameters (value gradient, not loss gradient)
                # dV/dW2 = h * (1 - v^2)  [tanh derivative]
                tanh_deriv = 1.0 - v ** 2
                gW2 = h.reshape(-1, 1) * tanh_deriv
                gb2 = np.array([tanh_deriv])

                # dV/dh = W2 * (1 - v^2)
                dh = self.W2.flatten() * tanh_deriv
                # dh/dz1 = ReLU'(z1)
                z1 = features @ self.W1 + self.b1
                dh = dh * (z1 > 0).astype(np.float64)
                gW1 = features.reshape(-1, 1) @ dh.reshape(1, -1)
                gb1 = dh

                # Update eligibility traces
                eW1 = gamma * lambda_ * eW1 + gW1
                eb1 = gamma * lambda_ * eb1 + gb1
                eW2 = gamma * lambda_ * eW2 + gW2
                eb2 = gamma * lambda_ * eb2 + gb2

                # Update weights: w += lr * td_error * e
                self.W1 += self.lr * td_error * eW1
                self.b1 += self.lr * td_error * eb1
                self.W2 += self.lr * td_error * eW2
                self.b2 += self.lr * td_error * eb2

    def _align_features(self, features: np.ndarray) -> np.ndarray:
        """Pad or truncate features to match expected input size."""
        n = self.n_features
        if len(features) == n:
            return features
        if len(features) < n:
            padded = np.zeros(n, dtype=np.float64)
            padded[:len(features)] = features
            return padded
        return features[:n]

    def save_weights(self, path: str) -> None:
        """Save neural network weights as JSON with type marker."""
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        data = {
            'type': 'neural',
            'hidden_size': self.hidden_size,
            'n_features': self.n_features,
            'W1': self.W1.tolist(),
            'b1': self.b1.tolist(),
            'W2': self.W2.tolist(),
            'b2': self.b2.tolist(),
        }
        with open(path, 'w') as f:
            json.dump(data, f)

    def load_weights(self, path: str) -> None:
        """Load neural network weights from JSON."""
        with open(path) as f:
            data = json.load(f)
        if isinstance(data, dict) and data.get('type') == 'neural':
            self.W1 = np.array(data['W1'], dtype=np.float64)
            self.b1 = np.array(data['b1'], dtype=np.float64)
            self.W2 = np.array(data['W2'], dtype=np.float64)
            self.b2 = np.array(data['b2'], dtype=np.float64)
            self.hidden_size = data.get('hidden_size', self.W1.shape[1])
            self.n_features = data.get('n_features', self.W1.shape[0])
        else:
            # Legacy flat array — convert to linear-compatible init
            loaded = np.array(data, dtype=np.float64)
            raise ValueError(
                f'Cannot load linear weights into NeuralTrainer. '
                f'Use LinearTrainer or create_trainer() with auto-detection.'
            )


def load_trainer(path: str, learning_rate: float = 0.01) -> Union[LinearTrainer, NeuralTrainer]:
    """Auto-detect weight format and return the appropriate trainer."""
    with open(path) as f:
        data = json.load(f)

    if isinstance(data, dict) and data.get('type') == 'neural':
        n_features = data.get('n_features', len(data['W1']))
        hidden_size = data.get('hidden_size', len(data['b1']))
        trainer = NeuralTrainer(
            n_features=n_features,
            hidden_size=hidden_size,
            learning_rate=learning_rate,
        )
        trainer.W1 = np.array(data['W1'], dtype=np.float64)
        trainer.b1 = np.array(data['b1'], dtype=np.float64)
        trainer.W2 = np.array(data['W2'], dtype=np.float64)
        trainer.b2 = np.array(data['b2'], dtype=np.float64)
        return trainer
    else:
        trainer = LinearTrainer(learning_rate=learning_rate)
        trainer.weights = np.array(data, dtype=np.float64)
        if len(trainer.weights) < NUM_FEATURES:
            padded = np.zeros(NUM_FEATURES, dtype=np.float64)
            padded[:len(trainer.weights)] = trainer.weights
            trainer.weights = padded
        return trainer


def create_trainer(
    model_type: str = 'linear',
    n_features: int = NUM_FEATURES,
    hidden_size: int = 32,
    learning_rate: float = 0.01,
) -> Union[LinearTrainer, NeuralTrainer]:
    """Factory function to create the appropriate trainer."""
    if model_type == 'neural':
        return NeuralTrainer(
            n_features=n_features,
            hidden_size=hidden_size,
            learning_rate=learning_rate,
        )
    return LinearTrainer(n_features=n_features, learning_rate=learning_rate)
