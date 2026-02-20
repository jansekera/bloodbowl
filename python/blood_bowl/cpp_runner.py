"""Run simulations via the C++ engine (bb_engine) for fast training."""
from __future__ import annotations

import json
import random
import time
from pathlib import Path
from typing import Callable, Optional

from .cli_runner import MatchResult, TournamentResult

# Race name mapping: Python/PHP names → C++ roster names
RACE_NAMES = [
    "human", "orc", "skaven", "dwarf", "wood-elf", "chaos",
    "undead", "lizardmen", "dark-elf", "halfling", "norse", "high-elf",
    "vampire", "amazon", "necromantic", "bretonnian", "khemri", "goblin",
    "chaos-dwarf", "ogre", "nurgle", "pro-elf", "slann", "underworld",
    "khorne", "chaos-pact",
]


class CPPRunner:
    """Run Blood Bowl simulations via bb_engine C++ module."""

    def __init__(self, project_root: Optional[str] = None):
        import bb_engine
        self.bb = bb_engine
        if project_root:
            self.project_root = Path(project_root)
        else:
            self.project_root = Path(__file__).parent.parent.parent

    def simulate(
        self,
        home_ai: str = 'greedy',
        away_ai: str = 'random',
        matches: int = 1,
        verbose: bool = False,
        timeout: int = 120,
        timeout_per_game: bool = True,
        weights: Optional[str] = None,
        epsilon: Optional[float] = None,
        log_dir: Optional[str] = None,
        progress_callback: Optional[Callable[[int, int, float, str], None]] = None,
        home_race: Optional[str] = None,
        away_race: Optional[str] = None,
        away_weights: Optional[str] = None,
        away_epsilon: Optional[float] = None,
        tv: Optional[int] = None,
        mcts_iterations: int = 0,
        policy_weights: Optional[str] = None,
    ) -> TournamentResult:
        results: list[MatchResult] = []
        home_wins = 0
        away_wins = 0
        draws = 0

        weights_path = weights or ''
        eps = epsilon if epsilon is not None else 0.3
        policy_path = policy_weights or ''

        log_path = Path(log_dir) if log_dir else None

        for i in range(matches):
            game_start = time.time()
            seed = random.randint(0, 2**31 - 1)

            # Resolve rosters
            hr = self._get_roster(home_race)
            ar = self._get_roster(away_race)

            # Map AI names for C++ engine
            cpp_home_ai = self._map_ai(home_ai)
            cpp_away_ai = self._map_ai(away_ai)

            if log_path or weights_path:
                logged = self.bb.simulate_game_logged(
                    hr, ar, cpp_home_ai, cpp_away_ai,
                    seed=seed, weights_path=weights_path, epsilon=eps,
                    mcts_iterations=mcts_iterations,
                    policy_weights_path=policy_path,
                )
                result = logged.result

                # Write JSONL log for trainer
                if log_path:
                    log_path.mkdir(parents=True, exist_ok=True)
                    log_file = log_path / f'game_{i + 1:04d}.jsonl'
                    self._write_log(log_file, logged, result)

                    # Write policy decisions if available
                    decisions = logged.get_policy_decisions()
                    if decisions:
                        dec_file = log_path / f'decisions_{i + 1:04d}.json'
                        self._write_decisions(dec_file, decisions)
            else:
                result = self.bb.simulate_game(
                    hr, ar, cpp_home_ai, cpp_away_ai, seed=seed,
                    mcts_iterations=mcts_iterations,
                )

            mr = MatchResult(
                home_score=result.home_score,
                away_score=result.away_score,
                total_actions=result.total_actions,
                phase='GAME_OVER',
                half=2,
            )
            results.append(mr)

            if mr.home_score > mr.away_score:
                home_wins += 1
            elif mr.away_score > mr.home_score:
                away_wins += 1
            else:
                draws += 1

            elapsed = time.time() - game_start
            if progress_callback:
                score = f'{mr.home_score}-{mr.away_score}'
                progress_callback(i + 1, matches, elapsed, score)

        return TournamentResult(
            home_ai=home_ai,
            away_ai=away_ai,
            matches=matches,
            home_wins=home_wins,
            away_wins=away_wins,
            draws=draws,
            results=results,
        )

    def _get_roster(self, race: Optional[str]):
        if race is None or race == 'random':
            name = random.choice(RACE_NAMES)
        else:
            name = race
        roster = self.bb.get_roster(name)
        if roster is None:
            raise ValueError(f'Unknown roster: {name}')
        return roster

    def _map_ai(self, ai_name: str) -> str:
        """Map Python AI names to C++ engine AI names."""
        if ai_name in ('random', 'greedy', 'learning', 'mcts', 'macro_mcts'):
            return ai_name
        # Default to random for unknown types
        return 'random'

    def _write_log(self, path: Path, logged, result) -> None:
        """Write JSONL log compatible with Python trainer."""
        states = logged.get_states()
        with open(path, 'w') as f:
            for state in states:
                features = state['features'].tolist()
                perspective = state['perspective']
                record = {
                    'type': 'state',
                    'features': features,
                    'perspective': perspective,
                }
                f.write(json.dumps(record) + '\n')

            # Final result record
            winner = None
            if result.home_score > result.away_score:
                winner = 'home'
            elif result.away_score > result.home_score:
                winner = 'away'

            result_record = {
                'type': 'result',
                'home_score': result.home_score,
                'away_score': result.away_score,
                'winner': winner,
            }
            f.write(json.dumps(result_record) + '\n')

    def _write_decisions(self, path: Path, decisions: list) -> None:
        """Write MCTS policy decisions for policy trainer."""
        serialized = []
        for dec in decisions:
            d = {
                'state_features': dec['state_features'].tolist(),
                'perspective': dec['perspective'],
                'visits': [
                    {
                        'action_features': v['action_features'].tolist(),
                        'visit_fraction': float(v['visit_fraction']),
                    }
                    for v in dec['visits']
                ],
            }
            serialized.append(d)
        with open(path, 'w') as f:
            json.dump(serialized, f)
