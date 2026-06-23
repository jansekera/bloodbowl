#!/usr/bin/env python3
"""AlphaZero iteration: freeze → self-play → gate → promote.

Equivalent to Colab Cell 3, runs locally or on server without Colab.

Usage:
    python3 run_iteration.py              # 1 iteration
    python3 run_iteration.py --loop 5    # 5 iterations
    python3 run_iteration.py --no-push   # skip git push
"""
from __future__ import annotations

import argparse
import json
import math
import os
import random
import shutil
import subprocess
import sys
import time
from multiprocessing import Pool, TimeoutError as MPTimeoutError
from pathlib import Path

# ─── Configuration ────────────────────────────────────────────────────────────
EPOCHS = 16
GAMES_PER_EPOCH = 40
HOME_RACE = 'human'
AWAY_RACE = 'orc,skaven,dwarf,wood-elf'
MCTS_ITERATIONS = 100
EPSILON_START = 0.35
EPSILON_END = 0.10
BENCHMARK_INTERVAL = 16
BENCHMARK_MATCHES = 400
LR = 0.0003
# VF_BLEND: experiment s 0.3 (commit caa99da) selhal — benchmark klesl na 76.0% a VF inversion
# v epochách 6, 7, 10 (oba avg VF pozitivní = MCTS dostával špatný signál). Vráceno na 0.0.
VF_BLEND = 0.0
VF_RAMP_EPOCHS = 10
GATING_MATCHES = 600  # bumped 400→600 (2026-06-15): chess gate is decisive-only,
                      # ~75% her končí remízou → víc her = víc rozhodnutých = míň šumu
BM_DROP_LIMIT = 0.05
BM_FLOOR = 0.77

# Smoke-test přepisy přes env (defaulty pro normální běh beze změny).
EPOCHS = int(os.environ.get('BB_EPOCHS', EPOCHS))
GAMES_PER_EPOCH = int(os.environ.get('BB_GAMES', GAMES_PER_EPOCH))
MCTS_ITERATIONS = int(os.environ.get('BB_MCTS', MCTS_ITERATIONS))
BENCHMARK_MATCHES = int(os.environ.get('BB_BM', BENCHMARK_MATCHES))
GATING_MATCHES = int(os.environ.get('BB_GATE', GATING_MATCHES))
# Team1 v2 validation knobs (defaulty = původní chování beze změny):
VF_BLEND = float(os.environ.get('BB_VF_BLEND', VF_BLEND))
POLICY_BLEND = float(os.environ.get('BB_POLICY_BLEND', 0.0))
IMITATION_EPOCHS = int(os.environ.get('BB_IMITATION_EPOCHS', 16))
TRAINING_METHOD = os.environ.get('BB_TRAINING_METHOD', 'mc_shaped')
# Gate dual-signal: požadovaná HtH výhra = 0.5 + k·σ, σ=0.5/√rozhodnuté.
# k podle benchmarku vs all-time-best (zlepšen/~stejný/klesl). Nahradilo pevné
# ANTI_REGRESSION=0.51, které leželo uvnitř šumu (mince). Viz Step 5.
GATE_SIGMA_IMPROVED = 1.0   # benchmark ≥ best
GATE_SIGMA_SAME = 1.5       # benchmark do 2 % pod best
GATE_SIGMA_DROPPED = 2.0    # benchmark 2–5 % pod best (>5 % = HARD-REJECT)
OPPONENT_MIX_RATIO = 0.5
MODEL = 'neural'
HIDDEN_SIZE = 64
# TV=1200 fields developed (skilled) rosters: goblins removed from Orc, Guard density,
# Strip Ball ball-hunter blitzer, Sure Feet gutter runners. tv<1200 = base rosters.
TV = 1200
WORKERS = min(12, os.cpu_count() or 1)
# Watchdog: a healthy macro_mcts vs macro_mcts game finishes in ~50s and with
# WORKERS in flight a result lands every few seconds. If NOTHING completes in
# this window a worker is wedged (e.g. an engine infinite loop) — abort the pool
# and log it instead of hanging the whole run forever, as happened 2026-06-10
# when the anti-regression pool froze at 150/400.
STALL_TIMEOUT = 300
# ──────────────────────────────────────────────────────────────────────────────

