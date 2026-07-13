"""Tests for the mc_td_mix MC/TD-bootstrap mixed value target.

Stage-0 contract (research_fable_20260709.md section 7): alpha=1.0 must
reduce BIT-EXACTLY to the existing mc_return method on all four paths
(Linear/Neural x full-log/transition) — the built-in null test for the A/B.
"""
from __future__ import annotations

import numpy as np
import pytest

from blood_bowl.evaluate import pre_td_value_ramp
from blood_bowl.replay_buffer import ReplayBuffer, Transition
from blood_bowl.rewards import episode_returns, episode_step_rewards
from blood_bowl.trainer import LinearTrainer, NeuralTrainer
from blood_bowl.training_loop import _train_on_log, _train_on_transition


@pytest.fixture(autouse=True)
def _pin_value_potential_beta(monkeypatch):
    # Stage-0 equivalence is only defined at beta=0.0 (the default of the
    # abandoned 2026-06-30 level-shaping experiment, which read this env);
    # pin it so a value inherited from the shell can't skew the comparison.
    monkeypatch.setenv('BB_VALUE_POTENTIAL_BETA', '0.0')


def _game_log_with_td() -> list[dict]:
    """Alternating-perspective log with per-state running scores; home scores
    a TD between its 2nd and 3rd state (exercises the Lever-B fold-in)."""
    return [
        {'type': 'state', 'features': [1.0, 0.0, 0.2, 0.0, 0.5], 'perspective': 'home',
         'home_score': 0, 'away_score': 0},
        {'type': 'state', 'features': [0.0, 1.0, 0.1, 0.3, 0.0], 'perspective': 'away',
         'home_score': 0, 'away_score': 0},
        {'type': 'state', 'features': [0.8, 0.1, 0.4, 0.0, 0.6], 'perspective': 'home',
         'home_score': 0, 'away_score': 0},
        {'type': 'state', 'features': [0.1, 0.9, 0.0, 0.5, 0.1], 'perspective': 'away',
         'home_score': 1, 'away_score': 0},
        {'type': 'state', 'features': [0.7, 0.2, 0.9, 0.0, 0.9], 'perspective': 'home',
         'home_score': 1, 'away_score': 0},
        {'type': 'state', 'features': [0.2, 0.8, 0.1, 0.6, 0.2], 'perspective': 'away',
         'home_score': 1, 'away_score': 0},
        {'type': 'result', 'home_score': 1, 'away_score': 0, 'winner': 'home'},
    ]


def _game_log_no_scores() -> list[dict]:
    """Old-format log without per-state scores (fallback branch)."""
    return [
        {'type': 'state', 'features': [1.0, 0.0, 0.2, 0.0, 0.5], 'perspective': 'home'},
        {'type': 'state', 'features': [0.0, 1.0, 0.1, 0.3, 0.0], 'perspective': 'away'},
        {'type': 'state', 'features': [0.8, 0.1, 0.4, 0.0, 0.6], 'perspective': 'home'},
        {'type': 'state', 'features': [0.2, 0.8, 0.1, 0.6, 0.2], 'perspective': 'away'},
        {'type': 'result', 'home_score': 2, 'away_score': 1, 'winner': 'home'},
    ]


def _linear(lr: float = 0.05) -> LinearTrainer:
    t = LinearTrainer(n_features=5, learning_rate=lr)
    t.weights = np.linspace(-0.4, 0.4, 5)  # nonzero so the bootstrap matters
    return t


def _twin_linear(lr: float = 0.05) -> tuple[LinearTrainer, LinearTrainer]:
    a, b = _linear(lr), _linear(lr)
    b.weights = a.weights.copy()
    return a, b


def _twin_neural(lr: float = 0.05) -> tuple[NeuralTrainer, NeuralTrainer]:
    a = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=lr)
    b = NeuralTrainer(n_features=5, hidden_size=4, learning_rate=lr)
    for attr in ('W1', 'b1', 'W2', 'b2'):
        setattr(b, attr, getattr(a, attr).copy())
    return a, b


