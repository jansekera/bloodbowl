"""Run simulations via the PHP CLI simulator."""
from __future__ import annotations

import json
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional


@dataclass
class MatchResult:
    home_score: int
    away_score: int
    total_actions: int
    phase: str
    half: int

    @property
    def winner(self) -> Optional[str]:
        if self.home_score > self.away_score:
            return 'home'
        elif self.away_score > self.home_score:
            return 'away'
        return None


@dataclass
class TournamentResult:
    home_ai: str
    away_ai: str
    matches: int
    home_wins: int
    away_wins: int
    draws: int
    results: list[MatchResult]

    @property
    def home_win_rate(self) -> float:
        return self.home_wins / self.matches if self.matches > 0 else 0.0

    @property
    def away_win_rate(self) -> float:
        return self.away_wins / self.matches if self.matches > 0 else 0.0


class CLIRunner:
    """Run Blood Bowl simulations via PHP CLI."""

    def __init__(self, project_root: Optional[str] = None):
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
    ) -> TournamentResult:
        cmd = [
            'php', str(self.project_root / 'cli' / 'simulate.php'),
            f'--home-ai={home_ai}',
            f'--away-ai={away_ai}',
            f'--matches={matches}',
        ]
        if verbose:
            cmd.append('--verbose')
        if weights is not None:
            cmd.append(f'--weights={weights}')
        if epsilon is not None:
            cmd.append(f'--epsilon={epsilon}')
        if log_dir is not None:
            cmd.append(f'--log={log_dir}')
        if home_race is not None:
            cmd.append(f'--home-race={home_race}')
        if away_race is not None:
            cmd.append(f'--away-race={away_race}')
        if away_weights is not None:
            cmd.append(f'--away-weights={away_weights}')
        if away_epsilon is not None:
            cmd.append(f'--away-epsilon={away_epsilon}')
        if tv is not None:
            cmd.append(f'--tv={tv}')

        effective_timeout = timeout * matches if timeout_per_game else timeout

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            cwd=str(self.project_root),
        )

        # Stream stderr for progress, collect stdout for JSON result
        stderr_lines = []
        stdout_data = ''

        import selectors
        sel = selectors.DefaultSelector()
        sel.register(proc.stdout, selectors.EVENT_READ)
        sel.register(proc.stderr, selectors.EVENT_READ)

        start_time = time.time()
        stdout_done = False
        stderr_done = False

        while not (stdout_done and stderr_done):
            elapsed = time.time() - start_time
            if elapsed > effective_timeout:
                proc.kill()
                raise subprocess.TimeoutExpired(cmd, effective_timeout)

            for key, _ in sel.select(timeout=1.0):
                data = key.fileobj.readline()
                if key.fileobj is proc.stdout:
                    if data:
                        stdout_data += data
                    else:
                        stdout_done = True
                elif key.fileobj is proc.stderr:
                    if data:
                        line = data.strip()
                        stderr_lines.append(line)
                        # Parse progress: GAME_DONE|current|total|elapsed|score
                        if line.startswith('GAME_DONE|') and progress_callback:
                            parts = line.split('|')
                            if len(parts) >= 5:
                                progress_callback(
                                    int(parts[1]), int(parts[2]),
                                    float(parts[3]), parts[4],
                                )
                    else:
                        stderr_done = True

        sel.close()
        proc.wait()

        if proc.returncode != 0:
            raise RuntimeError(f'Simulation failed: {chr(10).join(stderr_lines)}')

        data = json.loads(stdout_data)
        return TournamentResult(
            home_ai=data['homeAi'],
            away_ai=data['awayAi'],
            matches=data['matches'],
            home_wins=data['homeWins'],
            away_wins=data['awayWins'],
            draws=data['draws'],
            results=[
                MatchResult(
                    home_score=r['homeScore'],
                    away_score=r['awayScore'],
                    total_actions=r['totalActions'],
                    phase=r['phase'],
                    half=r['half'],
                )
                for r in data['results']
            ],
        )
