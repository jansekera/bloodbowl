"""Run simulations via the C++ engine (bb_engine) for fast training."""
from __future__ import annotations

import json
import random
import sys
import time
from multiprocessing import Pool
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


def _pool_init(paths: list) -> None:
    for p in paths:
        if p not in sys.path:
            sys.path.insert(0, p)


def _simulate_game_worker(args: tuple) -> dict:
    """Worker for parallel game simulation — called in a child process."""
    (seed, home_race_name, away_race_name, home_ai, away_ai,
     weights_path, epsilon, mcts_iterations, policy_path, policy_blend,
     vf_blend, away_weights, log_dir_str, game_num, tv) = args

    import bb_engine

    hr = bb_engine.get_developed_roster(home_race_name, tv)
    ar = bb_engine.get_developed_roster(away_race_name, tv)

    logged = bb_engine.simulate_game_logged(
        hr, ar, home_ai, away_ai,
        seed=seed, weights_path=weights_path, epsilon=epsilon,
        mcts_iterations=mcts_iterations,
        policy_weights_path=policy_path,
        policy_blend=policy_blend,
        vf_blend=vf_blend,
        away_weights_path=away_weights,
    )
    result = logged.result

    if log_dir_str:
        log_path = Path(log_dir_str)
        log_path.mkdir(parents=True, exist_ok=True)

        winner = ('home' if result.home_score > result.away_score
                  else 'away' if result.away_score > result.home_score else None)
        log_file = log_path / f'game_{game_num:04d}.jsonl'
        with open(log_file, 'w') as f:
            # states and turn_logs are pushed 1:1 in the engine (game_simulator.cpp),
            # so zip gives the running score at each logged state -> per-TD step
            # reward (Lever B, rewards.episode_returns).
            states = logged.get_states()
            turns = logged.get_turn_logs()
            assert len(states) == len(turns), \
                f'state/turn log mismatch: {len(states)} vs {len(turns)}'
            for state, turn in zip(states, turns):
                f.write(json.dumps({
                    'type': 'state',
                    'features': state['features'].tolist(),
                    'perspective': state['perspective'],
                    'home_score': turn['home_score'],
                    'away_score': turn['away_score'],
                }) + '\n')
            f.write(json.dumps({
                'type': 'result',
                'home_score': result.home_score,
                'away_score': result.away_score,
                'winner': winner,
            }) + '\n')

        decisions = logged.get_policy_decisions()
        if decisions:
            dec_file = log_path / f'decisions_{game_num:04d}.json'
            with open(dec_file, 'w') as f:
                json.dump([
                    {
                        'state_features': d['state_features'].tolist(),
                        'perspective': d['perspective'],
                        'visits': [
                            {
                                'action_features': v['action_features'].tolist(),
                                'visit_fraction': float(v['visit_fraction']),
                            }
                            for v in d['visits']
                        ],
                    }
                    for d in decisions
                ], f)

    return {
        'home_score': result.home_score,
        'away_score': result.away_score,
        'total_actions': result.total_actions,
    }