def _neural_params(t: NeuralTrainer) -> list[np.ndarray]:
    return [t.W1, t.b1, t.W2, t.b2]


class TestEpisodeStepRewards:
    def test_consistent_with_episode_returns_fold_in(self):
        """G_t must equal clamp(r_t + gamma*G_{t+1}) with r_t from
        episode_step_rewards — the two-halves-one-V^pi contract."""
        my, opp = [0, 0, 1, 1, 2], [0, 1, 1, 1, 1]
        gamma = 0.99
        G = episode_returns(my, opp, term_val=1.0, gamma=gamma)
        r = episode_step_rewards(my, opp)
        for i in range(len(G) - 1):
            expected = max(-1.0, min(1.0, r[i] + gamma * G[i + 1]))
            assert G[i] == expected

    def test_no_td_all_zero(self):
        assert episode_step_rewards([0, 0, 0], [0, 0, 0]) == [0.0, 0.0, 0.0]

    def test_terminal_step_reward_zero(self):
        # r_T has no successor by definition, even right after a TD.
        r = episode_step_rewards([0, 1], [0, 0])
        assert r == [0.2, 0.0]

    def test_signs(self):
        r = episode_step_rewards([0, 1, 1], [0, 0, 1])
        assert r[0] == pytest.approx(0.2)
        assert r[1] == pytest.approx(-0.2)


class TestTransitionRewardStep:
    def test_add_game_populates_reward_step(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_game_log_with_td(), gamma=0.99)
        trs = list(buf.buffer)
        assert len(trs) == 6
        home = [t for t in trs if t.perspective == 'home']
        away = [t for t in trs if t.perspective == 'away']
        # Home scores between its 2nd and 3rd state; away concedes between
        # its 1st and 2nd (away states straddle the TD one turn earlier).
        assert [t.reward_step for t in home] == pytest.approx([0.0, 0.2, 0.0])
        assert [t.reward_step for t in away] == pytest.approx([-0.2, 0.0, 0.0])

    def test_no_scores_log_gets_zero_step_rewards(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_game_log_no_scores(), gamma=0.99)
        assert all(t.reward_step == 0.0 for t in buf.buffer)

    def test_legacy_pickles_fall_back_to_class_default(self):
        # Old pickles restore __dict__ without the new field; the dataclass
        # default lives on the CLASS, so attribute lookup must yield None.
        tr = Transition(features=[0.0], reward=1.0, next_features=[0.0],
                        perspective='home', is_terminal=False)
        del tr.__dict__['reward_step']
        assert tr.reward_step is None


