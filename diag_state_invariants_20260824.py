#!/usr/bin/env python3
"""KONTROLA INVARIANTŮ STAVU na hotovém korpusu.

Vzniklo 24.08.2026 po nálezu "dva hráči na jednom poli" v Ball & Chain.
Uživatel se zeptal: "a co balon, nevyletí podobný problém?"

Rozbitý invariant není odchylka od pravidel -- je to stav, o kterém nikdo
neví, že je nemožný, takže se s ním počítá dál: getPlayerAtPosition vrátí
jednoho ze dvou, asistence se přepočtou, cesta vede skrz tělo.

⭐ A u míče se to UŽ JEDNOU STALO: komentář v game_simulator.cpp popisuje hru
g0040 -- "an entire second half of 108 moves, 19 blocks, three fouls and a
casualty, played WITHOUT A BALL", protože výkop dopadl na hráče, ten ho
neudržel, a míč zůstal mimo hřiště na (-1,-1). Našlo se to náhodou, 1 ze 120.
"""
import gzip, json, glob, os, collections
from multiprocessing import Pool

ON_PITCH = (0, 1, 2)   # STANDING, PRONE, STUNNED

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    bad = collections.Counter()
    for t in d['turn_logs']:
        players = t['home_players'] + t['away_players']
        # --- (1) dva hráči na jednom poli ---
        seen = {}
        for p in players:
            if p['state'] not in ON_PITCH:
                continue
            key = (p['x'], p['y'])
            if key in seen:
                bad['dva_hraci_na_poli'] += 1
            seen[key] = p['id']
            # --- (2) hráč na hřišti mimo hřiště ---
            if not (0 <= p['x'] <= 25 and 0 <= p['y'] <= 14):
                bad['hrac_na_pitchi_mimo_souradnice'] += 1
        # --- (3) míč ---
        held = t.get('ball_held')
        cid = t.get('ball_carrier_id', -1)
        bx, by = t.get('ball_x', -1), t.get('ball_y', -1)
        onpitch_ball = (0 <= bx <= 25 and 0 <= by <= 14)
        byid = {p['id']: p for p in players}
        if held:
            if cid not in byid:
                bad['drzi_ho_nikdo'] += 1
            else:
                c = byid[cid]
                if c['state'] not in ON_PITCH:
                    bad['nosic_neni_na_hristi'] += 1
                if (c['x'], c['y']) != (bx, by):
                    bad['mic_jinde_nez_nosic'] += 1
                if not c.get('has_ball', False):
                    bad['nosic_nevi_ze_ma_mic'] += 1
            if not onpitch_ball:
                bad['drzeny_mic_mimo_hriste'] += 1
        else:
            if cid != -1:
                bad['nedrzeny_mic_ma_nosice'] += 1
            if not onpitch_ball:
                bad['MIC_NIKDE'] += 1
            # volný míč na poli stojícího hráče: legální jen chvilkově
            # (neřešíme), ale míč pod hráčem MIMO hřiště ne
        # --- (4) víc hráčů tvrdí, že mají míč ---
        carriers = [p['id'] for p in players if p.get('has_ball')]
        if len(carriers) > 1:
            bad['vic_nosicu_najednou'] += 1
        if carriers and not held:
            bad['nosic_bez_drzeni'] += 1
    return bad

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r is not None]
    total = collections.Counter()
    games_with = collections.Counter()
    for r in rows:
        total += r
        for k in r:
            games_with[k] += 1
    print(f"her: {len(rows)}")
    print()
    if not total:
        print("✅ ŽÁDNÝ ROZBITÝ INVARIANT")
    else:
        print(f"{'invariant':<32}{'výskytů':>10}{'her':>8}{'% her':>9}")
        for k, v in total.most_common():
            print(f"{k:<32}{v:>10}{games_with[k]:>8}{100.0*games_with[k]/len(rows):>8.2f}%")