class CPPRunner:
    """Run Blood Bowl simulations via bb_engine C++ module."""

    def __init__(self, project_root: Optional[str] = None):
        import bb_engine
        self.bb = bb_engine
        if project_root:
            self.project_root = Path(project_root)
        else:
            self.project_root = Path(__file__).parent.parent.parent
        self._sys_path = list(sys.path)

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
        game_offset: int = 0,
        policy_blend: float = 0.0,
        vf_blend: float = 0.0,
        workers: int = 1,
    ) -> TournamentResult:
        # Team value level for roster resolution (>=1200 fields developed/skilled rosters).
        self._tv = tv if tv else 1000
        if workers > 1 and matches > 1:
            return self._simulate_parallel(
                home_ai=home_ai, away_ai=away_ai, matches=matches,
                weights=weights, epsilon=epsilon, log_dir=log_dir,
                progress_callback=progress_callback,
                home_race=home_race, away_race=away_race,
                away_weights=away_weights, mcts_iterations=mcts_iterations,
                policy_weights=policy_weights, game_offset=game_offset,
                policy_blend=policy_blend, vf_blend=vf_blend, workers=workers,
                tv=self._tv,
            )

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
                    policy_blend=policy_blend,
                    vf_blend=vf_blend,
                    away_weights_path=away_weights or '',
                )
                result = logged.result

                # Write JSONL log for trainer
                if log_path:
                    log_path.mkdir(parents=True, exist_ok=True)
                    game_num = game_offset + i + 1
                    log_file = log_path / f'game_{game_num:04d}.jsonl'
                    self._write_log(log_file, logged, result)

                    # Write policy decisions if available
                    decisions = logged.get_policy_decisions()
                    if decisions:
                        dec_file = log_path / f'decisions_{game_num:04d}.json'
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

    def _simulate_parallel(
        self,
        home_ai: str, away_ai: str, matches: int,
        weights: Optional[str], epsilon: Optional[float],
        log_dir: Optional[str],
        progress_callback: Optional[Callable],
        home_race: Optional[str], away_race: Optional[str],
        away_weights: Optional[str], mcts_iterations: int,
        policy_weights: Optional[str], game_offset: int,
        policy_blend: float, vf_blend: float, workers: int,
        tv: int = 1000,
    ) -> TournamentResult:
        weights_path = weights or ''
        eps = epsilon if epsilon is not None else 0.3
        policy_path = policy_weights or ''
        cpp_home_ai = self._map_ai(home_ai)
        cpp_away_ai = self._map_ai(away_ai)

        tasks = [
            (
                random.randint(0, 2**31 - 1),
                self._resolve_race_name(home_race),
                self._resolve_race_name(away_race),
                cpp_home_ai, cpp_away_ai,
                weights_path, eps, mcts_iterations,
                policy_path, policy_blend, vf_blend,
                away_weights or '', log_dir or '', game_offset + i + 1,
                tv,
            )
            for i in range(matches)
        ]

        actual_workers = min(workers, matches)
        with Pool(actual_workers, initializer=_pool_init, initargs=(self._sys_path,)) as pool:
            game_results = pool.map(_simulate_game_worker, tasks)

        results: list[MatchResult] = []
        home_wins = away_wins = draws = 0
        for i, gr in enumerate(game_results):
            mr = MatchResult(
                home_score=gr['home_score'],
                away_score=gr['away_score'],
                total_actions=gr['total_actions'],
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
            if progress_callback:
                progress_callback(game_offset + i + 1, game_offset + matches, 0.0,
                                  f'{mr.home_score}-{mr.away_score}')

        return TournamentResult(
            home_ai=home_ai, away_ai=away_ai, matches=matches,
            home_wins=home_wins, away_wins=away_wins, draws=draws,
            results=results,
        )

    def _get_roster(self, race: Optional[str], tv: Optional[int] = None):
        tv = tv if tv is not None else getattr(self, '_tv', 1000)
        roster = self.bb.get_developed_roster(self._resolve_race_name(race), tv)
        if roster is None:
            raise ValueError(f'Unknown roster: {race}')
        return roster

    def _resolve_race_name(self, race: Optional[str]) -> str:
        if race is None or race == 'random':
            return random.choice(RACE_NAMES)
        return race

    def _map_ai(self, ai_name: str) -> str:
        """Map Python AI names to C++ engine AI names."""
        if ai_name in ('random', 'greedy', 'learning', 'mcts', 'macro_mcts'):
            return ai_name
        # Default to random for unknown types
        return 'random'

    def _write_log(self, path: Path, logged, result) -> None:
        """Write JSONL log compatible with Python trainer."""
        states = logged.get_states()
        turns = logged.get_turn_logs()
        assert len(states) == len(turns), \
            f'state/turn log mismatch: {len(states)} vs {len(turns)}'
        with open(path, 'w') as f:
            for state, turn in zip(states, turns):
                features = state['features'].tolist()
                perspective = state['perspective']
                record = {
                    'type': 'state',
                    'features': features,
                    'perspective': perspective,
                    'home_score': turn['home_score'],
                    'away_score': turn['away_score'],
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
