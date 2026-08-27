#!/usr/bin/env python3
import gzip, json, time, sys
from collections import Counter
t = time.time()
g = json.load(gzip.open(sys.argv[1], 'rt'))
print("load s", round(time.time() - t, 3))
print({k: g[k] for k in g if k != 'turn_logs'})
tl = g['turn_logs'][0]
print("n turn_logs", len(g['turn_logs']))
print({k: tl[k] for k in tl if k not in ('home_players', 'away_players', 'events', 'plan')})
print("plan:", tl.get('plan'))
print("player:", tl['home_players'][0])
c = Counter(); keys = {}
for t in g['turn_logs']:
    for e in t['events']:
        c[e['type']] += 1; keys.setdefault(e['type'], set()).update(e.keys())
print(c)
for k, v in keys.items():
    print(k, sorted(v))
for t in g['turn_logs'][:3]:
    print("---", t['half'], t['turn'], t['active_team'], t['ball_x'], t['ball_y'], t['ball_held'])
    for e in t['events'][:12]:
        print("  ", e)
