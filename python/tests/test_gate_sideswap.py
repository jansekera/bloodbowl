"""Offline stub tests for the gating/benchmark side-swap (2026-07-14 proposal).

Validation tier (a) of proposals_gating_sideswap_20260714.md section 3: zero
engine dependency — `bb_engine` is faked in sys.modules, so these tests verify
the ORIENTATION PLUMBING only (which weights path / AI goes to which slot, the
candidate-perspective return flip, the echoed orientation flag, and legacy
tuple-arity backward compatibility), never actual game play.

Key contract points (proposal section 1):
- Orientation is decided by the WORKER from the task tuple (10th element for
  _gate_game, 8th for _benchmark_game) and echoed back in the return value —
  never derived by the caller from result order, because _imap_watchdog drops
  skipped games from the yield stream.
- _gate_game's return is ALWAYS candidate-first regardless of slot.
- Legacy <=9-tuple (_gate_game) / <=7-tuple (_benchmark_game) callers
  (diag_utils.run_arm, diag_mirror_budget.py, fuzz_gate.py, diag_* scripts)
  keep the exact pre-swap behavior and return arity.
"""
from __future__ import annotations

import sys
import types
from collections import Counter
from pathlib import Path

import pytest

# run_iteration.py lives at the repo root, one level above python/.
PROJECT_ROOT = str(Path(__file__).parent.parent.parent)
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from run_iteration import (_RACES, _benchmark_game, _gate_game,
                           _race_guard_verdict)


class _FakeResult:
    def __init__(self, home_score: int, away_score: int):
        self.home_score = home_score
        self.away_score = away_score


class _FakeSim:
    def __init__(self, result: _FakeResult):
        self.result = result


@pytest.fixture
def fake_engine(monkeypatch):
    """Install a fake bb_engine that records simulate_game_logged kwargs and
    returns deterministic scores (default home 7 - away 3)."""
    mod = types.ModuleType('bb_engine')
    mod.scores = (7, 3)  # (home_score, away_score); tests may override
    mod.calls = []

    def get_developed_roster(race: str, tv: int):
        return ('roster', race, tv)

    def simulate_game_logged(hr, ar, **kwargs):
        mod.calls.append({'hr': hr, 'ar': ar, **kwargs})
        return _FakeSim(_FakeResult(*mod.scores))

    mod.get_developed_roster = get_developed_roster
    mod.simulate_game_logged = simulate_game_logged
    monkeypatch.setitem(sys.modules, 'bb_engine', mod)
    return mod


