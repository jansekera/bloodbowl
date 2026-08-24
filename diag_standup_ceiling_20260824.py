#!/usr/bin/env python3
"""M4 -- JAKY STROP JESTE DRZI STAND-AND-GO?

Zadani z Fable auditu pohybu (24.08.): "Kolik % vstavani ma nevyuzity MA >= 1
a smysluplny cil?" Rekne, jak velky strop na namerenou hodnotu P45 porad drzi
to, ze makro umi hráče postavit JEN NA MISTE (ř. 670-671 dovoluji utratit
zbytek pohybu).

Data: crosses_20260821_data (18 000 her). Korpus je UZ PO oprave vstavani
(P45, 21.08.), takze meri prave ten zbyly strop, ne puvodni vadu.

Metoda -- cte se jen to, co je v zaznamu:
  * vstavani stoji 3 MA (ř. 689-691), takze VOLNY ZBYTEK = max(0, MA - 3);
    hráč s MA < 3 vstava hodem 4+ a zadny zbytek nema.
  * "sel po vstani?" = po udalosti STAND_UP nasleduje v TEMZE kole udalost
    MOVE tehoz hráče.
  * "mel kam?" = v dosahu (zbytek + 2 GFI) je volny mic, nebo vlastni nosic
    (eskorta), nebo souperova endzona, kdyz mic nese sam.
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
        players = {p['id']: p for p in t['home_players'] + t['away_players']}
        ball_held = t.get('ball_held')
        bx, by = t.get('ball_x', -1), t.get('ball_y', -1)
        cid = t.get('ball_carrier_id', -1)
        ev = t['events']
        # index prvniho MOVE po dane pozici pro daneho hráče
        for i, e in enumerate(ev):
            if e.get('type') != 'STAND_UP':
                continue
            pid = e.get('player_id')
            p = players.get(pid)
            if not p:
                continue
            c['standups'] += 1
            spare = max(0, p['ma'] - 3)
            if spare == 0:
                c['bez_zbytku_MA'] += 1
                continue
            c['se_zbytkem_MA'] += 1
            moved = any(e2.get('type') == 'MOVE' and e2.get('player_id') == pid
                        for e2 in ev[i+1:])
            reach = spare + 2
            px, py = p['x'], p['y']
            target = False
            if not ball_held and 0 <= bx <= 25:
                if dist(px, py, bx, by) <= reach:
                    target = True
            if not target and ball_held and cid in players:
                cp = players[cid]
                if cp['teamSide'] if False else True:
                    # vlastni nosic = eskorta
                    same = (cid in [q['id'] for q in t['home_players']]) == \
                           (pid in [q['id'] for q in t['home_players']])
                    if same and dist(px, py, cp['x'], cp['y']) <= reach:
                        target = True
            if not target and ball_held and cid == pid:
                ez = 25 if pid in [q['id'] for q in t['home_players']] else 0
                if abs(px - ez) <= reach:
                    target = True
            if moved:
                c['sel_po_vstani'] += 1
                if target: c['sel_a_mel_kam'] += 1
            else:
                c['NESEL'] += 1
                if target:
                    c['NESEL_A_MEL_KAM'] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]
    tot = collections.Counter()
    for r in rows: tot += r
    n = tot['standups']; sz = tot['se_zbytkem_MA']
    print(f"her: {len(rows)}   vstavani celkem: {n}  ({n/len(rows):.2f} na hru)")
    print()
    print(f"  bez zbytku MA (MA<=3):        {tot['bez_zbytku_MA']:>8}  {100*tot['bez_zbytku_MA']/n:5.1f} %")
    print(f"  SE ZBYTKEM MA >= 1:           {sz:>8}  {100*sz/n:5.1f} %")
    if sz:
        print()
        print(f"    z toho SEL po vstani:       {tot['sel_po_vstani']:>8}  {100*tot['sel_po_vstani']/sz:5.1f} %")
        print(f"    z toho NESEL:               {tot['NESEL']:>8}  {100*tot['NESEL']/sz:5.1f} %")
        print()
        print(f"  ⛔ NESEL, PRESTOZE MEL KAM:   {tot['NESEL_A_MEL_KAM']:>8}  "
              f"{100*tot['NESEL_A_MEL_KAM']/sz:5.1f} % vstavani se zbytkem")
        print(f"     (= {100*tot['NESEL_A_MEL_KAM']/n:5.1f} % VSECH vstavani, "
              f"{tot['NESEL_A_MEL_KAM']/len(rows):.2f} na hru)")