PROJECT_ROOT = Path(__file__).parent.resolve()


def _atomic_write_json(path: Path, obj) -> None:
    """Write JSON via temp file + os.replace so a kill mid-write can't leave a
    truncated/corrupt file (the corrupt-meta feeder behind the abort path)."""
    tmp = path.with_name(path.name + '.tmp')
    with open(tmp, 'w') as f:
        json.dump(obj, f)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def _imap_watchdog(pool: Pool, fn, tasks: list, label: str):
    """Yield results from pool.imap_unordered with a stall watchdog.

    If no result arrives within STALL_TIMEOUT seconds the pool is terminated and
    iteration stops (partial results), turning a permanent hang into a logged,
    recoverable event. Callers must compute scores over the yielded count.
    """
    n = len(tasks)
    it = pool.imap_unordered(fn, tasks)
    got = 0
    while got < n:
        try:
            r = it.next(timeout=STALL_TIMEOUT)
        except MPTimeoutError:
            print(f'  ⚠ WATCHDOG: {label} stalled — no game finished in '
                  f'{STALL_TIMEOUT}s at {got}/{n}. Aborting pool (engine hang?). '
                  f'Inspect with fuzz_gate.py.', flush=True)
            pool.terminate()
            return
        got += 1
        yield r

_RACES = ['human', 'orc', 'skaven', 'dwarf', 'wood-elf']


def _pool_init(engine_build: str, python_src: str) -> None:
    if engine_build not in sys.path:
        sys.path.insert(0, engine_build)
    if python_src not in sys.path:
        sys.path.insert(0, python_src)


def _benchmark_game(args: tuple) -> bool:
    seed, race_idx, gate_path, mcts_iterations, vf_blend, tv = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='random',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, epsilon=0.0, vf_blend=vf_blend,
    ).result
    return result.home_score > result.away_score


def _gate_game(args: tuple) -> tuple[int, int]:
    seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv = args
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, away_weights_path=frozen_path,
        epsilon=0.0, vf_blend=vf_blend,
    ).result
    return result.home_score, result.away_score


def _carry_over_policy(az_path: Path, best_path: Path, policy_path: Path) -> None:
    """Přenese uloženou policy hlavu do az_train PŘED tréninkem.

    BUG FIX (2026-06-18): copy best→az_train (níže) je value-only ('neural' plain),
    takže load_combined_weights spadne do legacy větve a neural policy startuje z
    RANDOM každou iteraci → nic se nekumuluje. Tady value bereme z best (frozen,
    gating záměr), ale policy hlavu přeneseme z minulé iterace, takže se policy
    učí napříč iteracemi. Viz paměť project-neural-policy-rootcause.
    """
    if not policy_path.exists():
        return  # 1. iterace: žádná uložená policy → trénuje se z random (OK)
    with open(policy_path) as f:
        pol = json.load(f)
    if 'policy_W1' not in pol:
        return
    with open(best_path) as f:
        val = json.load(f)
    data: dict = {'type': 'alphazero_neural'}
    if str(val.get('type', '')).startswith('alphazero'):
        for k in ('hidden_size', 'n_features', 'value_W1', 'value_b1', 'value_W2', 'value_b2'):
            if k in val:
                data[k] = val[k]
    else:  # plain 'neural' value: W1/b1/W2/b2 → value_*
        data['hidden_size'] = val['hidden_size']
        data['n_features'] = val['n_features']
        data['value_W1'] = val['W1']
        data['value_b1'] = val['b1']
        data['value_W2'] = val['W2']
        data['value_b2'] = val['b2']
    for k in ('policy_type', 'policy_hidden_size', 'policy_W1', 'policy_b1',
              'policy_W2', 'policy_b2', 'policy_temperature'):
        if k in pol:
            data[k] = pol[k]
    with open(az_path, 'w') as f:
        json.dump(data, f)
    print('Policy carry-over: policy hlava z předchozí iterace vložena do az_train', flush=True)


