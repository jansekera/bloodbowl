"""REST API client for the Blood Bowl server."""
from __future__ import annotations

from typing import Optional

import requests

from .game_state import GameState


class BloodBowlClient:
    """HTTP client for the Blood Bowl API."""

    def __init__(self, base_url: str = 'http://localhost:8080', api_key: Optional[str] = None):
        self.base_url = base_url.rstrip('/')
        self.session = requests.Session()
        if api_key:
            self.session.headers['Authorization'] = f'Bearer {api_key}'

    def _api(self, path: str) -> str:
        return f'{self.base_url}/api/v1{path}'

    def create_match(self, home_team_id: int, away_team_id: int, vs_ai: bool = True) -> GameState:
        resp = self.session.post(self._api('/matches'), json={
            'homeTeamId': home_team_id,
            'awayTeamId': away_team_id,
            'vsAi': vs_ai,
        })
        resp.raise_for_status()
        return GameState.from_dict(resp.json()['data'])

    def get_state(self, match_id: int) -> GameState:
        resp = self.session.get(self._api(f'/matches/{match_id}/state'))
        resp.raise_for_status()
        return GameState.from_dict(resp.json()['data'])

    def submit_action(self, match_id: int, action: str, params: dict) -> dict:
        resp = self.session.post(self._api(f'/matches/{match_id}/actions'), json={
            'action': action,
            **params,
        })
        resp.raise_for_status()
        data = resp.json()
        return {
            'state': GameState.from_dict(data['state']),
            'events': data.get('events', []),
            'success': data.get('success', True),
            'turnover': data.get('turnover', False),
        }

    def get_available_actions(self, match_id: int) -> list[dict]:
        resp = self.session.get(self._api(f'/matches/{match_id}/actions'))
        resp.raise_for_status()
        return resp.json().get('data', [])

    def get_move_targets(self, match_id: int, player_id: int) -> list[dict]:
        resp = self.session.get(self._api(f'/matches/{match_id}/players/{player_id}/moves'))
        resp.raise_for_status()
        return resp.json().get('data', [])

    def get_block_targets(self, match_id: int, player_id: int) -> list[dict]:
        resp = self.session.get(self._api(f'/matches/{match_id}/players/{player_id}/block-targets'))
        resp.raise_for_status()
        return resp.json().get('data', [])
