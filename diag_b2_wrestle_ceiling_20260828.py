#!/usr/bin/env python3
"""B2 — STROP: kolikrát za hru padne blok na obránce, který WRESTLE MÁ.

Píše se PŘED předregistrací noci B2. Měří PŘÍLEŽITOST, ne zisk:
rameno `setWrestlePricingArm` mění OCENĚNÍ bloku proti obránci s Wrestle,
takže víc příležitostí než tohle mít nemůže.

⚠️ Korpus je PRODUKČNÍ, rameno VYPNUTÉ — proto je to strop, ne spotřeba.
"""
import gzip, json, glob, sys
from collections import Counter

root = sys.argv[1] if len(sys.argv) > 1 else 'blitzcont_replic_20260827_corpus_data'
files = sorted(glob.glob(f'{root}/*.json.gz'))
if len(sys.argv) > 2:
    files = files[:int(sys.argv[2])]

games = 0
blocks = 0
blocks_vs_wrestle = 0
blocks_by_wrestler = 0            # útočník sám má Wrestle (jiná věc)
matchups = Counter()
per_game = []
wrestle_bodies = Counter()

for fn in files:
    d = json.load(gzip.open(fn))
    games += 1
    matchups[(d['home_race'], d['away_race'])] += 1
    g_bl = 0
    g_bw = 0
    for t in d['turn_logs']:
        # jméno -> má Wrestle, pro obě strany
        wrestle = {}
        for side in ('home_players', 'away_players'):
            for p in t[side]:
                wrestle[p['id']] = '+Wrestle' in p['name']
        for e in t['events']:
            if e['type'] != 'BLOCK':
                continue
            g_bl += 1
            tid = e['target_id']
            if wrestle.get(tid):
                g_bw += 1
            if wrestle.get(e['player_id']):
                global_by = 1
    blocks += g_bl
    blocks_vs_wrestle += g_bw
    per_game.append(g_bw)

print(f"her: {games}")
print("matchupy:", dict(matchups))
print()
print(f"bloků celkem                 {blocks:8d}   {blocks/games:6.2f}/hru")
print(f"z toho na obránce s Wrestle  {blocks_vs_wrestle:8d}   {blocks_vs_wrestle/games:6.2f}/hru"
      f"   ({100.0*blocks_vs_wrestle/blocks if blocks else 0:.1f} %)")
print()
nz = sum(1 for x in per_game if x)
print(f"her s aspoň jednou příležitostí: {nz}/{games} ({100.0*nz/games:.1f} %)")
per_game.sort()
print(f"medián/hru {per_game[len(per_game)//2]}, max {per_game[-1]}")

# --- rozpad po matchupech (běh B2 se má vybrat podle NĚJ) --------------------
import collections
mu_bl = collections.Counter(); mu_bw = collections.Counter(); mu_n = collections.Counter()
for fn in files:
    d = json.load(gzip.open(fn))
    key = tuple(sorted((d['home_race'], d['away_race'])))
    mu_n[key] += 1
    for t in d['turn_logs']:
        wr = {}
        for side in ('home_players', 'away_players'):
            for p in t[side]:
                wr[p['id']] = '+Wrestle' in p['name']
        for e in t['events']:
            if e['type'] == 'BLOCK':
                mu_bl[key] += 1
                if wr.get(e['target_id']):
                    mu_bw[key] += 1
print()
print(f"{'matchup':20s} {'her':>5s} {'bloků/hru':>10s} {'vs Wrestle/hru':>15s} {'podíl':>7s}")
for k in sorted(mu_n, key=lambda k: -mu_bw[k]/mu_n[k]):
    n = mu_n[k]
    print(f"{k[0]+'-'+k[1]:20s} {n:5d} {mu_bl[k]/n:10.2f} {mu_bw[k]/n:15.2f} "
          f"{100.0*mu_bw[k]/mu_bl[k] if mu_bl[k] else 0:6.1f} %")