class TestGateGameSideSwap:
    def test_10tuple_cand_home(self, fake_engine):
        res = _gate_game((123, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False))
        assert res == (7, 3, 0)
        call = fake_engine.calls[-1]
        assert call['weights_path'] == 'GATE'
        assert call['away_weights_path'] == 'FROZEN'
        assert call['home_ai'] == 'macro_mcts' and call['away_ai'] == 'macro_mcts'
        assert call['policy_weights_path'] == 'POL'
        assert call['seed'] == 123

    def test_10tuple_cand_away_swaps_paths_and_flips_score(self, fake_engine):
        res = _gate_game((123, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', True))
        # home 7 - away 3, candidate sits AWAY -> candidate-first = (3, 7)
        assert res == (3, 7, 1)
        call = fake_engine.calls[-1]
        assert call['weights_path'] == 'FROZEN'
        assert call['away_weights_path'] == 'GATE'
        assert call['home_ai'] == 'macro_mcts' and call['away_ai'] == 'macro_mcts'

    def test_10tuple_cand_away_win_reported_as_win(self, fake_engine):
        fake_engine.scores = (1, 4)  # away (candidate) wins
        res = _gate_game((7, 2, 'GATE', 'FROZEN', 100, 0.0, 1200, False, '', True))
        assert res == (4, 1, 1)

    def test_orientation_does_not_touch_rosters(self, fake_engine):
        """Rosters are matchup properties (race_idx), not slot properties —
        both orientations of the same task index play the same hr/ar pair."""
        _gate_game((1, 3, 'GATE', 'FROZEN', 100, 0.0, 1200, False, '', False))
        _gate_game((1, 3, 'GATE', 'FROZEN', 100, 0.0, 1200, False, '', True))
        fwd, swp = fake_engine.calls[-2], fake_engine.calls[-1]
        assert fwd['hr'] == swp['hr'] == ('roster', _RACES[3], 1200)
        assert fwd['ar'] == swp['ar'] == ('roster', _RACES[4], 1200)

    @pytest.mark.parametrize('args', [
        (123, 0, 'GATE', 'FROZEN', 100, 0.0, 1200),                    # 7-tuple
        (123, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, True),              # 8-tuple
        (123, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, True, 'POL'),       # 9-tuple
    ])
    def test_legacy_tuples_unchanged(self, fake_engine, args):
        res = _gate_game(args)
        assert res == (7, 3)          # 2-tuple, home-away as before, no flag
        assert len(res) == 2
        call = fake_engine.calls[-1]
        assert call['weights_path'] == 'GATE'          # candidate always HOME
        assert call['away_weights_path'] == 'FROZEN'
        assert call['leaf_lookahead'] == (len(args) >= 8)
        assert call['policy_weights_path'] == ('POL' if len(args) >= 9 else '')


class TestBenchmarkGameSideSwap:
    def test_8tuple_cand_away(self, fake_engine):
        res = _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True))
        call = fake_engine.calls[-1]
        assert call['home_ai'] == 'random' and call['away_ai'] == 'macro_mcts'
        assert call['weights_path'] == 'GATE'  # away VF falls back to weights_path
        assert 'away_weights_path' not in call
        assert res is False               # home 7 - away 3: candidate (away) lost

    def test_8tuple_cand_away_win(self, fake_engine):
        fake_engine.scores = (1, 4)
        assert _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True)) is True

    def test_8tuple_cand_home(self, fake_engine):
        res = _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', False))
        call = fake_engine.calls[-1]
        assert call['home_ai'] == 'macro_mcts' and call['away_ai'] == 'random'
        assert res is True                # home 7 - away 3: candidate (home) won

    @pytest.mark.parametrize('args', [
        (5, 1, 'GATE', 100, 0.0, 1200),           # 6-tuple
        (5, 1, 'GATE', 100, 0.0, 1200, 'POL'),    # 7-tuple
    ])
    def test_legacy_tuples_unchanged(self, fake_engine, args):
        res = _benchmark_game(args)
        call = fake_engine.calls[-1]
        assert call['home_ai'] == 'macro_mcts' and call['away_ai'] == 'random'
        assert call['weights_path'] == 'GATE'
        assert call['policy_weights_path'] == ('POL' if len(args) >= 7 else '')
        assert res is True                # home 7 - away 3, candidate always HOME


class TestGatePolicyBlend:
    """Krok 2 policy aktivace (2026-08-03): 11. prvek _gate_game je trojice
    (cand_policy_blend, frozen_policy_path, frozen_policy_blend); 9. prvek
    _benchmark_game je cand_policy_blend. Default = bajtově původní chování
    (blend 0, away sdílí home síť)."""

    def test_gate_11tuple_cand_home_shared_frozen(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False,
                    (0.2, '', 0.0)))
        call = fake_engine.calls[-1]
        assert call['policy_weights_path'] == 'POL'
        assert call['policy_blend'] == 0.2
        assert call['away_policy_weights_path'] == ''   # frozen sdílí síť
        assert call['away_policy_blend'] == 0.0

    def test_gate_11tuple_cand_away_shared_frozen(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', True,
                    (0.2, '', 0.0)))
        call = fake_engine.calls[-1]
        # HOME = frozen sdílí stash síť s blendem 0; AWAY = kandidát s blendem
        assert call['policy_weights_path'] == 'POL'
        assert call['policy_blend'] == 0.0
        assert call['away_policy_weights_path'] == ''
        assert call['away_policy_blend'] == 0.2

    def test_gate_11tuple_cand_home_frozen_own_policy(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False,
                    (0.2, 'BESTPOL', 0.1)))
        call = fake_engine.calls[-1]
        assert call['policy_weights_path'] == 'POL'
        assert call['policy_blend'] == 0.2
        assert call['away_policy_weights_path'] == 'BESTPOL'
        assert call['away_policy_blend'] == 0.1

    def test_gate_11tuple_cand_away_frozen_own_policy(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', True,
                    (0.2, 'BESTPOL', 0.1)))
        call = fake_engine.calls[-1]
        assert call['policy_weights_path'] == 'BESTPOL'   # HOME = frozen
        assert call['policy_blend'] == 0.1
        assert call['away_policy_weights_path'] == 'POL'  # AWAY = kandidát
        assert call['away_policy_blend'] == 0.2

    def test_gate_selection_h2h_blend_both_sides(self, fake_engine):
        """Selection H2H: obě strany stejný blend nad sdílenou stash policy."""
        _gate_game((1, 0, 'AZ', 'TB', 100, 0.0, 1200, False, 'POL', False,
                    (0.2, '', 0.2)))
        call = fake_engine.calls[-1]
        assert call['policy_blend'] == 0.2
        assert call['away_policy_blend'] == 0.2
        assert call['away_policy_weights_path'] == ''

    def test_gate_10tuple_default_is_byte_identical(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False))
        call = fake_engine.calls[-1]
        assert call['policy_blend'] == 0.0
        assert call['away_policy_blend'] == 0.0
        assert call['away_policy_weights_path'] == ''

    def test_benchmark_9tuple_cand_home(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', False, 0.2))
        call = fake_engine.calls[-1]
        assert call['policy_blend'] == 0.2
        assert call['away_policy_blend'] == 0.0

    def test_benchmark_9tuple_cand_away(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True, 0.2))
        call = fake_engine.calls[-1]
        assert call['policy_blend'] == 0.0
        assert call['away_policy_blend'] == 0.2

    def test_benchmark_8tuple_default_blend_zero(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True))
        call = fake_engine.calls[-1]
        assert call['policy_blend'] == 0.0
        assert call['away_policy_blend'] == 0.0


