"""Replay viewer for Blood Bowl AI games.

Usage:
    python -m blood_bowl.replay_viewer --matches=1 --home-race=human --away-race=orc
    python -m blood_bowl.replay_viewer --matches=1 --home-ai=macro_mcts --away-ai=random --mcts=200
    python -m blood_bowl.replay_viewer --matches=1 --save=replay.json
    python -m blood_bowl.replay_viewer --load=replay.json
"""
from __future__ import annotations

import argparse
import json
import random
import sys
from pathlib import Path


EVENT_ICONS = {
    'MOVE': '→',
    'DODGE': '↪',
    'GFI': '⚡',
    'BLOCK': '⚔',
    'PUSH': '↗',
    'INJURY': '💀',
    'TOUCHDOWN': '🏈',
    'TURNOVER': '❌',
    'BALL_BOUNCE': '⊙',
    'PASS': '↝',
    'CATCH': '🤲',
    'PICKUP': '⬆',
    'FOUL': '🦶',
    'KNOCKED_DOWN': '⬇',
    'ARMOR_BREAK': '💥',
    'CASUALTY': '☠',
}

STATE_NAMES = {0: 'standing', 1: 'prone', 2: 'stunned', 3: 'off'}


def render_pitch(turn: dict) -> str:
    """Render ASCII pitch for a turn snapshot."""
    # Pitch: 26 wide (0-25) x 15 tall (0-14)
    grid = [['.' for _ in range(26)] for _ in range(15)]

    # Place home players (uppercase)
    for p in turn['home_players']:
        x, y = int(p['x']), int(p['y'])
        if 0 <= x <= 25 and 0 <= y <= 14:
            ch = 'H'
            if p['state'] == 1:
                ch = 'h'  # prone
            elif p['state'] == 2:
                ch = '_'  # stunned
            if p['has_ball']:
                ch = '@'
            grid[y][x] = ch

    # Place away players (lowercase/numbers)
    for p in turn['away_players']:
        x, y = int(p['x']), int(p['y'])
        if 0 <= x <= 25 and 0 <= y <= 14:
            ch = 'A'
            if p['state'] == 1:
                ch = 'a'  # prone
            elif p['state'] == 2:
                ch = '_'
            if p['has_ball']:
                ch = '@'
            grid[y][x] = ch

    # Ball on ground
    if not turn['ball_held'] and 0 <= turn['ball_x'] <= 25 and 0 <= turn['ball_y'] <= 14:
        bx, by = int(turn['ball_x']), int(turn['ball_y'])
        if grid[by][bx] == '.':
            grid[by][bx] = 'o'

    # Build output
    lines = []
    lines.append('   ' + ''.join(f'{x % 10}' for x in range(26)))
    lines.append('   ' + '-' * 26)
    for y in range(15):
        row = ''.join(grid[y])
        lines.append(f'{y:2d}|{row}|')
    lines.append('   ' + '-' * 26)
    lines.append('   H=home  A=away  @=ball carrier  o=loose ball  h/a=prone  _=stunned')
    return '\n'.join(lines)


