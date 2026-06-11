#!/usr/bin/env python3
"""Limited end-to-end verification of the TV1200 baseline iteration.

Runs ONE shrunk run_iteration (fixed engine + watchdog) to prove the full cycle
— self-play → benchmark → gating → force-promote — completes without hanging,
then RESTORES all mutated state so the real long run starts from the identical
clean baseline (86% base model, meta without tv → baseline_reset fires again).
"""
from __future__ import annotations

import glob
import os
import shutil
import time
from pathlib import Path

import run_iteration as ri

ROOT = ri.PROJECT_ROOT
MUT = [
    'weights_best.json', 'weights_best_meta.json', 'weights_frozen.json',
    'weights_az_train.json', 'weights_az_train_meta.json',
    'weights_train_best.json', 'replay_buffer.pkl', 'epoch_metrics.csv',
    'score_log.csv', 'training_results.csv',
]

bak = ROOT / '_verify_bak'
if bak.exists():
    shutil.rmtree(bak)
bak.mkdir()
for f in MUT:
    p = ROOT / f
    if p.exists():
        shutil.copy2(p, bak / f)
snaps_before = set(glob.glob(str(ROOT / 'weights_snap_*.json')))
print(f'Backed up {len(os.listdir(bak))} files → _verify_bak/', flush=True)

# Shrink the iteration so it finishes in minutes but still exercises every stage.
ri.EPOCHS = 2
ri.GAMES_PER_EPOCH = 10
ri.BENCHMARK_INTERVAL = 2
ri.BENCHMARK_MATCHES = 24
ri.GATING_MATCHES = 24

t0 = time.time()
result = None
err = None
try:
    result = ri.run_iteration(no_push=True)
except Exception as e:  # noqa: BLE001
    err = e
finally:
    print(f'\n--- restoring clean state (run took {time.time()-t0:.0f}s) ---', flush=True)
    for f in MUT:
        b = bak / f
        if b.exists():
            shutil.copy2(b, ROOT / f)
    removed = 0
    for s in glob.glob(str(ROOT / 'weights_snap_*.json')):
        if s not in snaps_before:
            os.remove(s)
            removed += 1
    shutil.rmtree(bak, ignore_errors=True)
    print(f'Restored {len(MUT)} files, removed {removed} new snapshots.', flush=True)

if err is not None:
    print(f'\n❌ VERIFY FAILED: {type(err).__name__}: {err}', flush=True)
    raise SystemExit(1)
promote, bm, chess = result
print(f'\n✅ VERIFY OK — full cycle completed without hang. '
      f'promote={promote} benchmark={bm:.1%} chess={chess:.1%}', flush=True)
print('State restored — your long run will warm-start from the 86% base model '
      'and baseline-reset will fire again.', flush=True)