class TestAlphaOneBitExact:
    """Stage-0 null test: alpha=1.0 == mc_return, bit-exact, all four paths."""

    def test_linear_full_log(self):
        a, b = _twin_linear()
        a.train_monte_carlo_return(_game_log_with_td(), gamma=0.99)
        b.train_mc_td_mix(_game_log_with_td(), gamma=0.99, alpha=1.0)
        assert np.array_equal(a.weights, b.weights)

    def test_linear_full_log_no_scores(self):
        a, b = _twin_linear()
        a.train_monte_carlo_return(_game_log_no_scores(), gamma=0.99)
        b.train_mc_td_mix(_game_log_no_scores(), gamma=0.99, alpha=1.0)
        assert np.array_equal(a.weights, b.weights)

    def test_neural_full_log(self):
        # UNCONDITIONAL since 271579e: the Neural full-log path carries the
        # same Lever-B fold-in as the other three — no special case here.
        a, b = _twin_neural()
        a.train_monte_carlo_return(_game_log_with_td(), gamma=0.99)
        b.train_mc_td_mix(_game_log_with_td(), gamma=0.99, alpha=1.0)
        for pa, pb in zip(_neural_params(a), _neural_params(b)):
            assert np.array_equal(pa, pb)

    def test_neural_full_log_no_scores(self):
        a, b = _twin_neural()
        a.train_monte_carlo_return(_game_log_no_scores(), gamma=0.99)
        b.train_mc_td_mix(_game_log_no_scores(), gamma=0.99, alpha=1.0)
        for pa, pb in zip(_neural_params(a), _neural_params(b)):
            assert np.array_equal(pa, pb)

    def test_linear_transition(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_game_log_with_td(), gamma=0.99)
        a, b = _twin_linear()
        for tr in buf.buffer:
            a.train_transition_return(tr.features, tr.next_features,
                                      tr.mc_return, tr.is_terminal, gamma=0.99)
            b.train_transition_td_mix(tr.features, tr.next_features,
                                      tr.mc_return, tr.reward_step,
                                      tr.is_terminal, gamma=0.99, alpha=1.0)
        assert np.array_equal(a.weights, b.weights)

    def test_neural_transition(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_game_log_with_td(), gamma=0.99)
        a, b = _twin_neural()
        for tr in buf.buffer:
            a.train_transition_return(tr.features, tr.next_features,
                                      tr.mc_return, tr.is_terminal, gamma=0.99)
            b.train_transition_td_mix(tr.features, tr.next_features,
                                      tr.mc_return, tr.reward_step,
                                      tr.is_terminal, gamma=0.99, alpha=1.0)
        for pa, pb in zip(_neural_params(a), _neural_params(b)):
            assert np.array_equal(pa, pb)


class TestMixBehavior:
    def test_linear_transition_update_equation(self):
        """Non-terminal update must match the spec formula with V(s')
        evaluated on the weights AT CALL TIME (fresh bootstrap)."""
        t = _linear(lr=0.1)
        w0 = t.weights.copy()
        f = [1.0, 0.0, 0.5, 0.0, 1.0]
        nf = [0.5, 0.5, 0.0, 1.0, 0.0]
        g, r, gamma, alpha = 0.6, 0.2, 0.99, 0.7
        v_next = float(np.dot(w0, np.array(nf)))
        target = max(-1.0, min(1.0, alpha * g + (1 - alpha) * (r + gamma * v_next)))
        v = float(np.dot(w0, np.array(f)))
        expected = w0 + 0.1 * (target - v) * np.array(f)
        t.train_transition_td_mix(f, nf, g, r, is_terminal=False,
                                  gamma=gamma, alpha=alpha)
        assert np.allclose(t.weights, expected, atol=0, rtol=0)

    def test_bootstrap_is_fresh_not_cached(self):
        """Second update on the same transition must bootstrap from the
        weights AFTER the first update — the TD staleness trap guard."""
        t = _linear(lr=0.1)
        f = [1.0, 0.0, 0.5, 0.0, 1.0]
        nf = [0.5, 0.5, 0.0, 1.0, 0.0]
        g, r, gamma, alpha = 0.6, 0.2, 0.99, 0.5
        t.train_transition_td_mix(f, nf, g, r, is_terminal=False,
                                  gamma=gamma, alpha=alpha)
        w1 = t.weights.copy()  # weights the 2nd bootstrap MUST see
        v_next = float(np.dot(w1, np.array(nf)))
        target = max(-1.0, min(1.0, alpha * g + (1 - alpha) * (r + gamma * v_next)))
        expected = w1 + 0.1 * (target - float(np.dot(w1, np.array(f)))) * np.array(f)
        t.train_transition_td_mix(f, nf, g, r, is_terminal=False,
                                  gamma=gamma, alpha=alpha)
        assert np.allclose(t.weights, expected, atol=0, rtol=0)

    def test_terminal_anchor_independent_of_alpha(self):
        """target(s_T) = terminal_value regardless of alpha — never mixed."""
        f = [1.0, 0.0, 0.5, 0.0, 1.0]
        results = []
        for alpha in (0.0, 0.7, 1.0):
            t = _linear(lr=0.1)
            t.train_transition_td_mix(f, f, -0.5, 0.0, is_terminal=True,
                                      gamma=0.99, alpha=alpha)
            results.append(t.weights.copy())
        assert np.array_equal(results[0], results[1])
        assert np.array_equal(results[1], results[2])

    def test_target_is_clamped(self):
        """A TD component outside [-1,1] must be clamped before the update."""
        t = LinearTrainer(n_features=2, learning_rate=0.1)
        t.weights = np.array([0.0, 0.0])
        # alpha=0 -> pure TD target = r + gamma*V(s') = 5.0 -> clamps to 1.0
        t.train_transition_td_mix([1.0, 0.0], [0.0, 0.0], 0.0, 5.0,
                                  is_terminal=False, gamma=0.99, alpha=0.0)
        # update = lr * (1.0 - 0.0) * f  (would be 0.5 without the clamp)
        assert t.weights[0] == pytest.approx(0.1)

    def test_alpha_lowers_variance_pulls_toward_bootstrap(self):
        """With V(s') = 0 and no step reward, alpha scales the MC pull."""
        f, nf = [1.0, 0.0], [0.0, 0.0]
        t_full = LinearTrainer(n_features=2, learning_rate=0.1)
        t_mix = LinearTrainer(n_features=2, learning_rate=0.1)
        t_full.train_transition_td_mix(f, nf, 0.8, 0.0, False, gamma=0.99, alpha=1.0)
        t_mix.train_transition_td_mix(f, nf, 0.8, 0.0, False, gamma=0.99, alpha=0.5)
        assert t_mix.weights[0] == pytest.approx(0.5 * t_full.weights[0])


