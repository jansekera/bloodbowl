"""Tests for ReplayBuffer."""
from __future__ import annotations

import tempfile
from pathlib import Path

import pytest

from blood_bowl.replay_buffer import ReplayBuffer, Transition


def _make_game_log(n_features: int = 5, winner: str = 'home') -> list:
    return [
        {'type': 'state', 'features': [0.5] * n_features, 'perspective': 'home'},
        {'type': 'state', 'features': [0.3] * n_features, 'perspective': 'away'},
        {'type': 'state', 'features': [0.6] * n_features, 'perspective': 'home'},
        {'type': 'state', 'features': [0.4] * n_features, 'perspective': 'away'},
        {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': winner},
    ]


class TestReplayBuffer:
    def test_add_game_extracts_transitions(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_make_game_log())
        # 2 home states + 2 away states = 4 transitions
        assert len(buf) == 4

    def test_transition_rewards(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_make_game_log(winner='home'))
        transitions = list(buf.buffer)
        home_transitions = [t for t in transitions if t.perspective == 'home']
        away_transitions = [t for t in transitions if t.perspective == 'away']
        assert all(t.reward == 1.0 for t in home_transitions)
        assert all(t.reward == -1.0 for t in away_transitions)

    def test_transition_terminal_flag(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_make_game_log())
        transitions = list(buf.buffer)
        home_transitions = [t for t in transitions if t.perspective == 'home']
        # Last home state should be terminal
        assert not home_transitions[0].is_terminal
        assert home_transitions[-1].is_terminal

    def test_draw_reward(self):
        buf = ReplayBuffer(capacity=100)
        game_log = [
            {'type': 'state', 'features': [0.5, 0.5], 'perspective': 'home'},
            {'type': 'result', 'home_score': 1, 'away_score': 1, 'winner': None},
        ]
        buf.add_game(game_log)
        assert len(buf) == 1
        assert buf.buffer[0].reward == 0.0

    def test_sample_returns_correct_count(self):
        buf = ReplayBuffer(capacity=100)
        for _ in range(5):
            buf.add_game(_make_game_log())
        assert len(buf) == 20
        sample = buf.sample(batch_size=10)
        assert len(sample) == 10

    def test_sample_clamps_to_buffer_size(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_make_game_log())
        sample = buf.sample(batch_size=100)
        assert len(sample) == 4

    def test_capacity_limit(self):
        buf = ReplayBuffer(capacity=5)
        for _ in range(10):
            buf.add_game(_make_game_log())
        assert len(buf) == 5

    def test_save_load_roundtrip(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_make_game_log())
        original_len = len(buf)

        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / 'buffer.pkl')
            buf.save(path)

            buf2 = ReplayBuffer(capacity=100)
            buf2.load(path)
            assert len(buf2) == original_len
            assert buf2.buffer[0].features == buf.buffer[0].features

    def test_no_result_skips(self):
        buf = ReplayBuffer(capacity=100)
        game_log = [
            {'type': 'state', 'features': [0.5], 'perspective': 'home'},
        ]
        buf.add_game(game_log)
        assert len(buf) == 0

    def test_empty_buffer_sample(self):
        buf = ReplayBuffer(capacity=100)
        sample = buf.sample(batch_size=10)
        assert len(sample) == 0