class TestStagedPickup:
    """Item 13 wiring (2026-08-05): 12. prvek _gate_game je dvojice
    (cand_staged, frozen_staged); 10. prvek _benchmark_game je cand_staged.
    Default = plánovač vypnutý na obou stranách (dnešní chování)."""

    def test_gate_12tuple_cand_home(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False,
                    (0.2, '', 0.0), (True, False)))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is True        # HOME = kandidát
        assert call['away_staged_pickup'] is False  # AWAY = frozen bez plánovače

    def test_gate_12tuple_cand_away(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', True,
                    (0.2, '', 0.0), (True, False)))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is False       # HOME = frozen
        assert call['away_staged_pickup'] is True   # AWAY = kandidát

    def test_gate_12tuple_frozen_promoted_with_planner(self, fake_engine):
        """Šampion promotnutý s plánovačem (meta staged_pickup=true) s ním
        hraje i ve frozen slotu — fairness vzor policy_blend."""
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', True,
                    (0.2, 'BESTPOL', 0.2), (True, True)))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is True
        assert call['away_staged_pickup'] is True

    def test_gate_11tuple_default_off_both_sides(self, fake_engine):
        _gate_game((1, 0, 'GATE', 'FROZEN', 100, 0.0, 1200, False, 'POL', False,
                    (0.2, '', 0.0)))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is False
        assert call['away_staged_pickup'] is False

    def test_benchmark_10tuple_cand_home(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', False, 0.2, True))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is True
        assert call['away_staged_pickup'] is False  # random strana nikdy

    def test_benchmark_10tuple_cand_away(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True, 0.2, True))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is False
        assert call['away_staged_pickup'] is True

    def test_benchmark_9tuple_default_off(self, fake_engine):
        _benchmark_game((5, 1, 'GATE', 100, 0.0, 1200, 'POL', True, 0.2))
        call = fake_engine.calls[-1]
        assert call['staged_pickup'] is False
        assert call['away_staged_pickup'] is False


class TestScheduleBalance:
    """The production schedules assign orientation i % 2 and matchup
    races[i%5] vs races[(i+1)%5]; gcd(2,5)=1 -> period 10, so every
    (matchup x orientation) cell gets exactly N/10 games."""

    @staticmethod
    def _cells(n: int) -> Counter:
        # (race cell, orientation) exactly as the production comprehensions
        # derive them from the task index i.
        return Counter((i % 5, i % 2 == 1) for i in range(n))

    def test_gate_n600_exact_balance(self):
        cells = self._cells(600)
        orient = Counter(away for _, away in cells.elements())
        assert orient[False] == orient[True] == 300
        assert len(cells) == 10
        assert all(v == 60 for v in cells.values())

    def test_benchmark_half_bm_200_exact_balance(self):
        cells = self._cells(200)
        orient = Counter(away for _, away in cells.elements())
        assert orient[False] == orient[True] == 100
        assert len(cells) == 10
        assert all(v == 20 for v in cells.values())


