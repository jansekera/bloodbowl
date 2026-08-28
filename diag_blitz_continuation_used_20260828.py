#!/usr/bin/env python3
"""M1/N10 -- DOČTENÍ PŘEDREGISTRACE: body (2) STROP a (3) ODMÍTNUTÝ FOLLOW-UP.

⛔ PROČ NESTAČÍ `diag_blitz_retreat_ceiling_20260824.py`.
Ten čte pozici blitzera z `from_x/from_y` BLOKU, tedy V OKAMŽIKU BLOKU. Rameno
M1/N10 ale mění, co se stane POTOM. Strop z něj proto vyjde skoro identicky se
zapnutým i vypnutým ramenem (22,1 % vs 22,0 %) -- a to není nález, to je slepé
měřidlo. Vypadá to jako čistá nula, což je přesně ta past.

⭐ CO MĚŘÍ TENHLE. Pozici blitzera na KONCI jeho aktivace: po bloku se dohledají
další MOVE/GFI téhož hráče v témž kole a pozice se doskládá z `to_x/to_y`.
Soupeřova těla se během našeho kola nehýbou (kromě odsunu), takže tackle zóny
ze snímku na začátku kola platí.

Spouští se PÁROVĚ na dva korpusy se stejnými seedy: rameno ZAP vs VYP.
"""
import gzip, json, glob, collections, sys
from multiprocessing import Pool

DIRS = [(-1,-1),(-1,0),(-1,1),(0,-1),(0,1),(1,-1),(1,0),(1,1)]
STANDING = 0

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    c = collections.Counter()
    for t in d['turn_logs']:
        ours = t['active_team']
        mine = t['home_players'] if ours == 'home' else t['away_players']
        theirs = t['away_players'] if ours == 'home' else t['home_players']
        my_ids = {p['id'] for p in mine}
        ma = {p['id']: p['ma'] for p in mine}
        # tackle zony souperu ze snimku na zacatku kola
        tz = collections.Counter()
        opp_at = {}
        for p in theirs:
            if p['state'] != STANDING:
                continue
            opp_at[(p['x'], p['y'])] = p['id']
            for dx, dy in DIRS:
                tz[(p['x']+dx, p['y']+dy)] += 1
        occupied = {(p['x'], p['y']) for p in mine + theirs}

        # projdi udalosti a slozdruj aktivace nasich hracu
        steps = collections.Counter()      # kroku pred blokem
        pos = {p['id']: (p['x'], p['y']) for p in mine}
        blocked_at = {}                    # id -> pozice v okamziku bloku
        blocked_target = {}                # id -> id obrance
        moved_after = collections.Counter()
        followup_sq = {}                   # id -> UVOLNENE pole obrance
        # ⛔ JMENOVATEL. Follow-up existuje JEN tam, kde obrance pole opravdu
        # uvolnil -- tedy po PUSH. Puvodni verze delila vsemi bloky, takze
        # "odmitnuto v 99 %" bylo ve skutecnosti "v 99 % nebylo co odmitnout".
        # Presne trida feedback_na_bucket_is_the_finding.
        pushed_from = {}                   # id obrance -> pole, ze ktereho byl odsunut
        for e in t['events']:
            if e['type'] == 'PUSH':
                pushed_from.setdefault(e['player_id'], (e['from_x'], e['from_y']))
        for e in t['events']:
            pid = e['player_id']
            if pid not in my_ids:
                continue
            ty = e['type']
            if ty in ('MOVE', 'GFI'):
                pos[pid] = (e['to_x'], e['to_y'])
                if pid in blocked_at:
                    moved_after[pid] += 1
                else:
                    steps[pid] += 1
            elif ty == 'BLOCK':
                if steps[pid] > 0 and pid not in blocked_at:   # blitz
                    blocked_at[pid] = (e['from_x'], e['from_y'])
                    blocked_target[pid] = e['target_id']

        for pid, bp in blocked_at.items():
            c['blitzu'] += 1
            end = pos[pid]
            zbyva = ma[pid] - steps[pid] - 1
            v_tz_pri_bloku = tz[bp] > 0
            v_tz_na_konci = tz[end] > 0
            if moved_after[pid]:
                c['pokracoval_po_bloku'] += 1
            if zbyva >= 1:
                c['zbyva_MA'] += 1
            # STROP: mel MA, stal v TZ pri bloku, a existovalo volne pole MIMO TZ
            if zbyva >= 1 and v_tz_pri_bloku:
                c['v_TZ_pri_bloku'] += 1
                kam = any(tz[(bp[0]+dx, bp[1]+dy)] == 0
                          and (bp[0]+dx, bp[1]+dy) not in occupied
                          for dx, dy in DIRS)
                if kam:
                    c['MEL_KAM'] += 1
                    if not v_tz_na_konci:
                        c['ODESEL'] += 1          # ⭐ tohle stary strop nevidel
                    else:
                        c['ZUSTAL'] += 1
            # FOLLOW-UP: skoncil na UVOLNENEM poli obrance?
            fs = pushed_from.get(blocked_target.get(pid))
            if fs is not None:
                c['bloku_s_cilem'] += 1
                if end == fs:
                    c['followup_vzat'] += 1
                elif end == bp:
                    c['followup_odmitnut_stoji'] += 1
                else:
                    c['followup_odmitnut_odesel'] += 1
    return c

if __name__ == '__main__':
    root = sys.argv[1]
    files = sorted(glob.glob(f'{root}/g*.json.gz'))
    if not files:
        raise SystemExit(f'zadne hry v {root}')
    with Pool(10) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r is not None]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = len(rows); n = tot['blitzu']
    P = lambda k, d: (100.0*tot[k]/d if d else 0.0)
    print(f"=== {root}")
    print(f"her {g}   blitzu {n}  = {n/g:.2f}/hru")
    print(f"  pokracoval v pohybu PO bloku      {tot['pokracoval_po_bloku']:7d}  "
          f"{P('pokracoval_po_bloku', n):5.1f} %   {tot['pokracoval_po_bloku']/g:.2f}/hru")
    print()
    print("  (2) STROP -- mel MA, stal v TZ, a MEL KAM odejit:")
    print(f"      prilezitosti                  {tot['MEL_KAM']:7d}  "
          f"{P('MEL_KAM', n):5.1f} %   {tot['MEL_KAM']/g:.2f}/hru")
    print(f"      ⭐ ODESEL (konci mimo TZ)      {tot['ODESEL']:7d}  "
          f"{P('ODESEL', tot['MEL_KAM']):5.1f} % z prilezitosti")
    print(f"      ZUSTAL v kontaktu             {tot['ZUSTAL']:7d}  "
          f"{P('ZUSTAL', tot['MEL_KAM']):5.1f} %   {tot['ZUSTAL']/g:.2f}/hru  <= zbyvajici strop")
    print()
    print("  (3) FOLLOW-UP:")
    b = tot['bloku_s_cilem']
    print(f"      bloku, kde obrance POLE UVOLNIL {b:7d}  {P('bloku_s_cilem', n):5.1f} % z blitzu")
    print(f"      vzat (stoji na poli obrance)  {tot['followup_vzat']:7d}  {P('followup_vzat', b):5.1f} %")
    print(f"      odmitnut, zustal stat         {tot['followup_odmitnut_stoji']:7d}  {P('followup_odmitnut_stoji', b):5.1f} %")
    print(f"      odmitnut, odesel jinam        {tot['followup_odmitnut_odesel']:7d}  {P('followup_odmitnut_odesel', b):5.1f} %")
