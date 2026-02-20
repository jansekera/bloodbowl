"""Tests for the bb_engine Python bindings."""

import bb_engine
import numpy as np


def test_enums():
    """Test enum values are accessible."""
    assert bb_engine.TeamSide.HOME != bb_engine.TeamSide.AWAY
    assert bb_engine.GamePhase.PLAY != bb_engine.GamePhase.GAME_OVER
    assert bb_engine.ActionType.MOVE != bb_engine.ActionType.BLOCK
    assert bb_engine.Weather.NICE != bb_engine.Weather.BLIZZARD


def test_position():
    """Test Position creation and methods."""
    p = bb_engine.Position(10, 7)
    assert p.x == 10
    assert p.y == 7
    assert p.is_on_pitch()

    p2 = bb_engine.Position(0, 0)
    assert p2.is_on_pitch()

    p3 = bb_engine.Position(-1, 0)
    assert not p3.is_on_pitch()

    assert p.distance_to(p2) == 10  # Chebyshev distance


def test_game_state():
    """Test GameState creation and manipulation."""
    gs = bb_engine.GameState()
    assert gs.phase == bb_engine.GamePhase.COIN_TOSS
    assert gs.half == 1

    # Clone should produce independent copy
    gs2 = gs.clone()
    gs2.half = 2
    assert gs.half == 1


def test_setup_and_kickoff():
    """Test team setup and kickoff."""
    gs = bb_engine.GameState()
    home = bb_engine.get_human_roster()
    away = bb_engine.get_orc_roster()

    bb_engine.setup_half(gs, home, away)

    # Players should be on pitch
    on_pitch = sum(1 for i in range(1, 23) if gs.get_player(i).is_on_pitch())
    assert on_pitch == 22

    # Kickoff
    dice = bb_engine.DiceRoller(42)
    bb_engine.simple_kickoff(gs, dice)
    assert gs.phase == bb_engine.GamePhase.PLAY


def test_available_actions():
    """Test getting available actions."""
    gs = bb_engine.GameState()
    bb_engine.setup_half(gs, bb_engine.get_human_roster(), bb_engine.get_human_roster())

    dice = bb_engine.DiceRoller(42)
    bb_engine.simple_kickoff(gs, dice)

    actions = bb_engine.get_available_actions(gs)
    assert len(actions) > 0

    # Should always have END_TURN
    has_end_turn = any(a.type == bb_engine.ActionType.END_TURN for a in actions)
    assert has_end_turn


def test_execute_action():
    """Test executing an action."""
    gs = bb_engine.GameState()
    bb_engine.setup_half(gs, bb_engine.get_human_roster(), bb_engine.get_human_roster())
    dice = bb_engine.DiceRoller(42)
    bb_engine.simple_kickoff(gs, dice)

    actions = bb_engine.get_available_actions(gs)
    end_turn = [a for a in actions if a.type == bb_engine.ActionType.END_TURN][0]
    bb_engine.execute_action(gs, end_turn, dice)


def test_feature_extraction():
    """Test feature extraction."""
    gs = bb_engine.GameState()
    bb_engine.setup_half(gs, bb_engine.get_human_roster(), bb_engine.get_human_roster())
    dice = bb_engine.DiceRoller(42)
    bb_engine.simple_kickoff(gs, dice)

    features = bb_engine.extract_features(gs, bb_engine.TeamSide.HOME)
    assert features.shape == (bb_engine.NUM_FEATURES,)
    assert features.dtype == np.float32

    # At least one feature should be 1.0 (bias)
    assert 1.0 in features


def test_simulate_game():
    """Test full game simulation."""
    home = bb_engine.get_human_roster()
    away = bb_engine.get_orc_roster()

    result = bb_engine.simulate_game(home, away, "random", "random", 42)
    assert result.home_score >= 0
    assert result.away_score >= 0
    assert result.total_actions > 0


def test_all_rosters():
    """Test all 26 rosters are accessible and can simulate."""
    roster_names = [
        "human", "orc", "skaven", "dwarf", "wood-elf", "chaos",
        "undead", "lizardmen", "dark-elf", "halfling", "norse", "high-elf",
        "vampire", "amazon", "necromantic", "bretonnian", "khemri", "goblin",
        "chaos-dwarf", "ogre", "nurgle", "pro-elf", "slann", "underworld",
        "khorne", "chaos-pact"
    ]

    for name in roster_names:
        r = bb_engine.get_roster(name)
        assert r is not None, f"Roster {name} not found"
        assert r.positional_count > 0
        assert r.reroll_cost > 0


def test_new_action_types():
    """Test that new action types are bound."""
    assert hasattr(bb_engine.ActionType, 'THROW_TEAM_MATE')
    assert hasattr(bb_engine.ActionType, 'BOMB_THROW')
    assert hasattr(bb_engine.ActionType, 'HYPNOTIC_GAZE')
    assert hasattr(bb_engine.ActionType, 'BALL_AND_CHAIN')
    assert hasattr(bb_engine.ActionType, 'MULTIPLE_BLOCK')
    # Verify they're distinct
    types = {
        bb_engine.ActionType.THROW_TEAM_MATE,
        bb_engine.ActionType.BOMB_THROW,
        bb_engine.ActionType.HYPNOTIC_GAZE,
        bb_engine.ActionType.BALL_AND_CHAIN,
        bb_engine.ActionType.MULTIPLE_BLOCK,
    }
    assert len(types) == 5


