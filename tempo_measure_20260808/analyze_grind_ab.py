#!/usr/bin/env python3
# Vyhodnocení grind A/B (harness mode 1: cage+grind vs cage/fallback).
# Vstup: tempo_measure_20260806/ab_m{0,1}/diag_f1_grind_rows.jsonl + ab_grind log.
# Výstup: párová delta, remízovost/0:0 s CI, adoption obou ramen, attrition.
import json, math, os, re, glob
from collections import defaultdict

D = os.path.dirname(os.path.abspath(__file__))
MU = {0: 'dw-sk', 1: 'dw-we', 2: 'dw-dw', 3: 'orc-sk'}

def wilson(k, n, z=1.96):
    if n == 0: return (0.0, 0.0)
    p = k / n; d = 1 + z * z / n; c = p + z * z / (2 * n)
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n))
    return ((c - h) / d, (c + h) / d)

# Referenční remízovost z A/B 04.08. (cage vs off) pro stejné matchupy
REF = {0: (312, 800, 230), 1: (293, 800, 250), 2: (474, 800, 456), 3: (297, 800, 193)}

for sub in sorted(glob.glob(os.path.join(D, 'ab_m*'))):
    rows_f = os.path.join(sub, 'diag_f1_grind_rows.jsonl')
    if not os.path.exists(rows_f): continue
    rows = [json.loads(l) for l in open(rows_f)]
    # Dedup (2026-08-10): the 07.08. launcher fired twice, so that run's
    # jsonl holds every game exactly twice, bit-identically -- the engine is
    # deterministic given a seed, so a duplicate adds no information. Left
    # in, it halves the apparent variance: the draw-rate CI came out sqrt(2)
    # too narrow. The paired delta was never affected (it pairs on seed_idx).
    seen, dedup = set(), []
    for r in rows:
        k = (r['seed_idx'], r['cand'], r['cand_home'])
        if k in seen: continue
        seen.add(k); dedup.append(r)
    if len(dedup) != len(rows):
        print(f"  !! {len(rows)-len(dedup)} duplicitnich radku zahozeno "
              f"({len(rows)} -> {len(dedup)})")
    rows = dedup
    if not rows: continue
    mi = rows[0]['matchup']
    n = len(rows)
    draws = sum(1 for x in rows if x['cand'] == x['base'])
    zz = sum(1 for x in rows if x['cand'] == 0 and x['base'] == 0)
    w = sum(1 for x in rows if x['cand'] > x['base'])
    l = sum(1 for x in rows if x['cand'] < x['base'])
    tds = sum(x['cand'] + x['base'] for x in rows)
    cp = sum(x['cand_plans'] for x in rows) / n
    bp = sum(x.get('base_plans', 0) for x in rows) / n
    # párová delta ze seed párů (chess kandidáta home+away - 1)
    pair = defaultdict(dict)
    for x in rows:
        ch = 1.0 if x['cand'] > x['base'] else (0.0 if x['cand'] < x['base'] else 0.5)
        pair[x['seed_idx']]['h' if x['cand_home'] else 'a'] = ch
    ds = [p['h'] + p['a'] - 1.0 for p in pair.values() if len(p) == 2]
    mean = sum(ds) / len(ds)
    sd = math.sqrt(max(0, sum(d * d for d in ds) / len(ds) - mean * mean))
    se = sd / math.sqrt(len(ds))
    dlo, dhi = wilson(draws, n)
    print(f"\n===== {MU[mi]} GRIND A/B: {len(ds)} párů ({n} her) =====")
    print(f"  kandidát (grind): W{w} D{draws} L{l} | párová Δchess {mean:+.4f} ± {se:.4f} SE ({mean/se if se>0 else 0:.1f} SE)")
    print(f"  remízy {draws}/{n} = {draws/n:.1%} (CI {dlo:.1%}–{dhi:.1%}) | 0:0 {zz}/{n} = {zz/n:.1%} | TD/hru {tds/n:.2f}")
    rk, rn, rz = REF[mi]
    print(f"  reference 04.08. (cage vs off): remízy {rk/rn:.1%}, 0:0 {rz/rn:.1%} — jiné rameno, jen orientačně")
    print(f"  plánů/hru: grind {cp:.2f} vs fallback {bp:.2f}")
    # --- dwarf-perspektiva (závazný vstup uživatele 06.08.): matchup je
    # intrinsicky nakloněný, 50 % není cíl — reportuj TD/hru dwarfa a
    # DISTRIBUCI skóre (0:0/0:1/1:2/...) po ramenech (kdo hrál dwarfa).
    def side_scores(x):
        home = x['cand'] if x['cand_home'] else x['base']
        away = x['base'] if x['cand_home'] else x['cand']
        return home, away  # home = race_h (dwarf u m0/m1/m2)
    for arm, sel in (('dwarf=GRIND', lambda x: x['cand_home']),
                     ('dwarf=fallback', lambda x: not x['cand_home'])):
        g = [x for x in rows if sel(x)]
        if not g: continue
        dist = defaultdict(int)
        dtd = otd = 0
        for x in g:
            h, a = side_scores(x)
            dtd += h; otd += a
            dist[f"{h}:{a}"] += 1
        top = sorted(dist.items(), key=lambda kv: -kv[1])[:8]
        print(f"  {arm}: N={len(g)} dwarfTD/hru={dtd/len(g):.2f} "
              f"soupeřTD/hru={otd/len(g):.2f} | skóre: "
              + " ".join(f"{k}×{v}" for k, v in top))
    logf = glob.glob(os.path.join(sub, 'ab_grind_m*.log'))
    if logf:
        txt = open(logf[0]).read()
        m = re.search(r'=== ATTRITION.*', txt, re.S)
        if m: print("  " + m.group(0).strip().replace("\n", "\n  ")[:900])
