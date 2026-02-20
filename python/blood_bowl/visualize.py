"""Visualization of training progress.

Usage: python -m blood_bowl.visualize --csv=training_results.csv
"""
from __future__ import annotations

import argparse
import csv
from pathlib import Path


def plot_training(csv_path: str, output_path: str = 'training_curve.png') -> None:
    """Plot win rate over epochs using matplotlib."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    epochs = []
    win_rates = []
    epsilons = []

    with open(csv_path) as f:
        reader = csv.DictReader(f)
        for row in reader:
            epochs.append(int(row['epoch']))
            win_rates.append(float(row['win_rate']) * 100)
            epsilons.append(float(row['epsilon']))

    fig, ax1 = plt.subplots(figsize=(10, 6))

    # Win rate
    color_wr = '#2196F3'
    ax1.set_xlabel('Epoch')
    ax1.set_ylabel('Win Rate (%)', color=color_wr)
    ax1.plot(epochs, win_rates, color=color_wr, linewidth=2, label='Win Rate')
    ax1.tick_params(axis='y', labelcolor=color_wr)
    ax1.set_ylim(0, 100)

    # Epsilon on secondary axis
    ax2 = ax1.twinx()
    color_eps = '#FF9800'
    ax2.set_ylabel('Epsilon', color=color_eps)
    ax2.plot(epochs, epsilons, color=color_eps, linewidth=1, linestyle='--', label='Epsilon')
    ax2.tick_params(axis='y', labelcolor=color_eps)
    ax2.set_ylim(0, 1)

    # Title and grid
    plt.title('Blood Bowl Learning AI - Training Progress')
    ax1.grid(True, alpha=0.3)

    # Legend
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

    plt.tight_layout()
    plt.savefig(output_path, dpi=150)
    plt.close()
    print(f'Training curve saved to {output_path}')


def main():
    parser = argparse.ArgumentParser(description='Visualize training progress')
    parser.add_argument('--csv', default='training_results.csv', help='CSV input path')
    parser.add_argument('--output', default='training_curve.png', help='Output PNG path')
    args = parser.parse_args()

    plot_training(args.csv, args.output)


if __name__ == '__main__':
    main()