def _stash_policy(az_path: Path, policy_path: Path) -> None:
    """Uloží vytrénovanou policy hlavu z az_train pro další iteraci."""
    with open(az_path) as f:
        data = json.load(f)
    if 'policy_W1' not in data:
        return
    pol = {k: data[k] for k in ('policy_type', 'policy_hidden_size', 'policy_W1',
                                'policy_b1', 'policy_W2', 'policy_b2', 'policy_temperature')
           if k in data}
    with open(policy_path, 'w') as f:
        json.dump(pol, f)
    print('Policy stash: vytrénovaná policy hlava uložena pro další iteraci', flush=True)


def run_iteration(no_push: bool = False) -> tuple[bool, float | None, float]:
    os.chdir(str(PROJECT_ROOT))

    # Ensure bb_engine is importable
    engine_build = PROJECT_ROOT / 'engine' / 'build'
    python_src = PROJECT_ROOT / 'python'
    for p in (str(engine_build), str(python_src)):
        if p not in sys.path:
            sys.path.insert(0, p)

    try:
        import bb_engine  # noqa: F401
    except ImportError:
        print('ERROR: bb_engine not found. Run ./setup.sh first to build the C++ engine.')
        sys.exit(1)
    import bb_engine

    best_path = PROJECT_ROOT / 'weights_best.json'
    frozen_path = PROJECT_ROOT / 'weights_frozen.json'
    az_train_path = PROJECT_ROOT / 'weights_az_train.json'
    train_best_path = PROJECT_ROOT / 'weights_train_best.json'

    # baseline_reset: set when the TV (roster set) changed since the frozen
    # model was benchmarked — its win rate is on a different game and not comparable.
    baseline_reset = False
    # abort_promote: hard failures (corrupt meta / hung engine) where we must
    # neither promote nor push — keep the current best untouched. Overrides
    # baseline_reset's force-promote.
    abort_promote: list[str] = []
    # Step 1: Freeze current best
    if best_path.exists():
        shutil.copy2(str(best_path), str(frozen_path))
        print('Frozen: weights_best.json → weights_frozen.json')
        try:
            meta_path = PROJECT_ROOT / 'weights_best_meta.json'
            if meta_path.exists():
                with open(meta_path) as f:
                    meta = json.load(f)
                frozen_bm = meta.get('benchmark_win_rate', 0.0)
                all_time_best_bm = meta.get('all_time_best_benchmark', frozen_bm)
                meta_tv = meta.get('tv', 1000)
            else:
                with open(best_path) as f:
                    data = json.load(f)
                frozen_bm = data.get('benchmark_win_rate', 0.0) if isinstance(data, dict) else 0.0
                all_time_best_bm = frozen_bm
                meta_tv = 1000
            if meta_tv != TV:
                print(f'⚠ Změna TV {meta_tv}→{TV}: gating baseline RESET — předchozí '
                      f'benchmark {frozen_bm:.1%} byl na jiných rosterech a neplatí. '
                      f'První TV{TV} model se promotne jako nový baseline.', flush=True)
                frozen_bm = 0.0
                all_time_best_bm = 0.0
                baseline_reset = True
            print(f'Frozen benchmark: {frozen_bm:.1%}')
        except Exception as e:
            # Corrupt/unreadable meta while weights exist: the baseline is
            # unknown. Do NOT silently zero it and proceed — that disabled the
            # regression gates and could overwrite all_time_best with a lower
            # score (sibling of the _git_push meta bug). Keep current best,
            # skip promote+push this iteration.
            print(f'⛔ weights_best_meta.json nečitelné ({e!r}) — ponechávám '
                  f'současný best, přeskakuji promote+push.', flush=True)
            frozen_bm = 0.0
            all_time_best_bm = 0.0
            abort_promote.append(f'corrupt meta read: {e!r}')
    else:
        with open(best_path, 'w') as f:
            json.dump({'type': 'alphazero_neural', 'value_weights': [0.0] * 70}, f)
        shutil.copy2(str(best_path), str(frozen_path))
        frozen_bm = 0.0
        all_time_best_bm = 0.0
        print('First run: created fresh weights')

    if abort_promote:
        print('⛔ ABORT iterace (před tréninkem): ' + '; '.join(abort_promote), flush=True)
        shutil.copy2(str(frozen_path), str(best_path))  # best == frozen, beze změny
        return False, None, 0.0

    shutil.copy2(str(best_path), str(az_train_path))
    policy_cache_path = PROJECT_ROOT / 'weights_policy.json'
    _carry_over_policy(az_train_path, best_path, policy_cache_path)

    # Step 2: Self-play training
    print(f'\n=== Self-play training ({EPOCHS} epochs x {GAMES_PER_EPOCH} games) ===')
    env = os.environ.copy()
    env['PYTHONPATH'] = f'{engine_build}:{python_src}'

    cmd = [
        sys.executable, '-m', 'blood_bowl.train_cli',
        f'--epochs={EPOCHS}',
        f'--games={GAMES_PER_EPOCH}',
        '--use-cpp',
        '--opponent=learning', '--self-play',
        f'--home-race={HOME_RACE}', f'--away-race={AWAY_RACE}',
        f'--mcts-iterations={MCTS_ITERATIONS}',
        f'--lr={LR}', f'--model={MODEL}', f'--hidden-size={HIDDEN_SIZE}',
        f'--vf-blend={VF_BLEND}', f'--vf-ramp-epochs={VF_RAMP_EPOCHS}',
        # AZ bring-up krok 1 (NEURAL varianta): imitation-only. Lineární policy plató
        # na top1≈38 % / loss≈2.24 (kapacita + linear ignoruje passes, bug #5) →
        # přechod na neural. Neural RESPEKTUJE passes (policy_trainer.py:175), sdílí
        # --hidden-size=64 (na C++ stropu min(H,64), bez ořezu). Blend=0 po CELÝ běh
        # (imitation-epochs=16) → search bere heuristické priory, value se trénuje
        # stejně. Cíl: prolomí neural plató 38 %? klesá loss? drží value (~89 %)?
        # Blend až krok 5. Viz team_neural_policy_brief.md + paměť project-bloodbowl.
        '--policy-lr=0.01',
        '--policy-model=neural',
        f'--policy-blend={POLICY_BLEND}',
        f'--imitation-epochs={IMITATION_EPOCHS}',
        '--weights=weights_az_train.json',
        f'--tv={TV}',
        f'--training-method={TRAINING_METHOD}',
        f'--epsilon-start={EPSILON_START}', f'--epsilon-end={EPSILON_END}',
        f'--benchmark-interval={BENCHMARK_INTERVAL}', f'--benchmark-matches={BENCHMARK_MATCHES}',
        '--skip-greedy-benchmark', '--timeout=300',
        f'--opponent-mix-ratio={OPPONENT_MIX_RATIO}',
        f'--workers={WORKERS}',
    ]
    subprocess.run(cmd, env=env, cwd=str(PROJECT_ROOT), check=True)
    _stash_policy(az_train_path, policy_cache_path)

    # Step 3: Benchmark az_train (final epoch) AND train_best (best self-play epoch),
    # then gate on whichever scores higher vs random. Each gets BENCHMARK_MATCHES/2 games.
    print('\n=== Gating ===', flush=True)
    init_args = (str(engine_build), str(python_src))
    half_bm = BENCHMARK_MATCHES // 2

    def _run_benchmark(path: Path, label: str) -> tuple[float, bool]:
        tasks = [
            (random.randint(1, 999999), i, str(path), MCTS_ITERATIONS, VF_BLEND, TV)
            for i in range(half_bm)
        ]
        print(f'Benchmark {label}: {half_bm} games ({WORKERS} workers)...', flush=True)
        results: list[bool] = []
        with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
            for result in _imap_watchdog(pool, _benchmark_game, tasks, label):
                results.append(result)
                done = len(results)
                if done % 20 == 0 or done == half_bm:
                    print(f'  {label} {done}/{half_bm}: {sum(results)/done:.1%}', flush=True)
        score = sum(results) / len(results) if results else 0.0
        complete = len(results) >= half_bm
        print(f'  {label} final: {score:.1%} ({sum(results)}/{len(results)})', flush=True)
        return score, complete

    bm_az, az_complete = _run_benchmark(az_train_path, 'az_train')
    if train_best_path.exists():
        bm_tb, tb_complete = _run_benchmark(train_best_path, 'train_best')
    else:
        bm_tb, tb_complete = 0.0, True
        print('train_best not found, using az_train', flush=True)
    if not (az_complete and tb_complete):
        abort_promote.append('benchmark incomplete (engine hang? — viz WATCHDOG výše)')

    if bm_tb > bm_az:
        gate_path = train_best_path
        new_bm = bm_tb
        gate_label = f'train_best ({new_bm:.1%}) > az_train ({bm_az:.1%})'
    else:
        gate_path = az_train_path
        new_bm = bm_az
        gate_label = f'az_train ({new_bm:.1%}) >= train_best ({bm_tb:.1%})'

    print(f'Gating on: {gate_label}', flush=True)
    print(f'Benchmark: new={new_bm:.1%}  best={frozen_bm:.1%}  all_time_best={all_time_best_bm:.1%}  (max pokles {BM_DROP_LIMIT:.0%})', flush=True)

    # Step 4: Anti-regression gating games (winner vs frozen)
    gate_tasks = [
        (random.randint(1, 999999), i, str(gate_path), str(frozen_path), MCTS_ITERATIONS, VF_BLEND, TV)
        for i in range(GATING_MATCHES)
    ]
    print(f'Anti-regression: {GATING_MATCHES} games ({WORKERS} workers)...', flush=True)
    gate_results: list[tuple[int, int]] = []
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for hs, as_ in _imap_watchdog(pool, _gate_game, gate_tasks, 'Anti-regression'):
            gate_results.append((hs, as_))
            done = len(gate_results)
            if done % 10 == 0 or done == GATING_MATCHES:
                print(f'  Anti-regression {done}/{GATING_MATCHES}', flush=True)

    wins = draws = losses = 0
    for i, (hs, as_) in enumerate(gate_results):
        if hs > as_:
            wins += 1
        elif hs == as_:
            draws += 1
        else:
            losses += 1
        print(f'  Game {i + 1}: {hs}-{as_}', flush=True)

    total = wins + draws + losses
    # Decisive-only chess score: při ~75% remíz formule (W+0.5D)/N stlačí skóre
    # k 50% a práh ANTI_REGRESSION padne dovnitř šumového pásma (mince). Skórujeme
    # jen rozhodnuté hry → statistická síla. Remízy nenesou signál o síle modelu.
    decisive = wins + losses
    chess_score = wins / decisive if decisive else 0.5
    print(f'New vs Frozen: {wins}W {draws}D {losses}L = {chess_score:.1%} decisive '
          f'({decisive} decisive / {total} games)', flush=True)
    if total < GATING_MATCHES:
        abort_promote.append(f'anti-regression incomplete ({total}/{GATING_MATCHES} — engine hang?)')

    # A hard failure during benchmark/gating means the gate scores are based on
    # partial data — never promote (or push) on that. Overrides baseline_reset.
    if abort_promote:
        print('⛔ ABORT promote: ' + '; '.join(abort_promote), flush=True)
        print('   Ponechávám weights_best.json beze změny, nepushuju.', flush=True)
        shutil.copy2(str(frozen_path), str(best_path))
        return False, new_bm, chess_score

    # Step 5: Gate decision — dual-signal (head-to-head vs frozen + benchmark vs best).
    # Head-to-head je PRIMÁRNÍ signál (přímé párové srovnání, nedá se ošálit šťastným
    # benchmark hodem). Benchmark vs all-time-best OHÝBÁ požadovanou HtH laťku: čím
    # hůř na benchmarku, tím přesvědčivější HtH výhru vyžadujeme. Práh = 0,5 + k·σ,
    # kde σ = 0,5/√rozhodnuté_hry (šum 50/50 mince). Víc rozhodnutých her → nižší
    # laťka. Nahrazuje pevné ANTI_REGRESSION=0.51 (bylo uvnitř šumu → mince).
    # Viz paměť project-gating-redesign.
    promote = True
    reasons: list[str] = []

    sigma = 0.5 / math.sqrt(decisive) if decisive else 0.5

    if all_time_best_bm <= 0:
        # První baseline / neznámý best — žádná benchmark reference, jen mírná HtH laťka.
        k, tier = GATE_SIGMA_IMPROVED, 'bez reference'
    elif new_bm >= all_time_best_bm:
        k, tier = GATE_SIGMA_IMPROVED, 'benchmark zlepšen'
    elif new_bm > all_time_best_bm - 0.02:
        k, tier = GATE_SIGMA_SAME, 'benchmark ~stejný'
    elif new_bm > all_time_best_bm - BM_DROP_LIMIT:
        k, tier = GATE_SIGMA_DROPPED, 'benchmark klesl'
    else:
        k, tier = None, 'benchmark propadl'

    if k is None:
        promote = False
        reasons.append(f'benchmark propadl >{BM_DROP_LIMIT:.0%} pod best '
                       f'({new_bm:.1%} < {all_time_best_bm:.1%}) — HARD-REJECT')
        required = None
    else:
        required = 0.5 + k * sigma
        if chess_score < required and not baseline_reset:
            promote = False
            reasons.append(f'head-to-head {chess_score:.1%} < práh {required:.1%} '
                           f'({tier}: {k:.1f}σ, σ={sigma:.1%}, {decisive} rozhodnutých)')

    if new_bm < BM_FLOOR and not baseline_reset:
        promote = False
        reasons.append(f'benchmark pod minimem ({new_bm:.1%} < {BM_FLOOR:.0%})')

    bar_str = f'{required:.1%}' if required is not None else 'HARD-REJECT'
    print(f'Gate práh: HtH ≥ {bar_str} ({tier}), benchmark {new_bm:.1%} vs best {all_time_best_bm:.1%}', flush=True)

    if baseline_reset:
        promote = True  # force-establish the new TV baseline regardless of absolute score
        print(f'Baseline reset: TV{TV} model promoted as new baseline '
              f'(benchmark={new_bm:.1%}, chess vs old={chess_score:.1%})', flush=True)

    label = 'promoted' if promote else 'rejected'

    if promote:
        shutil.copy2(str(gate_path), str(best_path))
        new_all_time = max(all_time_best_bm, new_bm)
        _atomic_write_json(PROJECT_ROOT / 'weights_best_meta.json',
                           {'benchmark_win_rate': new_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS,
                            'all_time_best_benchmark': new_all_time, 'tv': TV})
        print(f'PROMOTED (benchmark={new_bm:.1%}, chess={chess_score:.1%}) → weights_best.json updated', flush=True)
    else:
        shutil.copy2(str(frozen_path), str(best_path))
        print(f'REJECTED: {"; ".join(reasons)}', flush=True)

    # Step 6: Git push
    if no_push:
        print('(git push přeskočen — --no-push)')
    else:
        _git_push(PROJECT_ROOT, promote, frozen_path, gate_path,
                  frozen_bm, new_bm, chess_score, label, all_time_best_bm)

    return promote, new_bm, chess_score


