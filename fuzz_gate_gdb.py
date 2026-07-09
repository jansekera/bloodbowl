#!/usr/bin/env python3
"""Fuzz the gating game to reproduce the engine hang, and gdb-attach a live
backtrace when a game overruns instead of just killing it (fuzz_gate.py's
behaviour).

Same matchup config as fuzz_gate.py / the gate phase that aborted promotion
on 2026-07-08 (weights_az_train vs weights_frozen, MCTS=100, TV=1200,
developed rosters across the 5 races) -- but points at engine/build_dbg
(a RelWithDebInfo, unstripped build of the *same current source*) so gdb
backtraces have function names and line numbers.

Mechanism: each worker calls prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY)
before running the game, which is required under Yama ptrace_scope=1 (the
default in this sandbox) to let a *non-ancestor* process (this script,
which spawns games as siblings via multiprocessing, and gdb as another
sibling subprocess) attach with ptrace. Without this, gdb attach fails with
"Could not attach to process" even for our own child.

Once a game has been alive past SAMPLE_START seconds, we gdb-attach up to
SAMPLE_COUNT times (a few seconds apart) with `thread apply all bt`, to
confirm the spin is stable in one place (not just slow). After sampling we
let the process keep running until DEADLINE, then kill it as a genuine hang
(matches fuzz_gate.py's classification).
"""
from __future__ import annotations

import ctypes
import glob
import multiprocessing as mp
import os
import random
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).parent.resolve()
BUILD_DIR = ROOT / 'engine' / 'build_dbg'   # debug build w/ symbols
sys.path.insert(0, str(BUILD_DIR))
sys.path.insert(0, str(ROOT / 'python'))

RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']
GATE = str(ROOT / 'weights_az_train.json')
FROZEN = str(ROOT / 'weights_frozen.json')
MCTS = 100
TV = 1200

SAMPLE_START = 90.0       # start gdb-sampling once a game is alive this long
SAMPLE_COUNT = 3
SAMPLE_GAP = 5.0
DEADLINE = 240.0
CONCURRENCY = 6           # lower than fuzz_gate.py -- gdb attach is heavier
TOTAL = int(sys.argv[1]) if len(sys.argv) > 1 else 400

PR_SET_PTRACER = 0x59616d61
PR_SET_PTRACER_ANY = 0xFFFFFFFFFFFFFFFF

# --- gdb (vendored under scratchpad; no system gdb in this sandbox) -----
GDB_ROOT = Path('/tmp/claude-1000/-home-jan-claude/10efd1d3-2a2a-4ead-a8fa-d8671e7b8729'
                 '/scratchpad/gdb_extract')
GDB_BIN = str(GDB_ROOT / 'usr' / 'bin' / 'gdb')
GDB_LIBDIRS = sorted({str(Path(p).parent) for p in
                       glob.glob(str(GDB_ROOT / '**' / '*.so*'), recursive=True)})
GDB_ENV = dict(os.environ)
GDB_ENV['LD_LIBRARY_PATH'] = ':'.join(GDB_LIBDIRS + [GDB_ENV.get('LD_LIBRARY_PATH', '')])


def _allow_any_ptracer() -> None:
    libc = ctypes.CDLL("libc.so.6", use_errno=True)
    libc.prctl(PR_SET_PTRACER, ctypes.c_ulong(PR_SET_PTRACER_ANY), 0, 0, 0)


def play(seed: int, race_idx: int) -> None:
    _allow_any_ptracer()
    import bb_engine
    hr = bb_engine.get_developed_roster(RACES[race_idx % len(RACES)], TV)
    ar = bb_engine.get_developed_roster(RACES[(race_idx + 1) % len(RACES)], TV)
    bb_engine.simulate_game_logged(
        hr, ar, home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=MCTS,
        weights_path=GATE, away_weights_path=FROZEN,
        epsilon=0.0, vf_blend=0.0,
    )


def gdb_bt(pid: int) -> str:
    try:
        out = subprocess.run(
            [GDB_BIN, '-p', str(pid), '-batch', '-ex', 'thread apply all bt'],
            env=GDB_ENV, capture_output=True, text=True, timeout=30,
        )
        return out.stdout + out.stderr
    except Exception as e:  # noqa: BLE001
        return f'<gdb attach failed: {e}>'


def main() -> None:
    if not os.path.exists(GDB_BIN):
        print(f'ERROR: vendored gdb not found at {GDB_BIN}', file=sys.stderr)
        sys.exit(1)

    tasks = [(random.randint(1, 999999), i) for i in range(TOTAL)]
    # idx -> (proc, seed, race_idx, start, samples_taken, next_sample_at)
    running: dict[int, list] = {}
    hangs: list[tuple[int, int]] = []
    done = 0
    next_task = 0
    t0 = time.time()

    while done < TOTAL:
        while len(running) < CONCURRENCY and next_task < TOTAL:
            seed, race_idx = tasks[next_task]
            p = mp.Process(target=play, args=(seed, race_idx), daemon=True)
            p.start()
            running[next_task] = [p, seed, race_idx, time.time(), 0, None]
            next_task += 1

        time.sleep(0.5)
        now = time.time()
        finished = []
        for idx, rec in running.items():
            p, seed, race_idx, start, samples_taken, next_sample_at = rec
            alive = p.is_alive()
            dur = now - start

            if alive and dur > SAMPLE_START and samples_taken < SAMPLE_COUNT:
                if next_sample_at is None or now >= next_sample_at:
                    mu = f'{RACES[race_idx % 5]} vs {RACES[(race_idx + 1) % 5]}'
                    print(f'\n=== gdb sample {samples_taken + 1}/{SAMPLE_COUNT} '
                          f'seed={seed} race_idx={race_idx} ({mu}) pid={p.pid} '
                          f'alive {dur:.0f}s ===', flush=True)
                    bt = gdb_bt(p.pid)
                    print(bt, flush=True)
                    rec[4] += 1
                    rec[5] = now + SAMPLE_GAP

            if not alive:
                if dur > 90:
                    print(f'  slow game seed={seed} race_idx={race_idx} took {dur:.0f}s',
                          flush=True)
                finished.append(idx)
            elif dur > DEADLINE:
                p.terminate()
                hangs.append((seed, race_idx))
                mu = f'{RACES[race_idx % 5]} vs {RACES[(race_idx + 1) % 5]}'
                print(f'*** HANG: seed={seed} race_idx={race_idx} ({mu}) '
                      f'ran >{DEADLINE:.0f}s — KILLED (samples_taken={rec[4]})', flush=True)
                finished.append(idx)

        for idx in finished:
            running.pop(idx)
            done += 1

        if finished:
            print(f'  progress {done}/{TOTAL}  hangs={len(hangs)}  '
                  f'elapsed={now - t0:.0f}s', flush=True)

    print(f'\nDONE: {TOTAL} games, {len(hangs)} hangs in {time.time() - t0:.0f}s')
    for seed, race_idx in hangs:
        print(f'  HANG seed={seed} race_idx={race_idx}')


if __name__ == '__main__':
    main()
