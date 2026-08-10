#!/usr/bin/env python3
"""Vyhodnocení srovnání ér D-vlny 1 (rules parity), 10.→11.08.2026.

Párově per seed: POST (po opravách) vs PRE (99067fc). Pohled TRPASLÍKŮ
u dw-sk/dw-we, pohled ORKŮ u kontrolní orc-sk.

PRE-REGISTRACE (pojistka, ne experiment):
  PRIMÁRNĚ  trpaslíci nesmí regredovat -> PROBLÉM při z <= -1.28
  NEUTRÁLNÍ = ÚSPĚCH, žádná hypotéza o zlepšení
  KONTROLA  orc-sk odliší "pomohlo trpaslíkům" od "zrychlilo hru všem"
"""
import json, math, os, glob
from collections import defaultdict

D = os.path.dirname(os.path.abspath(__file__))
Z_FAIL = -1.28


def load(path):
    """seed_idx -> {orient: (home_score, away_score)}"""
    out = defaultdict(dict)
    if not os.path.exists(path):
        return out
    for line in open(path):
        r = json.loads(line)
        h = r['cand'] if r['cand_home'] else r['base']
        a = r['base'] if r['cand_home'] else r['cand']
        out[r['seed_idx']][bool(r['cand_home'])] = (h, a)
    return out


def chess(h, a):
    return 1.0 if h > a else (0.0 if h < a else 0.5)


def per_seed(rows):
    """seed -> průměrné chess skóre HOME rasy přes obě orientace"""
    out = {}
    for s, d in rows.items():
        if len(d) == 2:
            out[s] = sum(chess(*v) for v in d.values()) / 2.0
    return out


def tds(rows):
    out = {}
    for s, d in rows.items():
        if len(d) == 2:
            out[s] = (sum(v[0] for v in d.values()) / 2.0,
                      sum(v[1] for v in d.values()) / 2.0)
    return out


print("=" * 72)
print("D-VLNA 1 (rules parity) — SROVNÁNÍ ÉR   POST vs PRE(99067fc)")
print("=" * 72)

for name, who in (('dw-sk', 'trpaslík'), ('dw-we', 'trpaslík'),
                  ('orc-sk', 'ork (KONTROLA)')):
    pre = load(os.path.join(D, f'{name}_pre', 'diag_era_rows.jsonl'))
    post = load(os.path.join(D, f'{name}_post', 'diag_era_rows.jsonl'))
    cpre, cpost = per_seed(pre), per_seed(post)
    common = sorted(set(cpre) & set(cpost))
    if not common:
        print(f"\n### {name}: ŽÁDNÁ DATA")
        continue

    ds = [cpost[s] - cpre[s] for s in common]
    m = sum(ds) / len(ds)
    sd = math.sqrt(max(0.0, sum(x * x for x in ds) / len(ds) - m * m))
    se = sd / math.sqrt(len(ds)) if len(ds) else 0.0
    z = m / se if se > 0 else 0.0

    tpre, tpost = tds(pre), tds(post)
    dh = sum(tpost[s][0] - tpre[s][0] for s in common) / len(common)
    da = sum(tpost[s][1] - tpre[s][1] for s in common) / len(common)

    print(f"\n### {name}  ({len(common)} společných seedů, pohled: {who})")
    print(f"  chess PRE  {sum(cpre[s] for s in common)/len(common):.4f}"
          f"   POST {sum(cpost[s] for s in common)/len(common):.4f}")
    print(f"  párová Δ   {m:+.4f} ± {se:.4f} SE   (z = {z:+.2f})")
    print(f"  TD home/hru Δ {dh:+.3f}   TD soupeře/hru Δ {da:+.3f}")
    if name.startswith('dw'):
        verdict = ("PROBLÉM — trpaslíci regredovali" if z <= Z_FAIL
                   else "OK — regrese neprokázána (neutrální = úspěch)")
        print(f"  >>> POJISTKA: {verdict}  (práh z <= {Z_FAIL})")
    else:
        print("  >>> KONTROLA: posun zde = efekt je GLOBÁLNÍ, ne trpasličí")

print("\nPozn.: pojistka je jednostranná. Kladná Δ NENÍ důkaz zlepšení —")
print("žádná hypotéza o zlepšení nebyla předem registrována.")