def summarize_events(events: list) -> list[str]:
    """Summarize events into human-readable lines."""
    lines = []
    for ev in events:
        etype = ev['type']
        icon = EVENT_ICONS.get(etype, '?')
        pid = ev.get('player_id', -1)
        tid = ev.get('target_id', -1)
        roll = ev.get('roll', 0)
        success = ev.get('success', False)
        result = '✓' if success else '✗'

        if etype == 'MOVE':
            continue  # Skip individual moves (too verbose)
        elif etype == 'DODGE':
            lines.append(f'  {icon} Player {pid} dodge (roll {roll}) {result}')
        elif etype == 'GFI':
            lines.append(f'  {icon} Player {pid} GFI (roll {roll}) {result}')
        elif etype == 'BLOCK':
            lines.append(f'  {icon} Player {pid} blocks {tid} (roll {roll}) {result}')
        elif etype == 'PUSH':
            fx, fy = ev.get('from_x', -1), ev.get('from_y', -1)
            tx, ty = ev.get('to_x', -1), ev.get('to_y', -1)
            lines.append(f'  {icon} Push {tid}: ({fx},{fy})→({tx},{ty})')
        elif etype == 'KNOCKED_DOWN':
            lines.append(f'  {icon} Player {pid} knocked down')
        elif etype == 'ARMOR_BREAK':
            lines.append(f'  {icon} Armor broken on {pid} (roll {roll})')
        elif etype in ('INJURY', 'CASUALTY'):
            lines.append(f'  {icon} Player {pid} injured (roll {roll})')
        elif etype == 'TOUCHDOWN':
            lines.append(f'  {icon} TOUCHDOWN by player {pid}!')
        elif etype == 'TURNOVER':
            lines.append(f'  {icon} TURNOVER!')
        elif etype == 'PICKUP':
            lines.append(f'  {icon} Player {pid} pickup (roll {roll}) {result}')
        elif etype == 'CATCH':
            lines.append(f'  {icon} Player {pid} catch (roll {roll}) {result}')
        elif etype == 'PASS':
            lines.append(f'  {icon} Player {pid} pass to {tid} (roll {roll}) {result}')
        elif etype == 'FOUL':
            lines.append(f'  {icon} Player {pid} fouls {tid}')
        elif etype == 'BALL_BOUNCE':
            tx, ty = ev.get('to_x', -1), ev.get('to_y', -1)
            lines.append(f'  {icon} Ball bounces to ({tx},{ty})')
        else:
            lines.append(f'  {icon} {etype}: player {pid}')

    return lines


def print_replay(turn_logs: list, verbose: bool = False) -> None:
    """Print turn-by-turn game replay."""
    for i, turn in enumerate(turn_logs):
        team = turn['active_team'].upper()
        half = turn['half']
        tn = turn['turn']
        score = f"{turn['home_score']}-{turn['away_score']}"
        n_home = len(turn['home_players'])
        n_away = len(turn['away_players'])

        header = f"=== Half {half}, Turn {tn} ({team}) | Score {score} | Players: {n_home}H vs {n_away}A ==="
        print(header)

        if verbose:
            print(render_pitch(turn))

        # Summarize events
        event_lines = summarize_events(turn['events'])
        if event_lines:
            for line in event_lines:
                print(line)

        # Turn result flags
        if turn.get('turnover'):
            print('  ❌ Turn ended in TURNOVER')
        if turn.get('touchdown'):
            print('  🏈 TOUCHDOWN scored!')

        if not event_lines and not turn.get('turnover') and not turn.get('touchdown'):
            n_events = len(turn['events'])
            if n_events > 0:
                print(f'  ({n_events} actions)')

        print()


def analyze_game(turn_logs: list) -> dict:
    """Analyze a game for patterns useful for AI improvement."""
    stats = {
        'total_turns': len(turn_logs),
        'home_turnovers': 0,
        'away_turnovers': 0,
        'home_touchdowns': 0,
        'away_touchdowns': 0,
        'failed_dodges': 0,
        'failed_gfi': 0,
        'failed_pickups': 0,
        'blocks': 0,
        'knockdowns': 0,
        'casualties': 0,
        'home_actions_per_turn': [],
        'away_actions_per_turn': [],
    }

    for turn in turn_logs:
        team = turn['active_team']
        n_events = len(turn['events'])

        if team == 'home':
            stats['home_actions_per_turn'].append(n_events)
        else:
            stats['away_actions_per_turn'].append(n_events)

        if turn.get('turnover'):
            if team == 'home':
                stats['home_turnovers'] += 1
            else:
                stats['away_turnovers'] += 1

        if turn.get('touchdown'):
            if team == 'home':
                stats['home_touchdowns'] += 1
            else:
                stats['away_touchdowns'] += 1

        for ev in turn['events']:
            if ev['type'] == 'DODGE' and not ev['success']:
                stats['failed_dodges'] += 1
            elif ev['type'] == 'GFI' and not ev['success']:
                stats['failed_gfi'] += 1
            elif ev['type'] == 'PICKUP' and not ev['success']:
                stats['failed_pickups'] += 1
            elif ev['type'] == 'BLOCK':
                stats['blocks'] += 1
            elif ev['type'] == 'KNOCKED_DOWN':
                stats['knockdowns'] += 1
            elif ev['type'] == 'CASUALTY':
                stats['casualties'] += 1

    return stats


