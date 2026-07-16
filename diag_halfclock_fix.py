#!/usr/bin/env python3
"""Smoke/decisive test (2026-07-07) for audit finding 2 (Fable 5 fresh-eyes
pass, 2026-07-03, project_bloodbowl_audit_findings_20260703 -- the biggest,
riskiest finding in that batch): every touchdown reset BOTH teams'
turnNumber to 0 (and rerolls to 3) via setupHalf(), so a "half" was
effectively 8 turns *since the last score*, not 8 turns total -- unbounded
game length, and the conceding team always got a fresh 8 turns to
equalize.

Fixed: split setupHalf's internals into setupHalf (true half boundaries:
game start, half-time -- unchanged behavior) and a new setupDrive
(post-touchdown restart -- re-places players/ball but does NOT reset the
turn clock or reroll pool). Both TOUCHDOWN branches in game_simulator.cpp
now call setupDrive instead of setupHalf.

This is a structural change to game-length dynamics -- per the audit's own
explicit warning, it invalidates every historical gating/benchmark
draw-rate baseline. This script does NOT compare against the old ~42-50%
reference numbers as if they were still meaningful; it reports the raw
result and total-actions/turn-count distribution so the next session can
judge fresh, and additionally reports how many games hit MAX_ACTIONS
(5000) as a red flag if that's now happening more (it shouldn't -- the fix
should make games shorter and more decisive, not longer).

Mirror match (champion vs itself, weights_best.json, vf_blend=0, MCTS=100),
policy_path=weights_policy.json (production default). Reuses production
_gate_game/_imap_watchdog for the win/draw/loss tally, plus a small direct
sample via simulate_game_logged for the totalActions/turn-count sanity
check (separate from the pooled gate games, run single-process to keep it
simple and fast).
"""
import random
import sys
from pathlib import Path
from multiprocessing import Pool

sys.path.insert(0, "python"); sys.path.insert(0, "engine/build")
from run_iteration import _gate_game, _imap_watchdog, _pool_init

N = int(sys.argv[1]) if len(sys.argv) > 1 else 20
W = "weights_best.json"
POLICY_PATH = "weights_policy.json"
TV = 1000
VF_BLEND = 0.0
MCTS_ITERS = 100
WORKERS = 12
init_args = ("engine/build", "python")

if not Path(POLICY_PATH).exists():
    print(f"ERROR: {POLICY_PATH} not found in cwd -- run from repo root.", file=sys.stderr)
    sys.exit(1)

# --- Part 1: small direct sample for game-length / turn-count sanity ---
print("\n--- game-length sanity sample (n=5, single-process, direct simulate_game_logged) ---", flush=True)
import bb_engine
hr = bb_engine.get_developed_roster("Human", TV)
ar = bb_engine.get_developed_roster("Orc", TV)
max_actions_hits = 0
for seed in range(5):
    r = bb_engine.simulate_game_logged(
        hr, ar, home_ai="mcts_macro", away_ai="mcts_macro",
        seed=seed, mcts_iterations=MCTS_ITERS, policy_weights_path=POLICY_PATH,
    )
    turns = len(r.get_turn_logs())
    total_actions = r.result.total_actions
    if total_actions >= 5000:
        max_actions_hits += 1
    print(f"  seed={seed} totalActions={total_actions} turnLogs={turns} "
          f"score={r.result.home_score}-{r.result.away_score}", flush=True)
print(f"  MAX_ACTIONS(5000) hit: {max_actions_hits}/5 -- should be 0, flag if >0", flush=True)

# --- Part 2: pooled gate-style win/draw/loss tally ---
print(f"\n--- starting MIRROR gating MCTS={MCTS_ITERS} policy_path={POLICY_PATH!r} "
      f"(+ half-clock fix) n={N} ---", flush=True)
tasks = [
    (random.randint(1, 999999), i, W, W, MCTS_ITERS, VF_BLEND, TV, False, POLICY_PATH)
    for i in range(N)
]
wins = draws = losses = 0
done = 0
with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
    for hs, as_ in _imap_watchdog(pool, _gate_game, tasks, 'halfclock-fix',
                                  mcts_iterations=MCTS_ITERS):
        done += 1
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        if done % 10 == 0 or done == N:
            total = wins + draws + losses
            print(f"  {done}/{N} done -- so far {wins}W {draws}D {losses}L "
                  f"= {100*draws/total:.1f}% draws", flush=True)

total = wins + draws + losses
print(f"\n=== MIRROR gate champion vs champion  halfclock-fix  n={total} (of {N} requested) ===", flush=True)
print(f"{wins}W {draws}D {losses}L = {100*draws/total:.1f}% draws, "
      f"{100*wins/total:.1f}% home win, {100*losses/total:.1f}% home loss", flush=True)
print("  NOTE: do NOT compare this number against pre-fix references (42.6%/49.7%/etc.) "
      "as if directly comparable -- this fix changes game-length dynamics structurally, "
      "per the audit's own explicit warning. Treat this as a FRESH baseline.", flush=True)
