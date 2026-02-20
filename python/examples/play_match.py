#!/usr/bin/env python3
"""Example: Run a single match simulation via CLI."""

from blood_bowl import CLIRunner

runner = CLIRunner()

print("Running Greedy vs Random match...")
result = runner.simulate(home_ai='greedy', away_ai='random', verbose=True, timeout=300)

match = result.results[0]
print(f"\nResult: Home {match.home_score} - {match.away_score} Away")
print(f"Winner: {match.winner or 'Draw'}")
print(f"Total actions: {match.total_actions}")
print(f"Final phase: {match.phase}, half: {match.half}")
