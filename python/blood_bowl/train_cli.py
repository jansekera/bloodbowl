"""CLI for training the learning AI.

Usage: python -m blood_bowl.train_cli --epochs=50 --games=20 --opponent=random --lr=0.01
       python -m blood_bowl.train_cli --model=neural --hidden-size=32 --training-method=td_lambda
       python -m blood_bowl.train_cli --curriculum --epochs=100 --model=neural
       python -m blood_bowl.train_cli --replay-buffer-size=50000 --replay-batch-size=64
       python -m blood_bowl.train_cli --benchmark-interval=5 --benchmark-matches=20
"""
from __future__ import annotations

import argparse
from pathlib import Path

from .training_loop import run_training


def main():
    parser = argparse.ArgumentParser(description='Train Blood Bowl learning AI')
    parser.add_argument('--epochs', type=int, default=50, help='Number of training epochs')
    parser.add_argument('--games', type=int, default=20, help='Games per epoch')
    parser.add_argument('--opponent', default='random', help='Opponent AI type (random/greedy)')
    parser.add_argument('--lr', type=float, default=0.01, help='Learning rate')
    parser.add_argument('--epsilon-start', type=float, default=0.3, help='Starting epsilon')
    parser.add_argument('--epsilon-end', type=float, default=0.10, help='Ending epsilon (floor)')
    parser.add_argument('--weights', default='weights.json', help='Weights file path')
    parser.add_argument('--log-dir', default='training_logs', help='Log directory')
    parser.add_argument('--output-csv', default='training_results.csv', help='CSV output path')
    parser.add_argument('--project-root', default=None, help='Project root directory')
    parser.add_argument('--timeout', type=int, default=120, help='Timeout per game (seconds)')
    parser.add_argument('--home-race', default='random', help='Home race (or "random")')
    parser.add_argument('--away-race', default='random', help='Away race (or "random")')
    parser.add_argument('--self-play', action='store_true', help='Both sides use learning AI')
    parser.add_argument('--lr-decay', type=float, default=1.0, help='Multiplicative LR decay per epoch')
    parser.add_argument('--training-method', default='mc',
                        choices=['mc', 'mc_shaped', 'td0', 'td_lambda'],
                        help='Training method (mc, mc_shaped, td0, td_lambda)')
    parser.add_argument('--gamma', type=float, default=0.99, help='Discount factor')
    parser.add_argument('--lambda', type=float, default=0.8, dest='lambda_',
                        help='Trace decay for td_lambda')
    parser.add_argument('--opponent-weights', default=None,
                        help='Separate weights file for opponent (frozen)')
    # Phase 18: model selection
    parser.add_argument('--model', default='linear', choices=['linear', 'neural'],
                        help='Model type (linear or neural)')
    parser.add_argument('--hidden-size', type=int, default=32,
                        help='Hidden layer size for neural model')
    # Phase 18: replay buffer
    parser.add_argument('--replay-buffer-size', type=int, default=0,
                        help='Replay buffer capacity (0 = disabled)')
    parser.add_argument('--replay-batch-size', type=int, default=64,
                        help='Number of transitions to sample from replay buffer per epoch')
    # Phase 18: auto-benchmark
    parser.add_argument('--benchmark-interval', type=int, default=0,
                        help='Run benchmark every N epochs (0 = disabled)')
    parser.add_argument('--benchmark-matches', type=int, default=10,
                        help='Matches per opponent in benchmark')
    parser.add_argument('--skip-greedy-benchmark', action='store_true',
                        help='Skip greedy opponent in benchmark (faster)')
    parser.add_argument('--benchmark-timeout', type=int, default=None,
                        help='Timeout per game in benchmark (defaults to --timeout value)')
    # Phase 18: curriculum
    parser.add_argument('--curriculum', action='store_true',
                        help='Enable curriculum training (auto difficulty escalation)')
    parser.add_argument('--tv', type=int, default=1000,
                        help='Team value level (1000=base, 1500=developed rosters)')
    # MCTS
    parser.add_argument('--mcts-iterations', type=int, default=0,
                        help='MCTS iterations per action (0 = disabled, recommended 400)')
    # Policy network
    parser.add_argument('--policy-lr', type=float, default=0.0,
                        help='Policy network learning rate (0 = disabled, recommended 0.01)')
    # C++ engine
    cpp_group = parser.add_mutually_exclusive_group()
    cpp_group.add_argument('--use-cpp', action='store_true', default=False,
                           help='Force use of C++ engine (bb_engine)')
    cpp_group.add_argument('--no-cpp', action='store_true', default=False,
                           help='Force use of PHP CLI runner (disable C++ auto-detect)')
    args = parser.parse_args()

    # Default project root
    project_root = args.project_root
    if project_root is None:
        project_root = str(Path(__file__).parent.parent.parent)

    # Determine use_cpp: True (force), False (disable), None (auto-detect)
    if args.use_cpp:
        use_cpp = True
    elif args.no_cpp:
        use_cpp = False
    else:
        use_cpp = None

    run_training(
        epochs=args.epochs,
        games_per_epoch=args.games,
        opponent=args.opponent,
        learning_rate=args.lr,
        epsilon_start=args.epsilon_start,
        epsilon_end=args.epsilon_end,
        weights_file=args.weights,
        log_dir=args.log_dir,
        output_csv=args.output_csv,
        project_root=project_root,
        timeout=args.timeout,
        home_race=args.home_race,
        away_race=args.away_race,
        self_play=args.self_play,
        lr_decay=args.lr_decay,
        training_method=args.training_method,
        gamma=args.gamma,
        lambda_=args.lambda_,
        opponent_weights=args.opponent_weights,
        model_type=args.model,
        hidden_size=args.hidden_size,
        replay_buffer_size=args.replay_buffer_size,
        replay_batch_size=args.replay_batch_size,
        benchmark_interval=args.benchmark_interval,
        benchmark_matches=args.benchmark_matches,
        curriculum=args.curriculum,
        skip_greedy_benchmark=args.skip_greedy_benchmark,
        benchmark_timeout=args.benchmark_timeout,
        tv=args.tv,
        use_cpp=use_cpp,
        mcts_iterations=args.mcts_iterations,
        policy_lr=args.policy_lr,
    )


if __name__ == '__main__':
    main()
