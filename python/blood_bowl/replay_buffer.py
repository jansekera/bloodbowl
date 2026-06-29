"""Experience replay buffer for RL training."""
from __future__ import annotations

import pickle
import random
from collections import deque
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from .rewards import episode_returns, terminal_value


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

        # Disconnect A fix (break-the-draw 2026-06-26): use the score-based
        # terminal_value SSOT instead of a winner-only +1/0/-1 (which scored every
        # draw as cost-free 0.0 and bypassed the score-aware reward entirely). Now
        # the replay path — dominant under MCTS — carries the same win>>draw>>loss,
        # TD-in-loss signal as the full-log path.
        home_score = result_record.get('home_score', 0)
        away_score = result_record.get('away_score', 0)

        # Group by perspective
        groups: dict = {}
        for record in game_log:
            if record.get('type') != 'state':
                continue
            perspective = record.get('perspective', 'home')
            groups.setdefault(perspective, []).append(record)

        for perspective, states in groups.items():
            reward = terminal_value(home_score, away_score, perspective)
            n_states = len(states)

            # Lever B (break-the-draw): fold a per-TD step reward into the MC
            # return so mid-game states leading up to a TD get a positive
            # discounted target (the missing "carry -> TD" pull). Needs the
            # running score at each logged state (home_score/away_score, added to
            # the state log alongside features). Old logs lack it -> fall back to
            # the terminal-only return G_t = gamma^(T-t) * final_reward, which is
            # exactly what episode_returns yields when no TD is scored.
            has_scores = all('home_score' in s for s in states)
            if has_scores:
                my_scores = [s['away_score'] if perspective == 'away'
                             else s['home_score'] for s in states]
                opp_scores = [s['home_score'] if perspective == 'away'
                              else s['away_score'] for s in states]
                returns = episode_returns(my_scores, opp_scores, reward, gamma)
            else:
                returns = [(gamma ** ((n_states - 1) - i)) * reward
                           for i in range(n_states)]

            for i, record in enumerate(states):
                features = record['features']
                is_terminal = (i == n_states - 1)
                next_features = states[i + 1]['features'] if not is_terminal else features

                self.buffer.append(Transition(
                    features=features,
                    reward=reward,
                    next_features=next_features,
                    perspective=perspective,
                    is_terminal=is_terminal,
                    mc_return=returns[i],
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
