#!/usr/bin/env python3
# M7 -- JAK CASTO SE HRAC POTKA S OBRANCEM, KTERY MA WRESTLE?
#
# Zadani z Fable auditu pohybu (B2): planovac `estimateBlockFailChance` pocita
# BOTH_DOWN jako bezpecny pro utocnika s Blockem -- ale obrance BEZ Blocku
# a S WRESTLE utocnika SLOZI (a u nosice je to turnover, ř. 8677-8678).
# Skaven stavi 2 linemany +Wrestle, takze blitz do nich je systematicky
# PODCENENE riziko.
#
# A zaroven: rozhodnuti o Wrestle na Longbeardovi (T5.13c) se dnes dela
# poslepu -- nemame ani jedno cislo o tom, jak casto se s tou dovednosti
# hráč vubec potka.
#
# ⚠️ Korpus je Z PREDDNESNIHO enginu: obrance s Wrestle ho pouzival VZDY
# (volba pribyla 24.08.) a BOTH_DOWN jeste odsouval (N8). Cetnosti tedy plati,
# vysledky jsou "jak to dopadalo PRED opravou".
import gzip, json, glob, collections
from multiprocessing import Pool

FACE = {0: 'ATTACKER_DOWN', 1: 'BOTH_DOWN', 2: 'PUSHED',
        3: 'DEFENDER_STUMBLES', 4: 'DEFENDER_DOWN'}

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    c = collections.Counter()
    for t in d['turn_logs']:
        who = {p['id']: p['name'] for p in t['home_players'] + t['away_players']}
        for e in t['events']:
            if e.get('type') != 'BLOCK':
                continue
            tgt = e.get('target_id', -1)
            name = who.get(tgt, '')
            wr = 'Wrestle' in name
            c['bloku'] += 1
            if wr:
                c['do_wrestle'] += 1
            face = FACE.get(e.get('roll', -1), '?')
            c[('face', face)] += 1
            if wr:
                c[('wface', face)] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = len(rows)
    n = tot['bloku']; w = tot['do_wrestle']
    print("her: %d   bloku celkem: %d (%.1f na hru)" % (g, n, n/g))
    print("z toho do obrance s WRESTLE: %d  = %.2f %%  (%.2f na hru)" % (w, 100.0*w/n, w/g))
    print()
    print("%-20s %12s %8s   %12s %8s" % ("tvar kostky", "vsechny", "%", "do Wrestle", "%"))
    for i in range(5):
        f = FACE[i]
        a = tot[('face', f)]; b = tot[('wface', f)]
        print("%-20s %12d %7.1f %%   %12d %7.1f %%" %
              (f, a, 100.0*a/n if n else 0, b, 100.0*b/w if w else 0))
    print()
    bd = tot[('wface', 'BOTH_DOWN')]
    print("⇒ BOTH_DOWN do obrance s Wrestle: %d = %.2f na hru" % (bd, bd/g))
    print("   To je presne ta situace, kterou planovac ceni jako BEZPECNOU pro")
    print("   utocnika s Blockem, a ktera ho ve skutecnosti SLOZI.")
