#!/usr/bin/env python3
# M1 -- JAK CASTO SE ZAKORENENY HRAC JESTE HYBE?
#
# Zadani z Fable auditu pohybu (k N11). Take Root: na 1 ma hráč MA 0 az do
# konce drivu (ř. 8574-8576) a "may not Go For It ... or use any skill that
# would allow him to move out of his current square" (8577-8579).
#
# ⚠️ Korpus je z PREDDNESNIHO enginu, kde zakorenení NEPERZISTOVALO (TA2) a
# `rooted` neznal pathfinder ani resolveMoveStep (N11). Tohle tedy NEMERI vadu,
# ktera zbyva -- meri, KOLIK CHOVANI dnesni opravy odebraly.
import gzip, json, glob, collections
from multiprocessing import Pool

TAKEROOT = 28

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    c = collections.Counter()
    for t in d['turn_logs']:
        ev = t['events']
        rooted_at = {}
        for i, e in enumerate(ev):
            if e.get('type') == 'SKILL' and e.get('roll') == TAKEROOT:
                c['hodu_TakeRoot'] += 1
                if not e.get('success'):
                    c['ZAKORENIL'] += 1
                    rooted_at.setdefault(e.get('player_id'), i)
        for pid, idx in rooted_at.items():
            later = [x for x in ev[idx+1:] if x.get('player_id') == pid]
            kinds = {x.get('type') for x in later}
            if 'MOVE' in kinds:
                c['pak_se_HYBAL'] += 1
            if 'GFI' in kinds:
                c['pak_GFI'] += 1
            if 'BLOCK' in kinds:
                c['pak_BLOKOVAL'] += 1
            if not kinds:
                c['pak_nic'] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r is not None]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = len(rows); z = tot['ZAKORENIL']
    print("her: %d" % g)
    print("hodu na Take Root:  %8d   (%.2f na hru)" % (tot['hodu_TakeRoot'], tot['hodu_TakeRoot']/g))
    print("z toho ZAKORENIL:   %8d   (%.1f %%, %.3f na hru)" % (z, 100.0*z/max(1,tot['hodu_TakeRoot']), z/g))
    if z:
        print()
        print("  po zakoreneni v TEMZE kole jeste:")
        print("    MOVE:      %8d   %5.1f %%" % (tot['pak_se_HYBAL'], 100.0*tot['pak_se_HYBAL']/z))
        print("    GFI:       %8d   %5.1f %%" % (tot['pak_GFI'], 100.0*tot['pak_GFI']/z))
        print("    BLOCK:     %8d   %5.1f %%" % (tot['pak_BLOKOVAL'], 100.0*tot['pak_BLOKOVAL']/z))
        print("    nic:       %8d   %5.1f %%" % (tot['pak_nic'], 100.0*tot['pak_nic']/z))
