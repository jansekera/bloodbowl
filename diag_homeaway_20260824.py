#!/usr/bin/env python3
"""Zrcadlová utkání (stejná rasa na obou stranách) MUSÍ být symetrická.
Nejsou. Hledám mechanismus: kdo dostává výkop a kdo skóruje."""
import gzip, json, glob, os, collections
from multiprocessing import Pool

def one(path):
    d = json.load(gzip.open(path))
    tl = d['turn_logs']
    if not tl: return None
    first_active = tl[0]['active_team']
    # kdo dal první TD
    first_td = None
    h2_first = None
    for t in tl:
        if t.get('touchdown') and first_td is None:
            first_td = t['active_team']
        if t['half'] == 2 and h2_first is None:
            h2_first = t['active_team']
    return (d['home_race'], d['away_race'], d['home_score'], d['away_score'],
            first_active, h2_first, first_td, len(tl))

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as p:
        rows = [r for r in p.map(one, files, chunksize=50) if r]
    mir = [r for r in rows if r[0] == r[1]]
    print(f"zrcadlových her: {len(mir)}  (dwarf/orc/human/skaven/wood-elf x 1200)")
    print()
    print(f"{'rasa':<10}{'TD dom':>8}{'TD host':>9}{'pomer':>8}{'V dom':>7}{'V host':>8}{'sigma':>8}")
    import math
    by = collections.defaultdict(list)
    for r in mir: by[r[0]].append(r)
    for race, rs in sorted(by.items(), key=lambda kv: kv[0]):
        n = len(rs)
        hs = sum(r[2] for r in rs)/n; as_ = sum(r[3] for r in rs)/n
        hw = sum(1 for r in rs if r[2] > r[3]); aw = sum(1 for r in rs if r[3] > r[2])
        dec = hw + aw
        z = (aw - dec/2)/math.sqrt(dec*0.25) if dec else 0
        print(f"{race:<10}{hs:>8.3f}{as_:>9.3f}{(as_/hs if hs else 0):>8.2f}{hw:>7}{aw:>8}{z:>+8.2f}")
    print()
    print("=== kdo je na tahu v 1. kole (0=home, 1=away) a kdo v 1. kole 2. půle ===")
    c1 = collections.Counter(r[4] for r in rows)
    c2 = collections.Counter(r[5] for r in rows)
    print("  1. kolo :", dict(c1))
    print("  2. půle :", dict(c2))
    print()
    print("=== první TD v zápase (jen zrcadla, kde padl aspoň jeden) ===")
    ct = collections.Counter(r[6] for r in mir if r[6] is not None)
    print("  ", dict(ct))
    print()
    print("=== TD podle půle (zrcadla) ===")
