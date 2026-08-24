#!/usr/bin/env python3
"""M4b -- VSTAL, MEL KAM, A NEMUSEL BY ANI DODGOVAT (uzivatel 24.08.).

Zostreni M4. Dodge se hazi JEN pri opusteni pole, ktere je v souperove tackle
zone (ř. 480-486). Kdo nema vedle sebe STOJICIHO soupere, jde uplne ZADARMO --
zadny hod, zadne riziko, zadny turnover.

⇒ "vstal, mel kam jit, nemusel by ani dodgovat, a presto zustal stat" je
nejtvrdsi verze toho stropu: neni to opatrnost, je to CHYBEJICI VOLBA.
"""
import gzip, json, glob, collections
from multiprocessing import Pool

def dist(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    c = collections.Counter()
    for t in d['turn_logs']:
        home_ids = {p['id'] for p in t['home_players']}
        players = {p['id']: p for p in t['home_players'] + t['away_players']}
        ball_held = t.get('ball_held')
        bx, by = t.get('ball_x', -1), t.get('ball_y', -1)
        cid = t.get('ball_carrier_id', -1)
        ev = t['events']
        for i, e in enumerate(ev):
            if e.get('type') != 'STAND_UP':
                continue
            pid = e.get('player_id'); p = players.get(pid)
            if not p:
                continue
            spare = max(0, p['ma'] - 3)
            if spare == 0:
                continue
            if any(x.get('type') == 'MOVE' and x.get('player_id') == pid for x in ev[i+1:]):
                continue
            c['nesel'] += 1
            px, py = p['x'], p['y']
            mine_home = pid in home_ids
            in_tz = any(q['state'] == 0 and (q['id'] in home_ids) != mine_home
                        and dist(px, py, q['x'], q['y']) == 1
                        for q in players.values())
            reach = spare + 2
            target = False
            if not ball_held and 0 <= bx <= 25 and dist(px, py, bx, by) <= reach:
                target = True
            if not target and ball_held and cid in players and (cid in home_ids) == mine_home:
                cp = players[cid]
                if dist(px, py, cp['x'], cp['y']) <= reach:
                    target = True
            if not target and cid == pid:
                ez = 25 if mine_home else 0
                if abs(px - ez) <= reach:
                    target = True
            c['v_TZ' if in_tz else 'VOLNY'] += 1
            if target:
                c['mel_kam'] += 1
                c['mel_kam_ale_v_TZ' if in_tz else 'ZADARMO_A_MEL_KAM'] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]
    tot = collections.Counter()
    for r in rows: tot += r
    n = tot['nesel']; g = len(rows)
    print("her: %d" % g)
    print("vstal se zbytkem MA a NESEL:      %8d   %5.2f na hru" % (n, n/g))
    print()
    print("  z toho v souperove TZ (dodge):  %8d   %5.1f %%" % (tot['v_TZ'], 100*tot['v_TZ']/n))
    print("  z toho VOLNY (pohyb zdarma):    %8d   %5.1f %%" % (tot['VOLNY'], 100*tot['VOLNY']/n))
    print()
    print("  mel kam jit celkem:             %8d   %5.1f %%" % (tot['mel_kam'], 100*tot['mel_kam']/n))
    print("    ...ale musel by dodgovat:     %8d   %5.1f %%" % (tot['mel_kam_ale_v_TZ'], 100*tot['mel_kam_ale_v_TZ']/n))
    print()
    print("  ZADARMO A MEL KAM:              %8d   %5.1f %%   %5.2f na hru" %
          (tot['ZADARMO_A_MEL_KAM'], 100*tot['ZADARMO_A_MEL_KAM']/n, tot['ZADARMO_A_MEL_KAM']/g))