def print_analysis(stats: dict) -> None:
    """Print game analysis summary."""
    print('=' * 50)
    print('GAME ANALYSIS')
    print('=' * 50)
    print(f"Total turns: {stats['total_turns']}")
    print(f"Touchdowns: HOME {stats['home_touchdowns']} - AWAY {stats['away_touchdowns']}")
    print(f"Turnovers:  HOME {stats['home_turnovers']} - AWAY {stats['away_turnovers']}")
    print(f"Failed dodges: {stats['failed_dodges']}")
    print(f"Failed GFIs: {stats['failed_gfi']}")
    print(f"Failed pickups: {stats['failed_pickups']}")
    print(f"Blocks: {stats['blocks']}, Knockdowns: {stats['knockdowns']}, Casualties: {stats['casualties']}")

    if stats['home_actions_per_turn']:
        avg_home = sum(stats['home_actions_per_turn']) / len(stats['home_actions_per_turn'])
        print(f"Home avg actions/turn: {avg_home:.1f}")
    if stats['away_actions_per_turn']:
        avg_away = sum(stats['away_actions_per_turn']) / len(stats['away_actions_per_turn'])
        print(f"Away avg actions/turn: {avg_away:.1f}")
    print()


def run_game(home_race: str, away_race: str, home_ai: str, away_ai: str,
             mcts_iterations: int, weights: str, seed: int | None = None) -> list:
    """Run a game and return turn logs."""
    import bb_engine

    if seed is None:
        seed = random.randint(0, 2**31 - 1)

    home_roster = bb_engine.get_roster(home_race)
    away_roster = bb_engine.get_roster(away_race)
    if not home_roster:
        raise ValueError(f'Unknown roster: {home_race}')
    if not away_roster:
        raise ValueError(f'Unknown roster: {away_race}')

    logged = bb_engine.simulate_game_logged(
        home_roster, away_roster,
        home_ai, away_ai,
        seed=seed,
        weights_path=weights,
        mcts_iterations=mcts_iterations,
    )

    turn_logs = logged.get_turn_logs()
    return turn_logs


def main():
    parser = argparse.ArgumentParser(description='Blood Bowl AI Replay Viewer')
    parser.add_argument('--matches', type=int, default=1, help='Number of games to play')
    parser.add_argument('--home-race', default='human', help='Home race')
    parser.add_argument('--away-race', default='orc', help='Away race')
    parser.add_argument('--home-ai', default='macro_mcts', help='Home AI')
    parser.add_argument('--away-ai', default='random', help='Away AI')
    parser.add_argument('--mcts', type=int, default=200, help='MCTS iterations')
    parser.add_argument('--weights', default='', help='Weights file path')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show pitch each turn')
    parser.add_argument('--save', default='', help='Save replay to JSON file')
    parser.add_argument('--load', default='', help='Load replay from JSON file')
    parser.add_argument('--seed', type=int, default=None, help='Random seed')
    args = parser.parse_args()

    if args.load:
        with open(args.load) as f:
            data = json.load(f)
        print(f"Loaded replay from {args.load}")
        for i, game in enumerate(data['games']):
            print(f"\n{'#' * 60}")
            print(f"GAME {i+1}")
            print(f"{'#' * 60}\n")
            print_replay(game['turns'], verbose=args.verbose)
            stats = analyze_game(game['turns'])
            print_analysis(stats)
        return

    all_games = []
    for game_num in range(args.matches):
        print(f"\n{'#' * 60}")
        print(f"GAME {game_num + 1}: {args.home_race} ({args.home_ai}) vs {args.away_race} ({args.away_ai})")
        print(f"{'#' * 60}\n")

        turn_logs = run_game(
            args.home_race, args.away_race,
            args.home_ai, args.away_ai,
            args.mcts, args.weights, args.seed,
        )

        print_replay(turn_logs, verbose=args.verbose)
        stats = analyze_game(turn_logs)
        print_analysis(stats)
        all_games.append({'turns': turn_logs})

    if args.save:
        data = {
            'home_race': args.home_race,
            'away_race': args.away_race,
            'home_ai': args.home_ai,
            'away_ai': args.away_ai,
            'games': all_games,
        }
        with open(args.save, 'w') as f:
            json.dump(data, f)
        print(f"Replay saved to {args.save}")


if __name__ == '__main__':
    main()