class TestDispatch:
    def test_train_on_log_dispatches_with_alpha(self):
        a, b = _twin_linear()
        _train_on_log(a, _game_log_with_td(), 'mc_td_mix', gamma=0.99,
                      lambda_=0.8, td_mix_alpha=0.5)
        b.train_mc_td_mix(_game_log_with_td(), gamma=0.99, alpha=0.5)
        assert np.array_equal(a.weights, b.weights)

    def test_train_on_transition_dispatches_with_alpha(self):
        buf = ReplayBuffer(capacity=100)
        buf.add_game(_game_log_with_td(), gamma=0.99)
        a, b = _twin_linear()
        for tr in buf.buffer:
            _train_on_transition(a, tr, 'mc_td_mix', gamma=0.99, lambda_=0.8,
                                 td_mix_alpha=0.5)
            b.train_transition_td_mix(tr.features, tr.next_features,
                                      tr.mc_return, tr.reward_step,
                                      tr.is_terminal, gamma=0.99, alpha=0.5)
        assert np.array_equal(a.weights, b.weights)

    def test_legacy_transition_falls_back(self):
        """Old buffers (mc_return/reward_step None) use reward and r=0."""
        tr = Transition(features=[1.0, 0.0, 0.0, 0.0, 0.0], reward=-0.5,
                        next_features=[0.0, 1.0, 0.0, 0.0, 0.0],
                        perspective='home', is_terminal=False)
        a, b = _twin_linear()
        _train_on_transition(a, tr, 'mc_td_mix', gamma=0.99, lambda_=0.8,
                             td_mix_alpha=0.7)
        b.train_transition_td_mix(tr.features, tr.next_features, tr.reward,
                                  0.0, tr.is_terminal, gamma=0.99, alpha=0.7)
        assert np.array_equal(a.weights, b.weights)

    def test_default_method_untouched(self):
        """mc_shaped (production default) must not route through td-mix."""
        a, b = _twin_linear()
        _train_on_log(a, _game_log_with_td(), 'mc_shaped', gamma=0.99, lambda_=0.8)
        b.train_monte_carlo_shaped(_game_log_with_td(), gamma=0.99)
        assert np.array_equal(a.weights, b.weights)