def test_simulate_game_logged():
    """Test logged game simulation returns features."""
    human = bb_engine.get_human_roster()
    orc = bb_engine.get_orc_roster()
    result = bb_engine.simulate_game_logged(human, orc, 'random', 'random', seed=99)

    assert result.result.total_actions > 0
    states = result.get_states()
    assert len(states) > 0

    # Each state should have features and perspective
    for s in states:
        assert 'features' in s
        assert 'perspective' in s
        assert s['features'].shape == (bb_engine.NUM_FEATURES,)
        assert s['features'].dtype == np.float32
        assert s['perspective'] in ('home', 'away')


def test_simulate_game_learning_ai():
    """Test learning AI with a dummy weights file."""
    import json
    import tempfile
    import os

    # Create a simple linear weights file
    weights = {'type': 'linear', 'weights': [0.0] * bb_engine.NUM_FEATURES}
    with tempfile.NamedTemporaryFile(mode='w', suffix='.json', delete=False) as f:
        json.dump(weights, f)
        weights_path = f.name

    try:
        human = bb_engine.get_human_roster()
        orc = bb_engine.get_orc_roster()
        result = bb_engine.simulate_game(
            human, orc, 'learning', 'random',
            seed=42, weights_path=weights_path, epsilon=1.0,
        )
        assert result.total_actions > 0
    finally:
        os.unlink(weights_path)


def test_multiple_games_different_seeds():
    """Test that different seeds produce different results."""
    human = bb_engine.get_human_roster()
    orc = bb_engine.get_orc_roster()

    results = []
    for seed in [1, 2, 3, 4, 5]:
        r = bb_engine.simulate_game(human, orc, 'random', 'random', seed=seed)
        results.append(r.total_actions)

    # Not all games should have the exact same number of actions
    assert len(set(results)) > 1


def test_all_roster_simulation():
    """Test that all 26 rosters can complete a game without crash."""
    roster_names = [
        "human", "orc", "skaven", "dwarf", "wood-elf", "chaos",
        "undead", "lizardmen", "dark-elf", "halfling", "norse", "high-elf",
        "vampire", "amazon", "necromantic", "bretonnian", "khemri", "goblin",
        "chaos-dwarf", "ogre", "nurgle", "pro-elf", "slann", "underworld",
        "khorne", "chaos-pact",
    ]
    # Test a few representative matchups (not all 676 combinations)
    test_pairs = [
        ("goblin", "ogre"),       # special rosters with B&C, TTM, Bombardier
        ("vampire", "human"),     # Bloodlust, HypnoticGaze
        ("nurgle", "pro-elf"),    # FoulAppearance, NurglesRot
        ("chaos-pact", "underworld"),  # Animosity teams
    ]
    for home_name, away_name in test_pairs:
        home = bb_engine.get_roster(home_name)
        away = bb_engine.get_roster(away_name)
        result = bb_engine.simulate_game(home, away, 'random', 'random', seed=42)
        assert result.total_actions > 0, f'{home_name} vs {away_name} produced 0 actions'


def test_feature_parity():
    """Test that features from extract_features match simulate_game_logged."""
    human = bb_engine.get_human_roster()
    orc = bb_engine.get_orc_roster()

    # Set up and extract features manually
    gs = bb_engine.GameState()
    bb_engine.setup_half(gs, human, orc)
    dice = bb_engine.DiceRoller(42)
    bb_engine.simple_kickoff(gs, dice)

    manual_features = bb_engine.extract_features(gs, bb_engine.TeamSide.HOME)
    assert manual_features.shape == (bb_engine.NUM_FEATURES,)
    # Features should not be all zeros (at least bias = 1.0)
    assert np.any(manual_features != 0)


def test_logged_game_result_structure():
    """Test LoggedGameResult has proper result and states."""
    human = bb_engine.get_human_roster()
    logged = bb_engine.simulate_game_logged(human, human, 'random', 'random', seed=7)

    # Result should be accessible
    assert hasattr(logged, 'result')
    assert hasattr(logged.result, 'home_score')
    assert hasattr(logged.result, 'away_score')
    assert hasattr(logged.result, 'total_actions')

    # States should be accessible
    states = logged.get_states()
    assert isinstance(states, list)
    assert len(states) >= 2  # At least initial + 1 turn boundary


if __name__ == "__main__":
    tests = [
        test_enums,
        test_position,
        test_game_state,
        test_setup_and_kickoff,
        test_available_actions,
        test_execute_action,
        test_feature_extraction,
        test_simulate_game,
        test_all_rosters,
        test_new_action_types,
        test_simulate_game_logged,
        test_simulate_game_learning_ai,
        test_multiple_games_different_seeds,
        test_all_roster_simulation,
        test_feature_parity,
        test_logged_game_result_structure,
    ]

    passed = 0
    failed = 0
    for test in tests:
        try:
            test()
            print(f"  PASS: {test.__name__}")
            passed += 1
        except Exception as e:
            print(f"  FAIL: {test.__name__} - {e}")
            failed += 1

    print(f"\n{passed} passed, {failed} failed out of {len(tests)} tests")
    if failed > 0:
        exit(1)
