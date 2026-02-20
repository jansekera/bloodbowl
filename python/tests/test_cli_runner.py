"""Tests for the CLI runner."""
import pytest
from pathlib import Path
from blood_bowl.cli_runner import CLIRunner, MatchResult


def get_project_root():
    return str(Path(__file__).parent.parent.parent)


class TestMatchResult:
    def test_winner_home(self):
        r = MatchResult(home_score=2, away_score=1, total_actions=100, phase='game_over', half=2)
        assert r.winner == 'home'

    def test_winner_away(self):
        r = MatchResult(home_score=0, away_score=1, total_actions=100, phase='game_over', half=2)
        assert r.winner == 'away'

    def test_winner_draw(self):
        r = MatchResult(home_score=1, away_score=1, total_actions=100, phase='game_over', half=2)
        assert r.winner is None


class TestCLIRunner:
    def test_project_root_detection(self):
        runner = CLIRunner(get_project_root())
        assert (runner.project_root / 'cli' / 'simulate.php').exists()