class TestCliEnv:
    def test_bb_td_mix_alpha_env_reaches_run_training(self, monkeypatch):
        """BB_TD_MIX_ALPHA is the Stage-1 launch knob: it must flow through
        train_cli's argparse default into run_training."""
        import blood_bowl.train_cli as cli
        monkeypatch.setenv('BB_TD_MIX_ALPHA', '0.55')
        captured: dict = {}
        monkeypatch.setattr(cli, 'run_training', lambda **kw: captured.update(kw))
        monkeypatch.setattr('sys.argv', ['train_cli', '--training-method=mc_td_mix'])
        cli.main()
        assert captured['training_method'] == 'mc_td_mix'
        assert captured['td_mix_alpha'] == 0.55

    def test_alpha_flag_overrides_env(self, monkeypatch):
        import blood_bowl.train_cli as cli
        monkeypatch.setenv('BB_TD_MIX_ALPHA', '0.55')
        captured: dict = {}
        monkeypatch.setattr(cli, 'run_training', lambda **kw: captured.update(kw))
        monkeypatch.setattr('sys.argv', ['train_cli', '--training-method=mc_td_mix',
                                         '--td-mix-alpha=1.0'])
        cli.main()
        assert captured['td_mix_alpha'] == 1.0

    def test_alpha_defaults_to_0_7(self, monkeypatch):
        import blood_bowl.train_cli as cli
        monkeypatch.delenv('BB_TD_MIX_ALPHA', raising=False)
        captured: dict = {}
        monkeypatch.setattr(cli, 'run_training', lambda **kw: captured.update(kw))
        monkeypatch.setattr('sys.argv', ['train_cli'])
        cli.main()
        assert captured['training_method'] == 'mc'  # default method unchanged
        assert captured['td_mix_alpha'] == 0.7


class TestPreTdValueRamp:
    @staticmethod
    def _log(values: list[float], my_scores: list[int]) -> list[dict]:
        """Single-perspective log; value_fn below reads features[0]."""
        recs = [
            {'type': 'state', 'features': [v], 'perspective': 'home',
             'home_score': s, 'away_score': 0}
            for v, s in zip(values, my_scores)
        ]
        recs.append({'type': 'result', 'home_score': my_scores[-1],
                     'away_score': 0, 'winner': 'home'})
        return recs

    def test_window_semantics(self):
        # TD registered at index 4 -> pre window = indices 1..3.
        log = self._log([0.0, 0.3, 0.5, 0.7, 0.9, 0.4],
                        [0, 0, 0, 0, 1, 1])
        ramp = pre_td_value_ramp([log], lambda f: f[0], window=3)
        expected = (0.3 + 0.5 + 0.7) / 3 - (0.0 + 0.9 + 0.4) / 3
        assert ramp == pytest.approx(expected)

    def test_positive_ramp_for_rising_value(self):
        log = self._log([0.0, 0.0, 0.4, 0.6, 0.8, 0.0],
                        [0, 0, 0, 0, 0, 1])
        assert pre_td_value_ramp([log], lambda f: f[0]) > 0

    def test_none_without_td(self):
        log = self._log([0.1, 0.2, 0.3], [0, 0, 0])
        assert pre_td_value_ramp([log], lambda f: f[0]) is None

    def test_none_for_old_logs_without_scores(self):
        log = [
            {'type': 'state', 'features': [0.5], 'perspective': 'home'},
            {'type': 'state', 'features': [0.7], 'perspective': 'home'},
            {'type': 'result', 'home_score': 1, 'away_score': 0, 'winner': 'home'},
        ]
        assert pre_td_value_ramp([log], lambda f: f[0]) is None

    def test_opponent_td_does_not_open_window(self):
        # Only OWN score increases define the window; conceding does not.
        recs = [
            {'type': 'state', 'features': [0.5], 'perspective': 'home',
             'home_score': 0, 'away_score': s}
            for s in [0, 0, 1, 1]
        ]
        recs.append({'type': 'result', 'home_score': 0, 'away_score': 1,
                     'winner': 'away'})
        assert pre_td_value_ramp([recs], lambda f: f[0]) is None