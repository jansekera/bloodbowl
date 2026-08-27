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
import gzip, json, glob, os, sys, collections
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

    # --- (5) TOUCHDOWNY SEDÍ NA KONEČNÉ SKÓRE (26.08.2026) ---
    # Vzešlo z uživatelovy kontroly 25.08.: "když se TD na konci první půle
    # načte v kole 9 -- zkontroluj, jak se načte TD v kole 16."
    # ⭐ Ověřeno tehdy na 900 hrách: `touchdown` flagy sedí na výsledek 900/900,
    # ale POSLEDNÍ SNÍMEK SKÓRE jen 882/900 (98 %). Ve 2 % her se TD v posledním
    # kole hry nemá kde projevit -- žádné další kolo už není. Kdo počítá TD
    # z přírůstku skóre mezi snímky, SYSTEMATICKY ZTRÁCÍ PRÁVĚ TY POSLEDNÍ,
    # tedy to, oč u grindu jde.
    # Kontrola je proti KONEČNÉMU skóre hry, ne proti snímkům, a je per-strana:
    # samotný součet by neodhalil TD připsaný nesprávné straně.
    # ⛔⛔ JEN SOUČET, NE ROZDĚLENÍ PO STRANÁCH -- a to je zjištění, ne lenost.
    # Zkusil jsem 26.08. obojí a obě cesty k "kdo skóroval" jsou slepé:
    #   · `active_team` je ve snímku s TD flagem UŽ PŘEPNUTÝ na soupeře
    #     (g1273: active=away, ale míč drží hráč 8 = HOME)
    #   · `ball_carrier_id` je v 5,5 % TD snímků -1 -- míč je po touchdownu
    #     odebraný, takže nosič už neexistuje
    # ⇒ Ze záznamu kola SE NEDÁ ODVODIT, KTERÁ STRANA TD DALA. Kontrola po
    #   stranách by proto hlásila 10,3 % her jako rozbité, ačkoli engine je
    #   v pořádku -- byla by to vada KONTROLY vydávaná za vadu enginu, přesně
    #   to, čemu se tenhle soubor má bránit. Do fronty patří chybějící pole
    #   (rodina T5.33 "hasActed do turn_logs").
    # Součet naproti tomu drží: ověřeno 600/600 a 400/400 před zapsáním.
    td_total = sum(1 for t in d['turn_logs'] if t.get('touchdown'))
    if td_total != d.get('home_score', 0) + d.get('away_score', 0):
        bad['TD_flagy_nesedi_na_soucet_skore'] += 1

    return bad

if __name__ == '__main__':
    # 25.08.: cesta byla NATVRDO na crosses_20260821_data. Pustit tuhle kontrolu
    # na nový korpus by tiše zkontrolovalo STARÝ -- a vyšlo by "čisto", protože
    # ten opravdu čistý je. To je přesně třída z 24.08.: KONTROLA HLÍDÁ JINÝ
    # OBJEKT, NEŽ SI MYSLÍŠ (preflight hlídal jiné dvojice a fáze B umřela).
    # Default zůstává, aby staré volání dál platilo, ale dá se předat kořen.
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    pats = [os.path.join(root, '*', 'g*.json.gz'),   # crosses: podadresář na matchup
            os.path.join(root, 'g*.json.gz')]        # collect: hry přímo v kořeni
    files = sorted({f for pat in pats for f in glob.glob(pat)})
    print(f"korpus: {root}")
    if not files:
        print(f"⛔ ŽÁDNÉ HRY v {root} — kontrola NEPROBĚHLA (a to NENÍ 'čisto')")
        raise SystemExit(2)
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
