#!/usr/bin/env python3
# M9 -- STROP NA HODNOTU USTUPU PO BLITZU (zadano 24.08.; blokovalo P31 od 22.07.)
#
# Otazka: je N10 ("blitzer po bloku nesmi pokracovat v pohybu") velka vada,
# nebo hygiena -- a PRO KOHO?
#
# METODA (a co z ni plyne a co ne):
#  * blitz poznam podle toho, ze hráč ma v temze kole MOVE/GFI PRED svym BLOCK.
#    (Cisty Block Action pohyb neobsahuje, ř. 675.)
#  * zbyvajici MA = MA - kroku pred blokem - 1 (blok pri blitzu stoji 1 MA).
#  * ⭐ Souperova tela se BEHEM NASEHO KOLA NEHYBOU (krome odsunu), takze jejich
#    pozice ze snimku na zacatku kola plati i v okamziku bloku. Diky tomu jde
#    spocitat, jestli blitzer po bloku stoji v cizi tackle zone a jestli by mel
#    kam ustoupit.
#  ⚠️ APROXIMACE: neresim follow-up (blitzer se muze posunout na pole obrance)
#    ani odsun. Bereme pozici blitzera v okamziku bloku (`from_x/from_y`).
#    Cil bloku se z "ohrozujicich" vylucuje -- po uspesnem bloku casto lezi.
import gzip, json, glob, collections
from multiprocessing import Pool

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    c = collections.Counter()
    for t in d['turn_logs']:
        home_ids = {p['id'] for p in t['home_players']}
        pl = {p['id']: p for p in t['home_players'] + t['away_players']}
        ev = t['events']
        steps = collections.Counter()
        for i, e in enumerate(ev):
            ty = e.get('type'); pid = e.get('player_id')
            if ty in ('MOVE', 'GFI'):
                steps[pid] += 1
                continue
            if ty != 'BLOCK':
                continue
            if steps[pid] == 0:
                continue                      # cisty Block, ne blitz
            p = pl.get(pid)
            if not p:
                continue
            c['blitzu'] += 1
            av = p['av']
            key = 'AV7' if av <= 7 else ('AV8' if av == 8 else 'AV9+')
            c[(key, 'n')] += 1
            spare = p['ma'] - steps[pid] - 1
            if spare >= 1:
                c['zbyva_MA'] += 1
                c[(key, 'zbyva')] += 1
            # kde stoji a kdo ho ohrozuje
            ax, ay = e.get('from_x'), e.get('from_y')
            tid = e.get('target_id', -1)
            mine_home = pid in home_ids
            enemies = [q for q in pl.values()
                       if (q['id'] in home_ids) != mine_home
                       and q['state'] == 0 and q['id'] != tid]
            def adj(x, y):
                return sum(1 for q in enemies
                           if max(abs(q['x']-x), abs(q['y']-y)) == 1)
            if adj(ax, ay) > 0:
                c['zustava_v_TZ'] += 1
                c[(key, 'vTZ')] += 1
                if spare >= 1:
                    # ma KAM? prazdne sousedni pole mimo vsechny cizi TZ
                    occupied = {(q['x'], q['y']) for q in pl.values() if q['state'] in (0,1,2)}
                    ok = False
                    for dx in (-1,0,1):
                        for dy in (-1,0,1):
                            if dx == 0 and dy == 0: continue
                            nx, ny = ax+dx, ay+dy
                            if not (0 <= nx <= 25 and 0 <= ny <= 14): continue
                            if (nx, ny) in occupied: continue
                            if adj(nx, ny) == 0:
                                ok = True; break
                        if ok: break
                    if ok:
                        c['MA_KAM'] += 1
                        c[(key, 'kam')] += 1
    return c

if __name__ == '__main__':
    # 2026-08-28: cesta byla natvrdo na korpus z 21.08. Zobecneno na argument,
    # aby sel STROP precist parove -- tyz seed, rameno M1/N10 ZAP vs VYP.
    # Bez toho je kazde cislo jen snimek proti pohnute bazi.
    import sys
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    files = sorted(glob.glob(f'{root}/*/g*.json.gz')) or sorted(glob.glob(f'{root}/g*.json.gz'))
    if not files:
        raise SystemExit(f'zadne hry v {root}')
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r is not None]
    tot = collections.Counter()
    for r in rows: tot += r
    g = len(rows); n = tot['blitzu']
    print("her: %d   blitzu (blok po pohybu): %d  = %.2f na hru" % (g, n, n/g))
    print()
    print("  (a) zbyva mu MA >= 1:            %8d   %5.1f %%" % (tot['zbyva_MA'], 100.0*tot['zbyva_MA']/n))
    print("  (b) zustava v cizi tackle zone:  %8d   %5.1f %%" % (tot['zustava_v_TZ'], 100.0*tot['zustava_v_TZ']/n))
    print("  (c) ...a MEL by KAM ustoupit:    %8d   %5.1f %%   %5.2f na hru"
          % (tot['MA_KAM'], 100.0*tot['MA_KAM']/n, tot['MA_KAM']/g))
    print()
    print("  (d) podle AV:")
    print("      %-6s %10s %10s %10s %10s" % ("", "blitzu", "zbyva MA", "v TZ", "mel by kam"))
    for k in ('AV7', 'AV8', 'AV9+'):
        kn = tot[(k,'n')]
        if not kn: continue
        print("      %-6s %10d %9.1f %% %9.1f %% %9.1f %%" %
              (k, kn, 100.0*tot[(k,'zbyva')]/kn, 100.0*tot[(k,'vTZ')]/kn, 100.0*tot[(k,'kam')]/kn))
