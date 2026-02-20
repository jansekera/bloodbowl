"""Data classes mirroring the PHP DTOs."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional


@dataclass(frozen=True)
class Position:
    x: int
    y: int

    def distance_to(self, other: Position) -> int:
        return max(abs(self.x - other.x), abs(self.y - other.y))


@dataclass(frozen=True)
class PlayerStats:
    movement: int
    strength: int
    agility: int
    armour: int


@dataclass
class MatchPlayer:
    id: int
    player_id: int
    name: str
    number: int
    positional_name: str
    stats: PlayerStats
    skills: list[str]
    team_side: str
    state: str
    position: Optional[Position]
    has_moved: bool
    has_acted: bool
    movement_remaining: int

    @classmethod
    def from_dict(cls, data: dict) -> MatchPlayer:
        pos = None
        if data.get('position'):
            pos = Position(data['position']['x'], data['position']['y'])
        return cls(
            id=data['id'],
            player_id=data['playerId'],
            name=data['name'],
            number=data['number'],
            positional_name=data['positionalName'],
            stats=PlayerStats(**data['stats']),
            skills=data.get('skills', []),
            team_side=data['teamSide'],
            state=data['state'],
            position=pos,
            has_moved=data.get('hasMoved', False),
            has_acted=data.get('hasActed', False),
            movement_remaining=data.get('movementRemaining', 0),
        )


@dataclass
class BallState:
    position: Optional[Position]
    is_held: bool
    carrier_id: Optional[int]

    @classmethod
    def from_dict(cls, data: dict) -> BallState:
        pos = None
        if data.get('position'):
            pos = Position(data['position']['x'], data['position']['y'])
        return cls(
            position=pos,
            is_held=data.get('isHeld', False),
            carrier_id=data.get('carrierId'),
        )


@dataclass
class TeamState:
    team_id: int
    name: str
    race_name: str
    side: str
    score: int
    rerolls: int
    turn_number: int

    @classmethod
    def from_dict(cls, data: dict) -> TeamState:
        return cls(
            team_id=data['teamId'],
            name=data['name'],
            race_name=data['raceName'],
            side=data['side'],
            score=data.get('score', 0),
            rerolls=data.get('rerolls', 0),
            turn_number=data.get('turnNumber', 0),
        )


@dataclass
class GameState:
    match_id: int
    half: int
    phase: str
    active_team: str
    home_team: TeamState
    away_team: TeamState
    players: list[MatchPlayer]
    ball: BallState
    ai_team: Optional[str] = None

    @classmethod
    def from_dict(cls, data: dict) -> GameState:
        return cls(
            match_id=data['matchId'],
            half=data['half'],
            phase=data['phase'],
            active_team=data['activeTeam'],
            home_team=TeamState.from_dict(data['homeTeam']),
            away_team=TeamState.from_dict(data['awayTeam']),
            players=[MatchPlayer.from_dict(p) for p in data.get('players', [])],
            ball=BallState.from_dict(data.get('ball', {})),
            ai_team=data.get('aiTeam'),
        )

    def get_team_players(self, side: str) -> list[MatchPlayer]:
        return [p for p in self.players if p.team_side == side]

    def get_players_on_pitch(self, side: str) -> list[MatchPlayer]:
        on_pitch = {'standing', 'prone', 'stunned'}
        return [p for p in self.players if p.team_side == side and p.state in on_pitch]
