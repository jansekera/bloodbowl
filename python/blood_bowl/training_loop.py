"""Training loop orchestrator: simulate games, train, repeat."""
from __future__ import annotations

import csv
import json
import shutil
import sys
import time
from pathlib import Path

from .cli_runner import CLIRunner
from .trainer import LinearTrainer, NeuralTrainer, create_trainer, load_trainer


def run_training(
    epochs: int = 50,
    games_per_epoch: int = 20,
    opponent: str = 'random',
    learning_rate: float = 0.01,
    epsilon_start: float = 0.3,
    epsilon_end: float = 0.05,
    weights_file: str = 'weights.json',
    log_dir: str = 'training_logs',
    output_csv: str = 'training_results.csv',
    project_root: str | None = None,
    timeout: int = 120,
    home_race: str = 'random',
    away_race: str = 'random',
    self_play: bool = False,
    lr_decay: float = 1.0,
    training_method: str = 'mc',
    gamma: float = 0.99,
    lambda_: float = 0.8,
    opponent_weights: str | None = None,
    model_type: str = 'linear',
    hidden_size: int = 32,
    replay_buffer_size: int = 0,
    replay_batch_size: int = 64,
    benchmark_interval: int = 0,
    benchmark_matches: int = 20,
    curriculum: bool = False,
    skip_greedy_benchmark: bool = False,
    benchmark_timeout: int | None = None,
    tv: int = 1000,
    use_cpp: bool | None = None,
    mcts_iterations: int = 0,
    policy_lr: float = 0.0,
) -> None:
    """Run the full training loop.

    Each epoch:
    1. Run games with current weights
    2. Train on game logs
    3. Train policy on MCTS decisions (if policy_lr > 0)
    4. Save updated weights
    5. Record metrics to CSV
    """
    # Auto-detect C++ engine or use flag
    cpp_available = False
    if use_cpp is not False:
        try:
            import bb_engine  # noqa: F401
            cpp_available = True
        except ImportError:
            pass

    if use_cpp is True and not cpp_available:
        raise RuntimeError('--use-cpp specified but bb_engine module not found')

    if cpp_available and use_cpp is not False:
        from .cpp_runner import CPPRunner
        runner = CPPRunner(project_root)
        print('Using C++ engine (bb_engine) for simulation')
    else:
        runner = CLIRunner(project_root)

    # Resolve paths relative to project root (PHP cwd)
    root = Path(runner.project_root)
    weights_path = root / weights_file
    log_base = root / log_dir

    # Policy training setup
    policy_trainer = None
    use_policy = policy_lr > 0 and mcts_iterations > 0
    if use_policy:
        from .policy_trainer import PolicyTrainer, save_combined_weights, load_combined_weights
        if weights_path.exists():
            trainer, policy_trainer = load_combined_weights(
                str(weights_path), value_lr=learning_rate, policy_lr=policy_lr)
        else:
            trainer = create_trainer(
                model_type=model_type, hidden_size=hidden_size, learning_rate=learning_rate)
            policy_trainer = PolicyTrainer(learning_rate=policy_lr)
    else:
        # Load existing weights or create new trainer
        if weights_path.exists():
            trainer = load_trainer(str(weights_path), learning_rate=learning_rate)
        else:
            trainer = create_trainer(
                model_type=model_type,
                hidden_size=hidden_size,
                learning_rate=learning_rate,
            )

    # Prepare CSV
    csv_path = Path(output_csv) if Path(output_csv).is_absolute() else root / output_csv
    with open(csv_path, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['epoch', 'win_rate', 'avg_score_diff', 'epsilon'])

    total_games = epochs * games_per_epoch
    games_done = 0
    training_start = time.time()
    game_times: list[float] = []

    effective_opponent = 'learning (self-play)' if self_play else opponent

    # Resolve opponent weights path
    opp_weights_path: str | None = None
    if opponent_weights is not None:
        opp_weights_path = str(root / opponent_weights) if not Path(opponent_weights).is_absolute() else opponent_weights

    actual_model = type(trainer).__name__
    print(f'Training: {epochs} epochs x {games_per_epoch} games = {total_games} games total')
    print(f'Model: {actual_model}, Opponent: {effective_opponent}, LR: {learning_rate}, Epsilon: {epsilon_start:.2f} -> {epsilon_end:.2f}')
    print(f'Method: {training_method}' + (f', gamma={gamma}' if training_method != 'mc' else '') + (f', lambda={lambda_}' if training_method == 'td_lambda' else ''))
    print(f'Races: {home_race} vs {away_race}' + (f', LR decay: {lr_decay}' if lr_decay != 1.0 else ''))
    if mcts_iterations > 0:
        print(f'MCTS: {mcts_iterations} iterations per action')
    if use_policy:
        print(f'Policy training: lr={policy_lr}')
    if opp_weights_path:
        print(f'Opponent weights: {opp_weights_path}')
    # Replay buffer — auto-enable for MCTS training to prevent forgetting
    if replay_buffer_size == 0 and mcts_iterations > 0:
        replay_buffer_size = 10000  # ~50 games worth of transitions
        replay_batch_size = max(replay_batch_size, 64)
    replay_buffer = None
    if replay_buffer_size > 0:
        from .replay_buffer import ReplayBuffer
        replay_buffer = ReplayBuffer(capacity=replay_buffer_size)
        replay_path = root / 'replay_buffer.pkl'
        if replay_path.exists():
            replay_buffer.load(str(replay_path))
            print(f'Replay buffer: loaded {len(replay_buffer)} transitions, capacity={replay_buffer_size}')
        else:
            print(f'Replay buffer: new, capacity={replay_buffer_size}, batch={replay_batch_size}')

    # Benchmark
    if benchmark_interval > 0:
        print(f'Benchmark: every {benchmark_interval} epochs, {benchmark_matches} matches')

    # Estimated completion time
    from datetime import datetime, timedelta
    avg_game_secs = 130 if mcts_iterations > 0 else 15  # rough estimate
    est_total_secs = total_games * avg_game_secs
    if benchmark_interval > 0:
        num_benchmarks = epochs // benchmark_interval
        est_total_secs += num_benchmarks * benchmark_matches * avg_game_secs
    est_end = datetime.now() + timedelta(seconds=est_total_secs)
    print(f'Estimated finish: ~{est_end.strftime("%H:%M")} ({est_total_secs // 3600}h {(est_total_secs % 3600) // 60}m)')

    # Curriculum
    curriculum_stages = [
        {'opponent': 'random', 'win_rate_threshold': 0.65},
        {'opponent': 'greedy', 'win_rate_threshold': 0.55},
        {'opponent': 'learning', 'self_play': True, 'win_rate_threshold': None},
    ]
    curriculum_stage = 0
    curriculum_win_rates: list[float] = []
    if curriculum:
        opponent = curriculum_stages[0]['opponent']
        effective_opponent = opponent
        print(f'Curriculum: starting at stage 0 ({opponent})')

    print()

    for epoch in range(1, epochs + 1):
        # Linear epsilon decay
        if epochs > 1:
            epsilon = epsilon_start + (epsilon_end - epsilon_start) * (epoch - 1) / (epochs - 1)
        else:
            epsilon = epsilon_start

        # LR decay: multiplicative per epoch
        trainer.lr = learning_rate * (lr_decay ** (epoch - 1))
        if policy_trainer:
            policy_trainer.lr = policy_lr * (lr_decay ** (epoch - 1))

        # Clean log dir for this epoch
        epoch_log_dir = log_base / f'epoch_{epoch:03d}'
        if epoch_log_dir.exists():
            shutil.rmtree(epoch_log_dir)
        epoch_log_dir.mkdir(parents=True, exist_ok=True)

        epoch_start = time.time()

        # Progress callback for per-game updates
        def on_game_done(current: int, total: int, elapsed: float, score: str) -> None:
            nonlocal games_done
            games_done += 1
            game_times.append(elapsed)
            from datetime import datetime, timedelta
            avg_time = sum(game_times) / len(game_times)
            remaining = (total_games - games_done) * avg_time
            end_time = datetime.now() + timedelta(seconds=remaining)
            eta = end_time.strftime('%H:%M')
            pct = games_done * 100 // total_games
            bar_len = 30
            filled = bar_len * games_done // total_games
            bar = '#' * filled + '-' * (bar_len - filled)
            sys.stdout.write(
                f'\r  [{bar}] {pct}% | Game {games_done}/{total_games} '
                f'({elapsed:.0f}s, score {score}) | Done ~{eta}   '
            )
            sys.stdout.flush()

        # Curriculum: override opponent for current stage
        if curriculum:
            stage = curriculum_stages[curriculum_stage]
            opponent = stage['opponent']
            self_play = stage.get('self_play', False)
            effective_opponent = 'learning (self-play)' if self_play else opponent

        # Self-play: both sides use same AI type with same weights/epsilon
        home_ai_type = 'macro_mcts' if mcts_iterations > 0 else 'learning'
        away_ai = home_ai_type if self_play else opponent

        # Opponent weights: frozen separate weights for away learning AI
        away_weights_arg = opp_weights_path if opp_weights_path else None
        away_epsilon_arg = 0.0 if opp_weights_path else None

        # Policy weights path for MCTS
        policy_weights_arg = str(weights_path) if use_policy else None

        # Run simulation games (timeout is per-game)
        # Support mixed away races: "orc,skaven,dwarf,wood-elf"
        away_races = [r.strip() for r in away_race.split(',')]
        if len(away_races) > 1:
            # Distribute games evenly across races
            all_results = []
            games_per_race = games_per_epoch // len(away_races)
            remainder = games_per_epoch % len(away_races)
            game_offset = 0
            for race_idx, race in enumerate(away_races):
                race_games = games_per_race + (1 if race_idx < remainder else 0)
                if race_games == 0:
                    continue
                sub_result = runner.simulate(
                    home_ai=home_ai_type,
                    away_ai=away_ai,
                    matches=race_games,
                    timeout=timeout,
                    timeout_per_game=True,
                    weights=str(weights_path),
                    epsilon=epsilon,
                    log_dir=str(epoch_log_dir),
                    progress_callback=on_game_done,
                    home_race=home_race,
                    away_race=race,
                    away_weights=away_weights_arg,
                    away_epsilon=away_epsilon_arg,
                    tv=tv if tv != 1000 else None,
                    mcts_iterations=mcts_iterations,
                    policy_weights=policy_weights_arg,
                    game_offset=game_offset,
                )
                all_results.extend(sub_result.results)
                game_offset += race_games
            # Merge into single TournamentResult
            from .cli_runner import TournamentResult
            hw = sum(1 for r in all_results if r.winner == 'home')
            aw = sum(1 for r in all_results if r.winner == 'away')
            dr = len(all_results) - hw - aw
            result = TournamentResult(
                home_ai=home_ai_type, away_ai=away_ai,
                matches=len(all_results), home_wins=hw, away_wins=aw,
                draws=dr, results=all_results,
            )
        else:
            result = runner.simulate(
                home_ai=home_ai_type,
                away_ai=away_ai,
                matches=games_per_epoch,
                timeout=timeout,
                timeout_per_game=True,
                weights=str(weights_path),
                epsilon=epsilon,
                log_dir=str(epoch_log_dir),
                progress_callback=on_game_done,
                home_race=home_race,
                away_race=away_race,
                away_weights=away_weights_arg,
                away_epsilon=away_epsilon_arg,
                tv=tv if tv != 1000 else None,
                mcts_iterations=mcts_iterations,
                policy_weights=policy_weights_arg,
            )

        # Clear progress line
        sys.stdout.write('\r' + ' ' * 80 + '\r')

        # Train on all game logs from this epoch
        log_files = sorted(epoch_log_dir.glob('game_*.jsonl'))
        for log_file in log_files:
            game_log = _read_jsonl(log_file)

            # Add to replay buffer if enabled
            if replay_buffer is not None:
                replay_buffer.add_game(game_log)

            _train_on_log(trainer, game_log, training_method, gamma, lambda_)

        # Replay buffer: train on random samples
        if replay_buffer is not None and len(replay_buffer) > 0:
            batch = replay_buffer.sample(replay_batch_size)
            for transition in batch:
                # Create a minimal game log from transition
                mini_log = [
                    {'type': 'state', 'features': transition.features, 'perspective': transition.perspective},
                ]
                if not transition.is_terminal:
                    mini_log.append(
                        {'type': 'state', 'features': transition.next_features, 'perspective': transition.perspective},
                    )
                mini_log.append({
                    'type': 'result',
                    'home_score': 1 if transition.reward > 0 else 0,
                    'away_score': 1 if transition.reward < 0 else 0,
                    'winner': transition.perspective if transition.reward > 0 else (
                        ('away' if transition.perspective == 'home' else 'home') if transition.reward < 0 else None
                    ),
                })
                _train_on_log(trainer, mini_log, training_method, gamma, lambda_)

            # Save replay buffer periodically
            replay_buffer.save(str(root / 'replay_buffer.pkl'))

        # Train policy on MCTS decisions
        if policy_trainer:
            decision_files = sorted(epoch_log_dir.glob('decisions_*.json'))
            all_decisions = []
            for dec_file in decision_files:
                with open(dec_file) as f:
                    decisions = json.load(f)
                all_decisions.extend(decisions)

            if all_decisions:
                policy_loss = policy_trainer.train_on_decisions(all_decisions)
                n_dec = len(all_decisions)
            else:
                policy_loss = 0.0
                n_dec = 0

        # Save updated weights
        if policy_trainer:
            save_combined_weights(trainer, policy_trainer, str(weights_path))
        else:
            trainer.save_weights(str(weights_path))

        # Compute metrics
        win_rate = result.home_win_rate  # learning AI is home
        avg_score_diff = sum(
            r.home_score - r.away_score for r in result.results
        ) / max(len(result.results), 1)

        epoch_elapsed = time.time() - epoch_start
        total_elapsed = time.time() - training_start

        # Append to CSV
        with open(csv_path, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([epoch, f'{win_rate:.3f}', f'{avg_score_diff:.2f}', f'{epsilon:.3f}'])

        msg = (f'Epoch {epoch}/{epochs}: win_rate={win_rate:.1%}, '
               f'avg_score_diff={avg_score_diff:+.2f}, epsilon={epsilon:.3f} '
               f'({epoch_elapsed:.0f}s, total {total_elapsed:.0f}s)')
        if policy_trainer and n_dec > 0:
            msg += f' | policy: {n_dec} decisions, loss={policy_loss:.3f}'
        print(msg)

        # Print per-game scores
        scores = [f'{r.home_score}-{r.away_score}' for r in result.results]
        print(f'  Scores: {", ".join(scores)}')

        # Benchmark
        if benchmark_interval > 0 and epoch % benchmark_interval == 0:
            from .benchmark import run_benchmark
            bench_results = run_benchmark(
                weights_file=str(weights_path),
                opponents=['random', 'greedy'],
                matches_per_opponent=benchmark_matches,
                project_root=str(root),
                timeout=benchmark_timeout or timeout,
                skip_greedy=skip_greedy_benchmark,
                tv=tv if tv != 1000 else None,
                use_cpp=use_cpp if cpp_available else False,
                mcts_iterations=mcts_iterations,
                policy_weights=str(weights_path) if use_policy else None,
            )
            bench_csv = csv_path.parent / 'benchmark_results.csv'
            _append_benchmark_csv(bench_csv, epoch, bench_results)
            for opp_name, stats in bench_results.items():
                print(f'  Benchmark vs {opp_name}: {stats["win_rate"]:.1%} '
                      f'(score diff {stats["avg_score_diff"]:+.2f})')

            # Auto-snapshot weights after benchmark
            if 'random' in bench_results:
                bm_wr = bench_results['random']['win_rate']
                bm_sd = bench_results['random']['avg_score_diff']
                snap_name = f'weights_snap_e{epoch}_{bm_wr:.0%}_{bm_sd:+.1f}.json'
                snap_path = weights_path.parent / snap_name
                shutil.copy2(str(weights_path), str(snap_path))
                # Update weights_best.json if this is the best benchmark
                best_path = weights_path.parent / 'weights_best.json'
                best_wr = 0.0
                if best_path.exists():
                    try:
                        with open(best_path) as f:
                            best_meta = json.load(f)
                        best_wr = best_meta.get('benchmark_win_rate', 0.0)
                    except Exception:
                        pass
                if bm_wr > best_wr:
                    if str(weights_path.resolve()) != str(best_path.resolve()):
                        shutil.copy2(str(weights_path), str(best_path))
                    # Store benchmark info in best weights
                    with open(best_path) as f:
                        best_data = json.load(f)
                    best_data['benchmark_win_rate'] = bm_wr
                    best_data['benchmark_epoch'] = epoch
                    with open(best_path, 'w') as f:
                        json.dump(best_data, f)
                    print(f'  New best! Saved to {best_path.name}')
                    # Auto-push weights_best.json to GitHub
                    _git_push_weights_best(best_path, bm_wr, bm_sd, epoch)
                print(f'  Snapshot: {snap_name}')

                # Auto-revert: if benchmark drops >5% below best, revert and stop
                if best_wr > 0 and bm_wr < best_wr - 0.05:
                    print(f'  Over-specialization detected: {bm_wr:.1%} < {best_wr:.1%} - 5%')
                    if str(weights_path.resolve()) != str(best_path.resolve()):
                        shutil.copy2(str(best_path), str(weights_path))
                        print(f'  Reverted weights to best ({best_wr:.1%})')
                    else:
                        # Training on weights_best.json directly — restore from snapshot
                        # Find the best snapshot
                        best_snap = None
                        for sp in sorted(weights_path.parent.glob('weights_snap_*.json')):
                            try:
                                with open(sp) as f:
                                    sm = json.load(f)
                                swr = sm.get('benchmark_win_rate', 0.0)
                                if swr >= best_wr:
                                    best_snap = sp
                            except Exception:
                                pass
                        if best_snap:
                            shutil.copy2(str(best_snap), str(weights_path))
                            print(f'  Reverted from snapshot {best_snap.name}')
                    print(f'  Stopping training early to prevent further degradation.')
                    break

        # Curriculum advancement
        if curriculum:
            curriculum_win_rates.append(win_rate)
            stage = curriculum_stages[curriculum_stage]
            threshold = stage.get('win_rate_threshold')
            if threshold is not None and len(curriculum_win_rates) >= 3:
                rolling_avg = sum(curriculum_win_rates[-3:]) / 3.0
                if rolling_avg >= threshold and curriculum_stage < len(curriculum_stages) - 1:
                    curriculum_stage += 1
                    curriculum_win_rates.clear()
                    next_stage = curriculum_stages[curriculum_stage]
                    print(f'  >> Curriculum: advancing to stage {curriculum_stage} '
                          f'({next_stage["opponent"]})')

    total_time = time.time() - training_start
    mins, secs = divmod(int(total_time), 60)
    hours, mins = divmod(mins, 60)
    print(f'\nTraining complete in {hours}h{mins:02d}m{secs:02d}s')


def _train_on_log(trainer, game_log: list[dict], method: str, gamma: float, lambda_: float) -> None:
    """Train on a single game log using the specified method."""
    if method == 'mc':
        trainer.train_monte_carlo(game_log)
    elif method == 'mc_shaped':
        trainer.train_monte_carlo_shaped(game_log, gamma=gamma)
    elif method == 'td0':
        trainer.train_td0(game_log, gamma=gamma)
    elif method == 'td_lambda':
        trainer.train_td_lambda(game_log, gamma=gamma, lambda_=lambda_)
    else:
        raise ValueError(f'Unknown training method: {method}')


def _git_push_weights_best(best_path: Path, win_rate: float, score_diff: float, epoch: int) -> None:
    """Auto-commit and push weights_best.json to GitHub."""
    import subprocess
    repo_dir = best_path.parent
    try:
        subprocess.run(
            ['git', 'add', 'weights_best.json'],
            cwd=str(repo_dir), capture_output=True, timeout=10
        )
        msg = f'Update weights_best.json: {win_rate:.0%} win rate, {score_diff:+.1f} score diff (epoch {epoch})'
        subprocess.run(
            ['git', 'commit', '-m', msg],
            cwd=str(repo_dir), capture_output=True, timeout=10
        )
        result = subprocess.run(
            ['git', 'push'],
            cwd=str(repo_dir), capture_output=True, timeout=30
        )
        if result.returncode == 0:
            print(f'  Pushed weights_best.json to GitHub')
        else:
            print(f'  Git push failed (non-critical): {result.stderr.decode()[:100]}')
    except Exception as e:
        print(f'  Git push skipped: {e}')


def _append_benchmark_csv(csv_path: Path, epoch: int, results: dict) -> None:
    """Append benchmark results to CSV."""
    write_header = not csv_path.exists()
    with open(csv_path, 'a', newline='') as f:
        writer = csv.writer(f)
        if write_header:
            writer.writerow(['epoch', 'opponent', 'win_rate', 'avg_score_diff', 'matches'])
        for opp_name, stats in results.items():
            writer.writerow([
                epoch, opp_name,
                f'{stats["win_rate"]:.3f}',
                f'{stats["avg_score_diff"]:.2f}',
                stats['matches'],
            ])


def _read_jsonl(path: Path) -> list[dict]:
    """Read a JSONL file into a list of dicts."""
    records = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                records.append(json.loads(line))
    return records
