#!/usr/bin/env python3
"""M1: škodí NAUČENÁ policy trpaslíkovi, nebo víc pomáhá soupeři?

Obě ramena mají policy načtenou (prior floory aktivní v obou); liší se jen
policyBlend 0,2 (cand) vs 0,0 (base), a to POUZE na trpasličí straně.

Diagnostika, ne brána — žádný pass/fail práh. Rozsuzuje:
  záporná Δ u OBOU matchupů  => naučená policy trpaslíka kazí
  záporná jen u dw-we        => spíš víc pomáhá rychlému soupeři
"""
import json, math, os
from collections import defaultdict

D = os.path.dirname(os.path.abspath(__file__))

print("=" * 70)
print("M1 — naučená policy (blend 0,2 vs 0,0), POUZE trpasličí strana")
print("=" * 70)

for name in ('dw-sk', 'dw-we'):
    f = os.path.join(D, name, 'diag_m1_rows.jsonl')
    if not os.path.exists(f):
        print(f"\n### {name}: ŽÁDNÁ DATA")
        continue
    rows = [json.loads(l) for l in open(f)]
    pair = defaultdict(dict)
    td = defaultdict(dict)
    for r in rows:
        # trpaslík je vždy HOME rasa u m0/m1
        h = r['cand'] if r['cand_home'] else r['base']
        a = r['base'] if r['cand_home'] else r['cand']
        # cand_home == True  => trpaslík hraje S naučenou policy
        arm = 'on' if r['cand_home'] else 'off'
        pair[r['seed_idx']][arm] = 1.0 if h > a else (0.0 if h < a else 0.5)
        td[r['seed_idx']][arm] = (h, a)

    seeds = [s for s in pair if len(pair[s]) == 2]
    if not seeds:
        print(f"\n### {name}: neúplné páry")
        continue
    ds = [pair[s]['on'] - pair[s]['off'] for s in seeds]
    m = sum(ds) / len(ds)
    sd = math.sqrt(max(0.0, sum(x * x for x in ds) / len(ds) - m * m))
    se = sd / math.sqrt(len(ds))
    dtd = sum(td[s]['on'][0] - td[s]['off'][0] for s in seeds) / len(seeds)
    otd = sum(td[s]['on'][1] - td[s]['off'][1] for s in seeds) / len(seeds)

    print(f"\n### {name}  ({len(seeds)} párů)")
    print(f"  chess trpaslíka  policy ON {sum(pair[s]['on'] for s in seeds)/len(seeds):.4f}"
          f"   OFF {sum(pair[s]['off'] for s in seeds)/len(seeds):.4f}")
    print(f"  párová Δ (ON − OFF)  {m:+.4f} ± {se:.4f} SE  ({m/se if se else 0:+.1f} SE)")
    print(f"  TD trpaslíka/hru Δ {dtd:+.3f}   TD soupeře/hru Δ {otd:+.3f}")

print("\nČtení: Δ výrazně záporná u OBOU => policy kazí trpaslíka.")
print("Záporná jen u dw-we => spíš víc pomáhá rychlému soupeři.")
print("Δ ~ 0 => −17 pp z fairtestu 31.07. má jiný původ než naučený obsah.")