def _git_push(root: Path, promote: bool, frozen_path: Path, gate_path: Path,
              frozen_bm: float, new_bm: float, chess_score: float, label: str,
              all_time_best_bm: float) -> None:
    try:
        # Read weights into memory BEFORE git reset (reset overwrites working tree)
        with open(gate_path, 'rb') as f:
            gate_data = f.read()
        with open(frozen_path, 'rb') as f:
            frozen_data = f.read()
        # epoch_metrics.csv je výstup běhu (neregeneruje se z ničeho), takže ho musíme
        # zachytit před resetem stejně jako váhy — jinak `git reset --hard` přepíše
        # čerstvé metriky committed (zastaralou) verzí DŘÍV, než je commitneme, a
        # policy_loss / top1_agreement se do gitu nikdy nedostanou.
        metrics_path = root / 'epoch_metrics.csv'
        metrics_data = metrics_path.read_bytes() if metrics_path.exists() else None

        # Pull latest to avoid conflict, then add our files
        subprocess.run(['git', 'fetch', 'origin'], cwd=str(root), capture_output=True)
        subprocess.run(['git', 'reset', '--hard', 'origin/main'], cwd=str(root), capture_output=True)

        # Re-apply weights after reset (reset overwrites working tree)
        best_path = root / 'weights_best.json'
        if not promote:
            best_path.write_bytes(frozen_data)
            meta = {'benchmark_win_rate': frozen_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS,
                    'all_time_best_benchmark': all_time_best_bm, 'tv': TV}
        else:
            best_path.write_bytes(gate_data)
            meta = {'benchmark_win_rate': new_bm, 'benchmark_mcts_iterations': MCTS_ITERATIONS,
                    'all_time_best_benchmark': max(all_time_best_bm, new_bm), 'tv': TV}

        _atomic_write_json(root / 'weights_best_meta.json', meta)

        # Re-apply epoch_metrics.csv after reset (reset clobbered it back to committed)
        if metrics_data is not None:
            metrics_path.write_bytes(metrics_data)

        files = [
            'weights_best.json', 'weights_best_meta.json',
            'weights_frozen.json', 'weights_az_train.json', 'weights_az_train_meta.json',
            'weights_train_best.json', 'epoch_metrics.csv',
        ]
        snaps = [f for f in os.listdir(str(root)) if f.startswith('weights_snap_')]
        subprocess.run(['git', 'add', '-f'] + files + snaps, cwd=str(root), capture_output=True)

        bm_str = f'{new_bm:.1%}'
        subprocess.run(
            ['git', 'commit', '-m', f'AlphaZero: bm={bm_str} chess={chess_score:.1%} ({label})'],
            cwd=str(root), capture_output=True,
        )
        r = subprocess.run(['git', 'push'], cwd=str(root), capture_output=True)
        if r.returncode == 0:
            print('Pushed!')
        else:
            print(f'Push failed: {r.stderr.decode()[:300]}')
            print('(Weights jsou uloženy lokálně)')
    except Exception as e:
        print(f'Git push failed: {e} — weights uloženy lokálně')


def main() -> None:
    sys.stdout.reconfigure(line_buffering=True)
    parser = argparse.ArgumentParser(description='AlphaZero iteration runner')
    parser.add_argument('--loop', type=int, default=1, metavar='N',
                        help='Počet iterací za sebou (default: 1)')
    parser.add_argument('--no-push', action='store_true',
                        help='Přeskoč git push po každé iteraci')
    args = parser.parse_args()

    for i in range(args.loop):
        if args.loop > 1:
            print(f'\n{"=" * 60}')
            print(f'ITERACE {i + 1}/{args.loop}')
            print(f'{"=" * 60}')
        run_iteration(no_push=args.no_push)

    if args.loop > 1:
        print(f'\nHotovo — {args.loop} iterací dokončeno.')


if __name__ == '__main__':
    main()
