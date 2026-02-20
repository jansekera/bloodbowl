"""Train from accumulated game logs (human vs AI, or any JSONL logs).

Usage: python -m blood_bowl.train_from_logs --logs-dir=logs/games/ --weights=weights.json --method=td_lambda
       python -m blood_bowl.train_from_logs --logs-dir=logs/games/ --weights=weights.json --method=mc_shaped --archive
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from .trainer import LinearTrainer


def _read_jsonl(path: Path) -> list[dict]:
    records = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                records.append(json.loads(line))
    return records


def train_from_logs(
    logs_dir: str,
    weights_file: str,
    method: str = 'td_lambda',
    learning_rate: float = 0.01,
    gamma: float = 0.99,
    lambda_: float = 0.8,
    archive: bool = False,
) -> None:
    """Train on all JSONL game logs in a directory."""
    logs_path = Path(logs_dir)
    if not logs_path.exists():
        print(f'Logs directory not found: {logs_dir}')
        return

    log_files = sorted(logs_path.glob('*.jsonl'))
    if not log_files:
        print(f'No .jsonl files found in {logs_dir}')
        return

    trainer = LinearTrainer(learning_rate=learning_rate)

    weights_path = Path(weights_file)
    if weights_path.exists():
        trainer.load_weights(str(weights_path))
        print(f'Loaded existing weights from {weights_file}')

    print(f'Training on {len(log_files)} game logs using {method}')
    print(f'LR={learning_rate}, gamma={gamma}' + (f', lambda={lambda_}' if method == 'td_lambda' else ''))

    trained = 0
    for log_file in log_files:
        game_log = _read_jsonl(log_file)
        if not game_log:
            continue

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

        trained += 1

    trainer.save_weights(str(weights_path))
    print(f'Trained on {trained} games, saved weights to {weights_file}')

    if archive and trained > 0:
        archive_dir = logs_path / 'archived'
        archive_dir.mkdir(exist_ok=True)
        for log_file in log_files:
            shutil.move(str(log_file), str(archive_dir / log_file.name))
        print(f'Archived {len(log_files)} log files to {archive_dir}')


def main():
    parser = argparse.ArgumentParser(description='Train from accumulated game logs')
    parser.add_argument('--logs-dir', required=True, help='Directory with .jsonl game logs')
    parser.add_argument('--weights', default='weights.json', help='Weights file path')
    parser.add_argument('--method', default='td_lambda',
                        choices=['mc', 'mc_shaped', 'td0', 'td_lambda'],
                        help='Training method')
    parser.add_argument('--lr', type=float, default=0.01, help='Learning rate')
    parser.add_argument('--gamma', type=float, default=0.99, help='Discount factor')
    parser.add_argument('--lambda', type=float, default=0.8, dest='lambda_',
                        help='Trace decay for td_lambda')
    parser.add_argument('--archive', action='store_true',
                        help='Move processed logs to archived/ subdirectory')
    args = parser.parse_args()

    train_from_logs(
        logs_dir=args.logs_dir,
        weights_file=args.weights,
        method=args.method,
        learning_rate=args.lr,
        gamma=args.gamma,
        lambda_=args.lambda_,
        archive=args.archive,
    )


if __name__ == '__main__':
    main()
