"""Tests for feature extraction."""
import json
import subprocess
from pathlib import Path

import pytest

from blood_bowl.features import NUM_FEATURES, extract_features


def get_project_root():
    return str(Path(__file__).parent.parent.parent)


class TestFeatureExtractor:
    def test_output_has_correct_feature_count(self):
        state = _make_simple_state()
        features = extract_features(state, 'home')
        assert len(features) == NUM_FEATURES
        assert NUM_FEATURES == 70

    def test_bias_is_always_one(self):
        state = _make_simple_state()
        home_feat = extract_features(state, 'home')
        away_feat = extract_features(state, 'away')
        assert home_feat[29] == 1.0
        assert away_feat[29] == 1.0

    def test_empty_state_defaults(self):
        state = {
            'half': 1,
            'activeTeam': 'home',
            'homeTeam': {'score': 0, 'rerolls': 3, 'turnNumber': 1,
                         'blitzUsedThisTurn': False, 'passUsedThisTurn': False},
            'awayTeam': {'score': 0, 'rerolls': 2, 'turnNumber': 1,
                         'blitzUsedThisTurn': False, 'passUsedThisTurn': False},
            'players': [],
            'ball': {'position': None, 'isHeld': False, 'carrierId': None},
            'weather': 'nice',
            'kickingTeam': 'away',
        }
        features = extract_features(state, 'home')
        assert len(features) == NUM_FEATURES
        # score_diff = 0
        assert features[0] == 0.0
        # bias
        assert features[29] == 1.0
        # weather_nice
        assert features[24] == 1.0

    def test_cross_validation_with_php(self):
        """Run PHP FeatureExtractor via CLI and compare with Python version."""
        project_root = get_project_root()
        php_script = Path(project_root) / 'cli' / 'extract_features.php'

        # Create a minimal PHP script for cross-validation
        php_code = '''<?php
require_once __DIR__ . '/../vendor/autoload.php';
use App\\AI\\FeatureExtractor;
use App\\DTO\\GameState;
use App\\DTO\\TeamStateDTO;
use App\\DTO\\MatchPlayerDTO;
use App\\DTO\\BallState;
use App\\Enum\\TeamSide;
use App\\Enum\\PlayerState;
use App\\Enum\\GamePhase;
use App\\Enum\\Weather;
use App\\ValueObject\\PlayerStats;
use App\\ValueObject\\Position;

// Build a test state
$homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withScore(1);
$awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);

$p1 = MatchPlayerDTO::create(1, 1, 'P1', 1, 'Blitzer', new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME, new Position(10, 7));
$p2 = MatchPlayerDTO::create(2, 2, 'P2', 2, 'Lineman', new PlayerStats(6, 3, 3, 8), [], TeamSide::AWAY, new Position(15, 7));

$state = new GameState(
    matchId: 1, half: 1, phase: GamePhase::PLAY, activeTeam: TeamSide::HOME,
    homeTeam: $homeTeam, awayTeam: $awayTeam,
    players: [1 => $p1, 2 => $p2],
    ball: BallState::carried(new Position(10, 7), 1),
    turnoverPending: false, kickingTeam: TeamSide::AWAY, weather: Weather::NICE,
);

$features = FeatureExtractor::extract($state, TeamSide::HOME);
echo json_encode(['features' => $features, 'state' => $state->toArray()]);
'''
        php_script.write_text(php_code)
        try:
            result = subprocess.run(
                ['php', str(php_script)],
                capture_output=True, text=True, timeout=10,
                cwd=project_root,
            )
            if result.returncode != 0:
                pytest.skip(f'PHP execution failed: {result.stderr}')

            data = json.loads(result.stdout)
            php_features = data['features']
            state_dict = data['state']

            # Now extract with Python
            py_features = extract_features(state_dict, 'home')

            assert len(php_features) == len(py_features)
            for i, (php_val, py_val) in enumerate(zip(php_features, py_features)):
                assert abs(php_val - py_val) < 0.001, \
                    f'Feature {i}: PHP={php_val}, Python={py_val}'
        finally:
            php_script.unlink(missing_ok=True)


    def test_sideline_features(self):
        state = _make_simple_state()
        # Move player 1 to sideline Y=0
        state['players'][0]['position'] = {'x': 10, 'y': 0}
        features = extract_features(state, 'home')

        # 1 of 1 home standing on sideline → 1.0
        assert abs(features[30] - 1.0) < 0.001
        # opp not on sideline → 0.0
        assert abs(features[31] - 0.0) < 0.001

    def test_stall_incentive_features(self):
        state = _make_simple_state()
        # HOME carrier at x=23 (near endzone for HOME: dist=2), ahead 1-0, turn 1
        state['players'][0]['position'] = {'x': 23, 'y': 7}
        state['ball'] = {'position': {'x': 23, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        state['homeTeam']['score'] = 1
        state['homeTeam']['turnNumber'] = 1
        state['awayTeam']['score'] = 0
        features = extract_features(state, 'home')

        # carrier_near_endzone = 1.0 (dist=2 <= 3)
        assert abs(features[34] - 1.0) < 0.001
        # score_advantage_with_ball = (1+1)/4 = 0.5
        assert abs(features[33] - 0.5) < 0.001
        # turns_remaining = (9-1)/8 = 1.0
        assert abs(features[32] - 1.0) < 0.001
        # stall_incentive = 0.5 * 1.0 * 1.0 = 0.5
        assert abs(features[35] - 0.5) < 0.001

    def test_carrier_tz_count(self):
        state = _make_simple_state()
        # HOME carrier at (10,7), AWAY adjacent at (11,7) and (10,8)
        state['players'] = [
            {'id': 1, 'teamSide': 'home', 'state': 'standing',
             'position': {'x': 10, 'y': 7},
             'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8}},
            {'id': 2, 'teamSide': 'away', 'state': 'standing',
             'position': {'x': 11, 'y': 7},
             'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8}},
            {'id': 3, 'teamSide': 'away', 'state': 'standing',
             'position': {'x': 10, 'y': 8},
             'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8}},
        ]
        state['ball'] = {'position': {'x': 10, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        features = extract_features(state, 'home')
        # 2 opp adjacent → 2/4 = 0.5
        assert abs(features[40] - 0.5) < 0.001

    def test_scoring_threat(self):
        state = _make_simple_state()
        # HOME carrier at x=22, MA=6 → dist to endzone (25) = 3, MA(6) >= 3 → threat
        state['players'][0]['position'] = {'x': 22, 'y': 7}
        state['players'][0]['stats']['movement'] = 6
        state['ball'] = {'position': {'x': 22, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        features = extract_features(state, 'home')
        assert abs(features[41] - 1.0) < 0.001

    def test_scoring_threat_not_enough_movement(self):
        state = _make_simple_state()
        # HOME carrier at x=10, MA=6 → dist to endzone = 15, MA(6) < 15 → no threat
        state['players'][0]['stats']['movement'] = 6
        state['ball'] = {'position': {'x': 10, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        features = extract_features(state, 'home')
        assert abs(features[41] - 0.0) < 0.001

    def test_opp_scoring_threat(self):
        state = _make_simple_state()
        # AWAY carrier at x=3, MA=6 → dist to AWAY endzone (0) = 3, MA(6) >= 3 → threat
        state['players'][1]['position'] = {'x': 3, 'y': 7}
        state['players'][1]['stats']['movement'] = 6
        state['ball'] = {'position': {'x': 3, 'y': 7}, 'isHeld': True, 'carrierId': 2}
        features = extract_features(state, 'home')
        assert abs(features[42] - 1.0) < 0.001

    def test_engaged_fractions(self):
        state = _make_simple_state()
        # Move away player adjacent to home player
        state['players'][1]['position'] = {'x': 11, 'y': 7}
        features = extract_features(state, 'home')
        # Only 1 home standing, 1 away standing, both adjacent
        assert abs(features[43] - 1.0) < 0.001  # my_engaged_fraction
        assert abs(features[44] - 1.0) < 0.001  # opp_engaged_fraction

    def test_engaged_fractions_no_contact(self):
        state = _make_simple_state()
        # HOME at (5,7), AWAY at (20,7) → far apart, not engaged
        state['players'][0]['position'] = {'x': 5, 'y': 7}
        state['players'][1]['position'] = {'x': 20, 'y': 7}
        state['ball'] = {'position': {'x': 5, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        features = extract_features(state, 'home')
        assert abs(features[43] - 0.0) < 0.001  # my_engaged_fraction
        assert abs(features[44] - 0.0) < 0.001  # opp_engaged_fraction

    def test_prone_stunned_features(self):
        state = _make_simple_state()
        state['players'].append(
            {'id': 3, 'teamSide': 'home', 'state': 'prone',
             'position': {'x': 8, 'y': 7},
             'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8}},
        )
        state['players'].append(
            {'id': 4, 'teamSide': 'away', 'state': 'stunned',
             'position': {'x': 18, 'y': 7},
             'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8}},
        )
        features = extract_features(state, 'home')
        # 1 prone home / 11 ≈ 0.0909
        assert abs(features[45] - 1.0 / 11.0) < 0.001
        # 1 stunned away / 11 ≈ 0.0909
        assert abs(features[46] - 1.0 / 11.0) < 0.001

    def test_free_players_feature(self):
        state = _make_simple_state()
        # HOME at (5,7), AWAY at (20,7) → not engaged → 1 free
        state['players'][0]['position'] = {'x': 5, 'y': 7}
        state['players'][1]['position'] = {'x': 20, 'y': 7}
        state['ball'] = {'position': {'x': 5, 'y': 7}, 'isHeld': True, 'carrierId': 1}
        features = extract_features(state, 'home')
        # 1 standing, 0 engaged → 1 free / 11 ≈ 0.0909
        assert abs(features[47] - 1.0 / 11.0) < 0.001


def _make_simple_state() -> dict:
    return {
        'half': 1,
        'activeTeam': 'home',
        'homeTeam': {
            'score': 1, 'rerolls': 3, 'turnNumber': 3,
            'blitzUsedThisTurn': False, 'passUsedThisTurn': False,
        },
        'awayTeam': {
            'score': 0, 'rerolls': 2, 'turnNumber': 3,
            'blitzUsedThisTurn': False, 'passUsedThisTurn': False,
        },
        'players': [
            {
                'id': 1, 'teamSide': 'home', 'state': 'standing',
                'position': {'x': 10, 'y': 7},
                'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8},
            },
            {
                'id': 2, 'teamSide': 'away', 'state': 'standing',
                'position': {'x': 15, 'y': 7},
                'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8},
            },
        ],
        'ball': {
            'position': {'x': 10, 'y': 7},
            'isHeld': True,
            'carrierId': 1,
        },
        'weather': 'nice',
        'kickingTeam': 'away',
    }
