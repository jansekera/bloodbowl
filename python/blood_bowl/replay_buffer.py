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
    """A single state transition."""
    features: list
    reward: float
    next_features: list
    perspective: str
    is_terminal: bool


class ReplayBuffer:
    """Circular buffer storing game transitions for experience replay."""

    def __init__(self, capacity: int = 50000):
        self.buffer: deque = deque(maxlen=capacity)
        self.capacity = capacity

    def add_game(self, game_log: list) -> None:
        """Extract transitions from a game log and add them to the buffer."""
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

            for i, record in enumerate(states):
                features = record['features']
                is_terminal = (i == len(states) - 1)
                next_features = states[i + 1]['features'] if not is_terminal else features

                self.buffer.append(Transition(
                    features=features,
                    reward=reward,
                    next_features=next_features,
                    perspective=perspective,
                    is_terminal=is_terminal,
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
