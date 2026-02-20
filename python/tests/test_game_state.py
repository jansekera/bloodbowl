"""Tests for the game state data classes."""
import json
import pytest
from blood_bowl.game_state import GameState, MatchPlayer, TeamState, BallState, Position


def make_sample_state():
    return {
        'matchId': 1,
        'half': 1,
        'phase': 'play',
        'activeTeam': 'home',
        'homeTeam': {
            'teamId': 1, 'name': 'Home FC', 'raceName': 'Human',
            'side': 'home', 'score': 0, 'rerolls': 3, 'turnNumber': 1,
            'rerollUsedThisTurn': False, 'blitzUsedThisTurn': False,
            'passUsedThisTurn': False, 'foulUsedThisTurn': False,
        },
        'awayTeam': {
            'teamId': 2, 'name': 'Away Utd', 'raceName': 'Orc',
            'side': 'away', 'score': 0, 'rerolls': 2, 'turnNumber': 1,
            'rerollUsedThisTurn': False, 'blitzUsedThisTurn': False,
            'passUsedThisTurn': False, 'foulUsedThisTurn': False,
        },
        'players': [
            {
                'id': 1, 'playerId': 1, 'name': 'Player 1', 'number': 1,
                'positionalName': 'Lineman', 'stats': {'movement': 6, 'strength': 3, 'agility': 3, 'armour': 8},
                'skills': ['Block'], 'teamSide': 'home', 'state': 'standing',
                'position': {'x': 5, 'y': 7}, 'hasMoved': False, 'hasActed': False,
                'movementRemaining': 6,
            },
            {
                'id': 2, 'playerId': 2, 'name': 'Player 2', 'number': 2,
                'positionalName': 'Blitzer', 'stats': {'movement': 7, 'strength': 3, 'agility': 3, 'armour': 8},
                'skills': [], 'teamSide': 'away', 'state': 'standing',
                'position': {'x': 20, 'y': 7}, 'hasMoved': False, 'hasActed': False,
                'movementRemaining': 7,
            },
        ],
        'ball': {'position': {'x': 5, 'y': 7}, 'isHeld': True, 'carrierId': 1},
        'turnoverPending': False,
        'kickingTeam': 'away',
        'aiTeam': 'away',
    }


def test_game_state_from_dict():
    data = make_sample_state()
    state = GameState.from_dict(data)

    assert state.match_id == 1
    assert state.phase == 'play'
    assert state.active_team == 'home'
    assert state.half == 1
    assert state.ai_team == 'away'


def test_team_state():
    data = make_sample_state()
    state = GameState.from_dict(data)

    assert state.home_team.name == 'Home FC'
    assert state.home_team.race_name == 'Human'
    assert state.home_team.rerolls == 3
    assert state.away_team.name == 'Away Utd'


def test_players():
    data = make_sample_state()
    state = GameState.from_dict(data)

    assert len(state.players) == 2
    p1 = state.players[0]
    assert p1.name == 'Player 1'
    assert p1.position == Position(5, 7)
    assert p1.stats.movement == 6
    assert 'Block' in p1.skills


def test_ball_state():
    data = make_sample_state()
    state = GameState.from_dict(data)

    assert state.ball.is_held
    assert state.ball.carrier_id == 1
    assert state.ball.position == Position(5, 7)


def test_get_team_players():
    data = make_sample_state()
    state = GameState.from_dict(data)

    home = state.get_team_players('home')
    assert len(home) == 1
    assert home[0].id == 1

    away = state.get_team_players('away')
    assert len(away) == 1
    assert away[0].id == 2


def test_position_distance():
    p1 = Position(5, 5)
    p2 = Position(8, 7)
    assert p1.distance_to(p2) == 3  # Chebyshev distance


def test_ai_team_null():
    data = make_sample_state()
    data['aiTeam'] = None
    state = GameState.from_dict(data)
    assert state.ai_team is None
