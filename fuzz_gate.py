#!/usr/bin/env python3
"""Fuzz the gating game to reproduce the engine hang.

Mirrors run_iteration._gate_game but runs each game in its own process with a
wall-clock deadline. Any game exceeding the deadline is recorded (seed + matchup)
and killed — that's a reproduced hang.
"""
from __future__ import annotations

import multiprocessing as mp
import random
import sys
import time
from pathlib import Path

ROOT = Path(__file__).parent.resolve()
sys.path.insert(0, str(ROOT / 'engine' / 'build'))
sys.path.insert(0, str(ROOT / 'python'))

RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']
GATE = str(ROOT / 'weights_az_train.json')      # the model that was being gated
FROZEN = str(ROOT / 'weights_frozen.json')
MCTS = 100
TV = 1200

DEADLINE = 240.0         # normal game ~50s; >240s == genuine infinite loop
CONCURRENCY = 12
TOTAL = int(sys.argv[1]) if len(sys.argv) > 1 else 1200


def play(seed: int, race_idx: int) -> None:
    import bb_engine
    hr = bb_engine.get_developed_roster(RACES[race_idx % len(RACES)], TV)
    ar = bb_engine.get_developed_roster(RACES[(race_idx + 1) % len(RACES)], TV)
    bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=MCTS,
        weights_path=GATE, away_weights_path=FROZEN,
        epsilon=0.0, vf_blend=0.0,
    )


def main() -> None:
    tasks = [(random.randint(1, 999999), i) for i in range(TOTAL)]
    running: dict[int, tuple] = {}   # idx -> (proc, seed, race_idx, start)
    hangs: list[tuple[int, int]] = []
    done = 0
    next_task = 0
    t0 = time.time()

    while done < TOTAL:
        # fill slots
        while len(running) < CONCURRENCY and next_task < TOTAL:
            seed, race_idx = tasks[next_task]
            p = mp.Process(target=play, args=(seed, race_idx), daemon=True)
            p.start()
            running[next_task] = (p, seed, race_idx, time.time())
            next_task += 1

        time.sleep(0.2)
        now = time.time()
        finished = []
        for idx, (p, seed, race_idx, start) in running.items():
            if not p.is_alive():
                dur = now - start
                if dur > 90:
                    print(f'  slow game seed={seed} race_idx={race_idx} took {dur:.0f}s', flush=True)
                finished.append(idx)
            elif now - start > DEADLINE:
                p.terminate()
                hangs.append((seed, race_idx))
                mu = f'{RACES[race_idx % 5]} vs {RACES[(race_idx+1) % 5]}'
                print(f'*** HANG: seed={seed} race_idx={race_idx} ({mu}) '
                      f'ran >{DEADLINE:.0f}s — KILLED', flush=True)
                finished.append(idx)
        for idx in finished:
            running.pop(idx)
            done += 1

        if done % 50 == 0 and done > 0 and not finished:
            pass
        if finished:
            print(f'  progress {done}/{TOTAL}  hangs={len(hangs)}  '
                  f'elapsed={now - t0:.0f}s', flush=True)

    print(f'\nDONE: {TOTAL} games, {len(hangs)} hangs in {time.time()-t0:.0f}s')
    for seed, race_idx in hangs:
        print(f'  HANG seed={seed} race_idx={race_idx}')


if __name__ == '__main__':
    main()
