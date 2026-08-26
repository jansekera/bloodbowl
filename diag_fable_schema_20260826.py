#!/usr/bin/env python3
"""Jednorázový průzkum schématu jednoho souboru korpusu."""
import gzip, json, sys
from collections import Counter
g = json.load(gzip.open(sys.argv[1], 'rt'))
print([k for k in g if k != 'turn_logs'], g['home_race'], g['away_race'], len(g['turn_logs']))
tl = g['turn_logs'][3]
print(sorted(tl.keys()))
print({k: v for k, v in tl.items() if k not in ('home_players', 'away_players', 'events', 'plan')})
print(tl['home_players'][0])
print(tl.get('plan'))
c = Counter(); keys = {}
for t in g['turn_logs']:
    for e in t['events']:
        c[e['type']] += 1; keys.setdefault(e['type'], set()).update(e.keys())
print(c)
for k, v in keys.items():
    print(k, sorted(v))
# one sample of each event
seen = set()
for t in g['turn_logs']:
    for e in t['events']:
        if e['type'] not in seen:
            seen.add(e['type']); print(e)