class TestRaceGuardEcho:
    """13th tuple element (echo_race, 2026-08-06): worker echoes race_idx so
    the caller can attribute races skip-proof (never from result order)."""

    def test_13tuple_echoes_race_idx_cand_home(self, fake_engine):
        res = _gate_game((123, 3, 'GATE', 'FROZEN', 100, 0.0, 1200,
                          False, 'POL', False, (0.2, '', 0.0), (True, False),
                          True))
        assert res == (7, 3, 0, 3)

    def test_13tuple_echoes_race_idx_cand_away(self, fake_engine):
        res = _gate_game((123, 7, 'GATE', 'FROZEN', 100, 0.0, 1200,
                          False, 'POL', True, (0.2, '', 0.0), (True, False),
                          True))
        # home 7 - away 3, cand AWAY -> (3, 7); race_idx 7 % 5 = 2
        assert res == (3, 7, 1, 2)

    def test_13tuple_false_keeps_3tuple(self, fake_engine):
        res = _gate_game((123, 3, 'GATE', 'FROZEN', 100, 0.0, 1200,
                          False, 'POL', False, (0.2, '', 0.0), (True, False),
                          False))
        assert res == (7, 3, 0)

    def test_12tuple_unchanged(self, fake_engine):
        res = _gate_game((123, 3, 'GATE', 'FROZEN', 100, 0.0, 1200,
                          False, 'POL', False, (0.2, '', 0.0), (True, False)))
        assert res == (7, 3, 0)

    def test_race_attribution_formula(self):
        # home = _RACES[ri%5], away = _RACES[(ri+1)%5]; cand away kdyz ca=1.
        # Kandidatova rasa = _RACES[(ri+ca)%5], frozen = _RACES[(ri+1-ca)%5].
        for ri in range(10):
            for ca in (0, 1):
                home, away = _RACES[ri % 5], _RACES[(ri + 1) % 5]
                cand = _RACES[(ri % 5 + ca) % 5]
                frozen = _RACES[(ri % 5 + 1 - ca) % 5]
                assert cand == (away if ca else home)
                assert frozen == (home if ca else away)


class TestRaceGuardVerdict:
    def test_big_regression_vetoes(self):
        # Elfi-intuice-style: cand-as-dwarf 33 %, frozen-as-dwarf 50 %.
        pc = {'dwarf': [25, 45, 50]}          # 75 decisive, 33.3 %
        pf = {'dwarf': [40, 40, 40]}          # frozen wr = L/(W+L) = 50 %
        v = _race_guard_verdict(pc, pf, 'dwarf', 1.28)
        assert v is not None and v['veto']
        assert v['cand_wr'] < 0.35 and abs(v['frozen_wr'] - 0.5) < 1e-9

    def test_noise_passes(self):
        pc = {'dwarf': [36, 45, 39]}          # 48 %
        pf = {'dwarf': [39, 45, 36]}          # frozen wr 48 % -> delta 0
        v = _race_guard_verdict(pc, pf, 'dwarf', 1.28)
        assert v is not None and not v['veto']

    def test_improvement_passes(self):
        pc = {'dwarf': [50, 40, 25]}
        pf = {'dwarf': [45, 40, 40]}          # frozen wr 40/85 = 47 %
        v = _race_guard_verdict(pc, pf, 'dwarf', 1.28)
        assert v is not None and not v['veto'] and v['delta'] > 0

    def test_no_decisive_returns_none(self):
        assert _race_guard_verdict({'dwarf': [0, 10, 0]},
                                   {'dwarf': [5, 5, 5]}, 'dwarf', 1.28) is None
        assert _race_guard_verdict({}, {}, 'dwarf', 1.28) is None

    def test_frozen_wr_is_from_candidate_perspective(self):
        # Ve hrach, kde dwarf hraje FROZEN, je W/D/L porad z pohledu
        # kandidata: frozen vyhry = kandidatovy prohry (L).
        pc = {'dwarf': [30, 0, 30]}           # cand 50 %
        pf = {'dwarf': [10, 0, 50]}           # frozen wr = 50/60 = 83 %
        v = _race_guard_verdict(pc, pf, 'dwarf', 1.28)
        assert abs(v['frozen_wr'] - 50 / 60) < 1e-9
        assert v['veto']                      # 50 % vs 83 % = tezka regrese
