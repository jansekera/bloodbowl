#!/usr/bin/env python3
"""P40: vybírá rameno P38 pole, která koridor OBĚHNOU, nebo PROLOMÍ?

Zadání z fronty (20.08.): „pro každé vybrané pole porovnat odpor s polem
základu a rozdělit na OBĚHNUTÍ / PROLOMENÍ, zvlášť pro trpaslíka a pro elfa."
Důvod: uživatelovo rozlišení — ZEĎ je univerzální objekt, ale odpověď na ni je
RASOVÁ: trpaslík PROLOMÍ, elf OBĚHNE. Boční volnost (složka A ramene) JE
oběhnutí, tedy elfí nástroj. Když trpaslíkův zisk z P38 nese taky ona, engine
vyhrává tím, že hraje za trpaslíka elfa — a to je varování, ne úspěch.

⚠️ APROXIMACE, přiznaná: krokový rozpočet `steps` se v enginu počítá
`carrierStallAwareSteps` (stav skóre, zbývající kola). Tady se používá
`movementRemaining` jako HORNÍ MEZ, takže se prochází stejně velký nebo větší
čtverec než engine. Vliv: kandidátů je víc, ne míň — směr závěru to neotáčí.
"""
import gzip, json, glob, sys
from collections import Counter

DEPTH, HALF = 4, 2          # cage_advance.h: CORRIDOR_DEPTH / CORRIDOR_HALF_WIDTH
DIAG = ((-1,-1),(-1,1),(1,-1),(1,1))
ORTH = ((-1,0),(1,0),(0,-1),(0,1))

def onp(x, y): return 0 <= x <= 25 and 0 <= y <= 14
def cheb(ax, ay, bx, by): return max(abs(ax-bx), abs(ay-by))

def corridor_resistance(theirs, cx, cy, dx):
    """1:1 přepis corridorResistance (cage_advance.cpp)."""
    n = 0
    for p in theirs:
        if p['state'] != 0: continue
        ahead = (p['x'] - cx) * dx
        if ahead < 1 or ahead > DEPTH: continue
        if abs(p['y'] - cy) > HALF: continue
        n += 1
    return n

def cage_ok(mine, theirs, carrier_id, cand, budget):
    """1:1 přepis cageScoreForSquare: (3) žádný jiný soused, (2) čisté rohy,
       (1) čtyři těla na ně dosáhnou."""
    cx, cy = cand
    occ = {(p['x'], p['y']): p for p in mine + theirs if p['state'] == 0}
    # (3) čtyři ortogonály prázdné (nikdo kromě nosiče)
    for ox, oy in ORTH:
        q = (cx+ox, cy+oy)
        if not onp(*q): continue
        p = occ.get(q)
        if p is not None and p['id'] != carrier_id: return False
    # rohy
    corners = []
    for dxr, dyr in DIAG:
        q = (cx+dxr, cy+dyr)
        if not onp(*q): continue
        p = occ.get(q)
        if p is not None and any(p is t for t in theirs): return False
        for e in theirs:                       # (2) čistý = žádný stojící soupeř vedle
            if e['state'] == 0 and cheb(e['x'], e['y'], *q) <= 1: return False
        corners.append(q)
    if len(corners) < 4: return False
    # (1) čtyři těla dosáhnou
    used = set()
    for q in corners:
        best, bd = None, 999
        for b in mine:
            if b['id'] == carrier_id or b['state'] != 0 or b['id'] in used: continue
            d = cheb(b['x'], b['y'], *q)
            if d > b.get('ma', 6): continue
            if d < bd: bd, best = d, b['id']
        if best is None: return False
        used.add(best)
    return True

def main(dirs, limit):
    V = Counter(); R = {}
    for corp in dirs:
        for f in sorted(glob.glob(corp + '/g*.json.gz'))[:limit]:
            d = json.load(gzip.open(f))
            for side in ('home', 'away'):
                race = d[side + '_race']; opp = 'away' if side == 'home' else 'home'
                dx = 1 if side == 'home' else -1
                for t in d['turn_logs']:
                    if t['active_team'] != side: continue
                    cid = t.get('ball_carrier_id', -1)
                    if not cid or cid < 1: continue
                    mine = t[side + '_players']; theirs = t[opp + '_players']
                    me = [p for p in mine if p['id'] == cid]
                    if not me or me[0]['state'] != 0: continue
                    c = me[0]; cx, cy = c['x'], c['y']
                    # --- validace reimplementace proti exportu ---
                    exp = t.get('corridor_resistance', -1)
                    if exp is not None and exp >= 0:
                        V['validace_celkem'] += 1
                        if corridor_resistance(theirs, cx, cy, dx) == exp:
                            V['validace_sedi'] += 1
                    budget = max(1, c.get('ma', 6))
                    base_x = max(1, min(24, cx + dx*budget))
                    base_y = cy + (1 if cy < 5 else (-1 if cy > 9 else 0))
                    r_base = corridor_resistance(theirs, base_x, base_y, dx)
                    occ = {(p['x'], p['y']) for p in mine + theirs if p['state'] == 0}
                    best = None
                    for ox in range(-budget, budget+1):
                        for oy in range(-budget, budget+1):
                            q = (cx+ox, cy+oy)
                            if not onp(*q) or q in occ: continue
                            prog = ox * dx
                            if best and prog < best[0]: continue
                            if cage_ok(mine, theirs, cid, q, budget):
                                rr = corridor_resistance(theirs, q[0], q[1], dx)
                                if not best or (prog, -rr) > (best[0], -best[1]):
                                    best = (prog, rr, q)
                    a = R.setdefault(race, Counter())
                    a['kol'] += 1
                    if not best:
                        a['rameno_nenaslo_pole'] += 1; continue
                    a['rameno_naslo'] += 1
                    if best[1] < r_base:   a['OBEHNUTI (nizsi odpor)'] += 1
                    elif best[1] > r_base: a['PROLOMENI (vyssi odpor)'] += 1
                    else:                  a['STEJNY odpor'] += 1
                    a['souc_odpor_rameno'] += best[1]; a['souc_odpor_zaklad'] += r_base
    v = V['validace_celkem'] or 1
    print(f"VALIDACE reimplementace corridorResistance: {V['validace_sedi']}/{V['validace_celkem']}"
          f" = {100*V['validace_sedi']/v:.1f} %  ⇐ musí být 100 %, jinak se závěr NEČTE\n")
    for race in sorted(R, key=lambda r: -R[r]['kol']):
        a = R[race]; n = a['rameno_naslo'] or 1
        print(f"{race:10s} kol s nosičem {a['kol']:6d} | rameno našlo pole {a['rameno_naslo']:6d}"
              f" ({100*a['rameno_naslo']/max(1,a['kol']):4.1f} %)")
        for k in ('OBEHNUTI (nizsi odpor)', 'STEJNY odpor', 'PROLOMENI (vyssi odpor)'):
            print(f"      {k:26s} {a[k]:6d}  {100*a[k]/n:5.1f} %")
        print(f"      ⌀ odpor: rameno {a['souc_odpor_rameno']/n:.2f} vs základ {a['souc_odpor_zaklad']/n:.2f}")

if __name__ == '__main__':
    lim = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    main(['blitzlanding_replic_20260825_corpus_data'], lim)
