"""Experience replay buffer for RL training."""
from __future__ import annotations

import pickle
import random
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


@dataclass
class Transition:
    """A single state transition.

    `reward` is the broadcast final game outcome (+1/-1/0) for the state's
    perspective. `mc_return` is the TRUE discounted Monte-Carlo return
    G_t = gamma^(T-t) * final_reward, precomputed at add_game() time where the
    full per-perspective ordering is available (replay sampling loses t/T).
    Defaults to None for buffers pickled before this field existed; consumers
    fall back to the one-step shaped target in that case.
    """
    features: list
    reward: float
    next_features: list
    perspective: str
    is_terminal: bool
    mc_return: Optional[float] = None


class ReplayBuffer:
    """Circular buffer storing game transitions for experience replay."""

    def __init__(self, capacity: int = 50000):
        self.buffer: deque = deque(maxlen=capacity)
        self.capacity = capacity

    def add_game(self, game_log: list, gamma: float = 0.99) -> None:
        """Extract transitions from a game log and add them to the buffer.

        `gamma` is used to precompute the discounted MC return
        G_t = gamma^(T-t) * final_reward per transition (stored as mc_return),
        since the replay buffer otherwise loses the per-game time index needed
        for it. Defaults to the trainer's gamma (0.99)."""
        result_record = None
        for record in reversed(game_log):
            if record.get('type') == 'result':
                result_record = record
                break

        if result_record is None:
            return

        winner = result_record.get('winner')

        # Group by perspective
        groups: dict = {}
        for record in game_log:
            if record.get('type') != 'state':
                continue
            perspective = record.get('perspective', 'home')
            groups.setdefault(perspective, []).append(record)

        for perspective, states in groups.items():
            if winner is None:
                reward = 0.0
            elif winner == perspective:
                reward = 1.0
            else:
                reward = -1.0

            n_states = len(states)
            for i, record in enumerate(states):
                features = record['features']
                is_terminal = (i == n_states - 1)
                next_features = states[i + 1]['features'] if not is_terminal else features
                # True discounted MC return: G_t = gamma^(T-t) * final_reward.
                steps_to_end = (n_states - 1) - i
                mc_return = (gamma ** steps_to_end) * reward

                self.buffer.append(Transition(
                    features=features,
                    reward=reward,
                    next_features=next_features,
                    perspective=perspective,
                    is_terminal=is_terminal,
                    mc_return=mc_return,
                ))

    def sample(self, batch_size: int = 64) -> List[Transition]:
        """Sample random transitions from the buffer."""
        n = min(batch_size, len(self.buffer))
        return random.sample(list(self.buffer), n)

    def save(self, path: str) -> None:
        """Save buffer to a pickle file."""
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'wb') as f:
            pickle.dump(list(self.buffer), f)

    def load(self, path: str) -> None:
        """Load buffer from a pickle file."""
        with open(path, 'rb') as f:
            data = pickle.load(f)
        self.buffer = deque(data, maxlen=self.capacity)

    def __len__(self) -> int:
        return len(self.buffer)
