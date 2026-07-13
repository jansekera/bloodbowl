"""Value function trainers for Blood Bowl RL (linear + neural)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Union

import numpy as np

from .features import NUM_FEATURES
from .rewards import episode_returns, episode_step_rewards, terminal_value

# Default shaping weights: (feature_index, weight)
DEFAULT_SHAPING_WEIGHTS: list[tuple[int, float]] = [
    (1, 3.0),     # my_score advantage
    (2, -3.0),    # opp_score disadvantage
    (12, 0.5),    # having ball
    (14, -0.8),   # ball_on_ground: penalizace za míč na zemi, pickup = +0.8 bonus
    (67, 0.8),    # loose_ball_proximity: incentiva přibližovat se k míči
    (15, -1.5),   # carrier_dist_to_td (3× silnější signál pro postup k endzone)
    (8, -0.3),    # my injured (bad)
    (9, 0.3),     # opp injured (good)
    (34, 0.5),    # carrier_near_endzone (binary: ≤3 sq od endzone)
    (35, -0.1),   # stall_incentive — negativní: penalizuje pasivní čekání, urgency konzistentní s carrier_dist_to_td
    (59, 0.8),    # carrier_can_score — vráceno na 0.8 (bylo při rekordu 96.7%; sníženo na 0.6
                  # jako kompromis pro 2:1 grind, ale grind je odložen)
    (42, -0.8),   # opp_scoring_threat — soupeřův nosič blízko endzone (Team 2)
    (40, -0.6),   # carrier_tz_count — nosič v tackle zones = nebezpečí (Team 2)
    (63, -0.4),   # carrier_blitzable — nosič je zasažitelný blitzem (Team 2)
    (70, -0.5),   # fix #3: loose_ball_dist_to_td — míč blíž mojí endzóně = lepší šance skórovat
    (71, -0.3),   # fix #3: my_nearest_to_ball — můj hráč blíž volnému míči = lepší
    (72, 0.3),    # fix #3: pickup_clear — čistý (nehlídaný) pickup = dobrý
]


class LinearTrainer:
    """Trains weights for a linear value function using game logs."""

    def __init__(self, n_features: int = NUM_FEATURES, learning_rate: float = 0.01):
        self.weights = np.zeros(n_features, dtype=np.float64)
        self.lr = learning_rate

    @staticmethod
    def _get_reward(result_record: dict | None, perspective: str) -> float:
        # Terminal reward SSOT (break-the-draw 2026-06-26): win >> draw >> loss,
        # and a TD has value even in a loss. See rewards.terminal_value.
        home = result_record.get('home_score', 0) if result_record else 0
        away = result_record.get('away_score', 0) if result_record else 0
        return terminal_value(home, away, perspective)

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
            reward = self._get_reward(result_record, perspective)

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
            final_reward = self._get_reward(result_record, perspective)

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

    def train_transition_shaped(
        self,
        features: list,
        next_features: list,
        final_reward: float,
        is_terminal: bool,
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """One potential-shaped MC update for a single replay transition.

        Same target as train_monte_carlo_shaped for this state; next_features is
        only the Φ(s') lookahead, never trained as a spurious terminal state.
        Pass shaping_weights=[] for plain MC (Φ≡0 → target = final_reward)."""
        sw = shaping_weights if shaping_weights is not None else DEFAULT_SHAPING_WEIGHTS
        f = self._align_features(np.array(features, dtype=np.float64))
        phi_current = self._compute_potential(f, sw)
        if is_terminal:
            shaped_reward = final_reward - phi_current
        else:
            nf = self._align_features(np.array(next_features, dtype=np.float64))
            shaped_reward = final_reward + gamma * self._compute_potential(nf, sw) - phi_current
        v = float(np.dot(self.weights, f))
        self.weights += self.lr * (shaped_reward - v) * f

    def train_monte_carlo_return(self, game_log: list[dict], gamma: float = 0.99) -> None:
        """Update weights using the TRUE discounted MC return per state.

        Target at state t: G_t = γ^(T−t) · final_reward (graded credit toward the
        outcome), instead of the same ±1 broadcast to every state. States are
        grouped by perspective (home/away independently)."""
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
            final_reward = self._get_reward(result_record, perspective)
            n_states = len(states)
            # Lever B: fold the per-TD step reward into the return (mirrors
            # replay_buffer.add_game). Needs the running score per logged state;
            # old logs without it fall back to the terminal-only return, which is
            # exactly episode_returns with no TD scored.
            if all('home_score' in s for s in states):
                my_scores = [s['away_score'] if perspective == 'away'
                             else s['home_score'] for s in states]
                opp_scores = [s['home_score'] if perspective == 'away'
                              else s['away_score'] for s in states]
                returns = episode_returns(my_scores, opp_scores, final_reward, gamma)
            else:
                returns = [(gamma ** ((n_states - 1) - i)) * final_reward
                           for i in range(n_states)]
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                g_return = returns[i]
                v = float(np.dot(self.weights, features))
                self.weights += self.lr * (g_return - v) * features

    def train_transition_return(
        self,
        features: list,
        next_features: list,
        mc_return: float,
        is_terminal: bool,
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """One discounted-MC-return update for a single replay transition.

        Target: G_t [+ γΦ(s') − Φ(s)] where G_t is the precomputed discounted
        return (replay_buffer.Transition.mc_return). Pass shaping_weights=[]
        (default) for the pure return; pass DEFAULT_SHAPING_WEIGHTS to add
        potential-based shaping (PBRS) on top of the return."""
        sw = shaping_weights if shaping_weights is not None else []
        f = self._align_features(np.array(features, dtype=np.float64))
        target = mc_return
        if sw:
            phi_current = self._compute_potential(f, sw)
            if is_terminal:
                target += -phi_current
            else:
                nf = self._align_features(np.array(next_features, dtype=np.float64))
                target += gamma * self._compute_potential(nf, sw) - phi_current
        v = float(np.dot(self.weights, f))
        self.weights += self.lr * (target - v) * f

    def train_mc_td_mix(self, game_log: list[dict], gamma: float = 0.99,
                        alpha: float = 0.7) -> None:
        """Update weights using the MC/TD-bootstrap mixed value target.

        Target at non-terminal state t:
            clamp(alpha*G_t + (1-alpha)*(r_t + gamma*V(s_{t+1})), -1, 1)
        with G_t/r_t from rewards.episode_returns/episode_step_rewards and
        V(s_{t+1}) evaluated with the CURRENT weights at update time. The
        terminal state keeps the plain terminal_value anchor (== G_T). Both
        halves estimate the same V^pi, so the target is policy-invariant by
        construction — no new reward terms, no potential. alpha=1.0 reduces
        bit-exactly to train_monte_carlo_return (built-in null test); alpha<1
        trades MC variance for within-game TD signal (the flat-value-target
        root cause, research_fable_20260709.md section 7).
        """
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(result_record, perspective)
            n_states = len(states)
            # Same G_t as train_monte_carlo_return (Lever B fold-in; old logs
            # without per-state scores fall back to the terminal-only return,
            # with r_t = 0 to match: no scores recorded means no TD terms).
            if all('home_score' in s for s in states):
                my_scores = [s['away_score'] if perspective == 'away'
                             else s['home_score'] for s in states]
                opp_scores = [s['home_score'] if perspective == 'away'
                              else s['away_score'] for s in states]
                returns = episode_returns(my_scores, opp_scores, final_reward, gamma)
                step_rewards = episode_step_rewards(my_scores, opp_scores)
            else:
                returns = [(gamma ** ((n_states - 1) - i)) * final_reward
                           for i in range(n_states)]
                step_rewards = [0.0] * n_states
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                if i == n_states - 1:
                    target = returns[i]  # terminal anchor, never bootstrapped
                else:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next = float(np.dot(self.weights, next_features))
                    target = alpha * returns[i] \
                        + (1.0 - alpha) * (step_rewards[i] + gamma * v_next)
                    target = max(-1.0, min(1.0, target))
                v = float(np.dot(self.weights, features))
                self.weights += self.lr * (target - v) * features

    def train_transition_td_mix(
        self,
        features: list,
        next_features: list,
        mc_return: float,
        reward_step: float,
        is_terminal: bool,
        gamma: float = 0.99,
        alpha: float = 0.7,
    ) -> None:
        """One mixed MC/TD-bootstrap update for a single replay transition.

        Same target as train_mc_td_mix for this state. V(s') is computed from
        the trainer's CURRENT weights at call time — the buffer stores only
        r_t (Transition.reward_step), never a value, so the bootstrap can't
        go stale. alpha=1.0 reduces bit-exactly to train_transition_return
        with no shaping (mc_return is already clamped at buffer-write time).
        """
        f = self._align_features(np.array(features, dtype=np.float64))
        if is_terminal:
            target = mc_return  # terminal anchor, never bootstrapped
        else:
            nf = self._align_features(np.array(next_features, dtype=np.float64))
            v_next = float(np.dot(self.weights, nf))
            target = alpha * mc_return + (1.0 - alpha) * (reward_step + gamma * v_next)
            target = max(-1.0, min(1.0, target))
        v = float(np.dot(self.weights, f))
        self.weights += self.lr * (target - v) * f

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
            final_reward = self._get_reward(result_record, perspective)

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
            final_reward = self._get_reward(result_record, perspective)
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
        self._grad_norm_sum = 0.0
        self._grad_norm_count = 0

    @staticmethod
    def _get_reward(result_record: dict | None, perspective: str) -> float:
        # Terminal reward SSOT (break-the-draw 2026-06-26): win >> draw >> loss,
        # and a TD has value even in a loss. See rewards.terminal_value.
        home = result_record.get('home_score', 0) if result_record else 0
        away = result_record.get('away_score', 0) if result_record else 0
        return terminal_value(home, away, perspective)

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
        """Compute potential Φ(s) from features using shaping weights."""
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
        self._grad_norm_sum += float(np.linalg.norm(dW1) + np.linalg.norm(dW2))
        self._grad_norm_count += 1

    def pop_mean_grad_norm(self) -> float:
        """Return mean gradient norm since last call, then reset accumulator."""
        if self._grad_norm_count == 0:
            return 0.0
        result = self._grad_norm_sum / self._grad_norm_count
        self._grad_norm_sum = 0.0
        self._grad_norm_count = 0
        return result

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
            reward = self._get_reward(result_record, perspective)
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
            final_reward = self._get_reward(result_record, perspective)
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

    def train_transition_shaped(
        self,
        features: list,
        next_features: list,
        final_reward: float,
        is_terminal: bool,
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """One potential-shaped MC update for a single replay transition.

        Identical target to what train_monte_carlo_shaped computes for this
        state in the full game log — next_features is used ONLY as the Φ(s')
        lookahead, never trained as its own (spurious terminal) state. Pass
        shaping_weights=[] for plain MC (Φ≡0 → target = final_reward)."""
        sw = shaping_weights if shaping_weights is not None else DEFAULT_SHAPING_WEIGHTS
        f = self._align_features(np.array(features, dtype=np.float64))
        phi_current = self._compute_potential(f, sw)
        if is_terminal:
            shaped_reward = final_reward - phi_current
        else:
            nf = self._align_features(np.array(next_features, dtype=np.float64))
            shaped_reward = final_reward + gamma * self._compute_potential(nf, sw) - phi_current
        dW1, db1, dW2, db2 = self._backprop(f, shaped_reward)
        self._update(dW1, db1, dW2, db2)

    def train_monte_carlo_return(self, game_log: list[dict], gamma: float = 0.99) -> None:
        """Update weights using the TRUE discounted MC return per state.

        Target at state t: G_t per rewards.episode_returns -- the discounted
        return with the Lever-B per-TD step reward folded in, the same definition
        LinearTrainer.train_monte_carlo_return and replay_buffer.add_game use.
        Old logs without per-state scores fall back to γ^(T−t)·final_reward."""
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
            final_reward = self._get_reward(result_record, perspective)
            n_states = len(states)
            # Lever B: fold the per-TD step reward into the return (mirrors
            # replay_buffer.add_game). This class was missed when Lever B landed
            # in 6fb5b9b -- it went into LinearTrainer and the replay buffer but
            # not here, so the neural head (the one actually in production) was
            # trained on a pure terminal broadcast from the full-log path while
            # the replay path fed it the folded-in return: two different targets
            # for the same game. Needs the running score per logged state; old
            # logs without it fall back to the terminal-only return, which is
            # exactly episode_returns with no TD scored.
            if all('home_score' in s for s in states):
                my_scores = [s['away_score'] if perspective == 'away'
                             else s['home_score'] for s in states]
                opp_scores = [s['home_score'] if perspective == 'away'
                              else s['away_score'] for s in states]
                returns = episode_returns(my_scores, opp_scores, final_reward, gamma)
            else:
                returns = [(gamma ** ((n_states - 1) - i)) * final_reward
                           for i in range(n_states)]
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                g_return = returns[i]
                dW1, db1, dW2, db2 = self._backprop(features, g_return)
                self._update(dW1, db1, dW2, db2)

    def train_transition_return(
        self,
        features: list,
        next_features: list,
        mc_return: float,
        is_terminal: bool,
        gamma: float = 0.99,
        shaping_weights: list[tuple[int, float]] | None = None,
    ) -> None:
        """One discounted-MC-return update for a single replay transition.

        Target: G_t [+ γΦ(s') − Φ(s)]. Pass shaping_weights=[] (default) for the
        pure return; pass DEFAULT_SHAPING_WEIGHTS to add PBRS on top."""
        sw = shaping_weights if shaping_weights is not None else []
        f = self._align_features(np.array(features, dtype=np.float64))
        target = mc_return
        if sw:
            phi_current = self._compute_potential(f, sw)
            if is_terminal:
                target += -phi_current
            else:
                nf = self._align_features(np.array(next_features, dtype=np.float64))
                target += gamma * self._compute_potential(nf, sw) - phi_current
        dW1, db1, dW2, db2 = self._backprop(f, target)
        self._update(dW1, db1, dW2, db2)

    def train_mc_td_mix(self, game_log: list[dict], gamma: float = 0.99,
                        alpha: float = 0.7) -> None:
        """Update weights using the MC/TD-bootstrap mixed value target.

        Same target as LinearTrainer.train_mc_td_mix:
            clamp(alpha*G_t + (1-alpha)*(r_t + gamma*V(s_{t+1})), -1, 1)
        with V(s_{t+1}) a semi-gradient bootstrap from the CURRENT weights at
        update time (never cached). Terminal state keeps the terminal_value
        anchor. alpha=1.0 reduces bit-exactly to train_monte_carlo_return.
        """
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break
        if result_record is None:
            return

        groups = self._group_by_perspective(game_log)

        for perspective, states in groups.items():
            final_reward = self._get_reward(result_record, perspective)
            n_states = len(states)
            # Same G_t as train_monte_carlo_return (Lever B fold-in; old logs
            # without per-state scores fall back to the terminal-only return,
            # with r_t = 0 to match: no scores recorded means no TD terms).
            if all('home_score' in s for s in states):
                my_scores = [s['away_score'] if perspective == 'away'
                             else s['home_score'] for s in states]
                opp_scores = [s['home_score'] if perspective == 'away'
                              else s['away_score'] for s in states]
                returns = episode_returns(my_scores, opp_scores, final_reward, gamma)
                step_rewards = episode_step_rewards(my_scores, opp_scores)
            else:
                returns = [(gamma ** ((n_states - 1) - i)) * final_reward
                           for i in range(n_states)]
                step_rewards = [0.0] * n_states
            for i, record in enumerate(states):
                features = self._align_features(np.array(record['features'], dtype=np.float64))
                if i == n_states - 1:
                    target = returns[i]  # terminal anchor, never bootstrapped
                else:
                    next_features = self._align_features(
                        np.array(states[i + 1]['features'], dtype=np.float64)
                    )
                    v_next, _ = self.forward(next_features)
                    target = alpha * returns[i] \
                        + (1.0 - alpha) * (step_rewards[i] + gamma * v_next)
                    target = max(-1.0, min(1.0, target))
                dW1, db1, dW2, db2 = self._backprop(features, target)
                self._update(dW1, db1, dW2, db2)

    def train_transition_td_mix(
        self,
        features: list,
        next_features: list,
        mc_return: float,
        reward_step: float,
        is_terminal: bool,
        gamma: float = 0.99,
        alpha: float = 0.7,
    ) -> None:
        """One mixed MC/TD-bootstrap update for a single replay transition.

        Same target as train_mc_td_mix for this state. V(s') is computed from
        the trainer's CURRENT weights at call time — the buffer stores only
        r_t (Transition.reward_step), never a value, so the bootstrap can't
        go stale. alpha=1.0 reduces bit-exactly to train_transition_return
        with no shaping (mc_return is already clamped at buffer-write time).
        """
        f = self._align_features(np.array(features, dtype=np.float64))
        if is_terminal:
            target = mc_return  # terminal anchor, never bootstrapped
        else:
            nf = self._align_features(np.array(next_features, dtype=np.float64))
            v_next, _ = self.forward(nf)
            target = alpha * mc_return + (1.0 - alpha) * (reward_step + gamma * v_next)
            target = max(-1.0, min(1.0, target))
        dW1, db1, dW2, db2 = self._backprop(f, target)
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
            final_reward = self._get_reward(result_record, perspective)
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
            final_reward = self._get_reward(result_record, perspective)

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


def warm_start_expand(trainer: 'NeuralTrainer', new_hidden_size: int) -> 'NeuralTrainer':
    """Expand a NeuralTrainer to a larger hidden size, preserving existing weights.

    New neurons are initialized with small random weights so they don't disturb
    the model's current output. The existing neurons are copied exactly.
    """
    old_h = trainer.hidden_size
    assert new_hidden_size > old_h, f'new_hidden_size {new_hidden_size} must be > {old_h}'
    extra = new_hidden_size - old_h

    new_trainer = NeuralTrainer(
        n_features=trainer.n_features,
        hidden_size=new_hidden_size,
        learning_rate=trainer.lr,
    )

    # W1: (n_features, old_h) → (n_features, new_h)
    # New columns: small random (1% of Xavier scale) so new neurons start near-zero
    limit_extra = np.sqrt(6.0 / (trainer.n_features + new_hidden_size)) * 0.01
    W1_extra = np.random.uniform(-limit_extra, limit_extra, (trainer.n_features, extra))
    new_trainer.W1 = np.concatenate([trainer.W1, W1_extra], axis=1)

    # b1: copy + zeros
    new_trainer.b1 = np.concatenate([trainer.b1, np.zeros(extra)])

    # W2: (old_h, 1) → (new_h, 1)
    # New rows near-zero so new neurons don't affect output initially
    W2_extra = np.zeros((extra, 1))
    new_trainer.W2 = np.concatenate([trainer.W2, W2_extra], axis=0)

    # b2: unchanged
    new_trainer.b2 = trainer.b2.copy()

    return new_trainer


def _expand_value_W1(W1: np.ndarray, target_n_features: int) -> np.ndarray:
    """Warm-start expansion for added INPUT features (fix #3, 2026-06-24).
    Pads the value head's W1 with zero rows so the new features start at zero
    weight -> the loaded model's output is preserved EXACTLY, and the new
    features are learned from scratch during training. No-op if already sized."""
    if W1.shape[0] >= target_n_features:
        return W1
    pad = np.zeros((target_n_features - W1.shape[0], W1.shape[1]), dtype=np.float64)
    return np.vstack([W1, pad])


def load_trainer(path: str, learning_rate: float = 0.01) -> Union[LinearTrainer, NeuralTrainer]:
    """Auto-detect weight format and return the appropriate trainer."""
    with open(path) as f:
        data = json.load(f)

    if isinstance(data, dict) and data.get('type') == 'neural':
        n_features = data.get('n_features', len(data['W1']))
        hidden_size = data.get('hidden_size', len(data['b1']))
        trainer = NeuralTrainer(
            n_features=max(n_features, NUM_FEATURES),
            hidden_size=hidden_size,
            learning_rate=learning_rate,
        )
        trainer.W1 = _expand_value_W1(np.array(data['W1'], dtype=np.float64), trainer.n_features)
        trainer.b1 = np.array(data['b1'], dtype=np.float64)
        trainer.W2 = np.array(data['W2'], dtype=np.float64)
        trainer.b2 = np.array(data['b2'], dtype=np.float64)
        return trainer
    elif isinstance(data, dict) and data.get('type') == 'alphazero_neural':
        # AlphaZero neural format (saved by save_combined_weights with NeuralTrainer)
        n_features = data.get('n_features', len(data['value_W1']))
        hidden_size = data.get('hidden_size', len(data['value_b1']))
        trainer = NeuralTrainer(
            n_features=max(n_features, NUM_FEATURES),
            hidden_size=hidden_size,
            learning_rate=learning_rate,
        )
        trainer.W1 = _expand_value_W1(np.array(data['value_W1'], dtype=np.float64), trainer.n_features)
        trainer.b1 = np.array(data['value_b1'], dtype=np.float64)
        trainer.W2 = np.array(data['value_W2'], dtype=np.float64)
        trainer.b2 = np.array(data['value_b2'], dtype=np.float64)
        return trainer
    elif isinstance(data, dict) and data.get('type') == 'alphazero_linear':
        # AlphaZero linear format: plain value_weights array
        weights = data.get('value_weights', [0.0] * NUM_FEATURES)
        trainer = LinearTrainer(learning_rate=learning_rate)
        trainer.weights = np.array(weights, dtype=np.float64)
        if len(trainer.weights) < NUM_FEATURES:
            padded = np.zeros(NUM_FEATURES, dtype=np.float64)
            padded[:len(trainer.weights)] = trainer.weights
            trainer.weights = padded
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
