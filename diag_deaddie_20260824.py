#!/usr/bin/env python3
# T5.13c -- JAK CASTO BY WRESTLE NA TRPASLIKOVI VUBEC MELO CO DELAT?
#
# Uzivatel 24.08.: "Wrestle je pri prolomeni dulezity pro zvyseni
# pravdepodobnosti shozeni braniciho pri pokusu prolomeni zdi."
# Mechanika: proti obranci S BLOCKEM je BOTH_DOWN MRTVA KOSTKA -- nestane se
# nic. Utocnik s Block+Wrestle si ji smi precist jako Wrestle a obrance SLOZIT
# (l. 8672-8676). Rozhodnuti o slotu (T5.13c) se dosud delalo bez cisla.
#
# Merime: bloky, kde UTOCI TRPASLIK (vsechny jeho pozice maji Block) a OBRANCE
# MA BLOCK. Tam a jen tam je Both Down dnes mrtvy a Wrestle by ho ozivil.
#
# ⚠️ Korpus je z preddnesniho enginu: BOTH_DOWN jeste odsouval obrance (N8),
# a `scoreFace` Wrestle neumel ocenit. Cetnosti plati, vysledky ne.
import gzip, json, glob, collections
from multiprocessing import Pool

BLOCK_POS = {
    'Blitzer +Guard+Tackle', 'Longbeard', 'Longbeard +Guard', 'Runner +Block',
    'Troll Slayer +Guard+Tackle', 'Blitzer +Guard', 'Blitzer +Mighty Blow',
    'Blitzer ball-hunter', 'Catcher +Block', 'Ogre +Block', 'Thrower +Block',
    'Black Orc +Guard+Block',
}
FACE = {0: 'ATTACKER_DOWN', 1: 'BOTH_DOWN', 2: 'PUSHED',
        3: 'DEFENDER_STUMBLES', 4: 'DEFENDER_DOWN'}

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    if 'dwarf' not in (hr, ar):
        return None
    c = collections.Counter()
    c['her'] = 1
    for t in d['turn_logs']:
        home_ids = {p['id'] for p in t['home_players']}
        who = {p['id']: p['name'] for p in t['home_players'] + t['away_players']}
        for e in t['events']:
            if e.get('type') != 'BLOCK':
                continue
            aid = e.get('player_id'); tid = e.get('target_id', -1)
            arace = hr if aid in home_ids else ar
            if arace != 'dwarf':
                continue
            c['bloku_trpaslika'] += 1
            defblock = who.get(tid, '') in BLOCK_POS
            face = FACE.get(e.get('roll', -1), '?')
            if defblock:
                c['proti_Blocku'] += 1
                if face == 'BOTH_DOWN':
                    c['MRTVA_KOSTKA'] += 1
            else:
                if face == 'BOTH_DOWN':
                    c['BD_proti_bezBlocku'] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = tot['her']; n = tot['bloku_trpaslika']
    print("her s trpaslikem: %d" % g)
    print("bloku, kde utoci trpaslik: %d  (%.1f na hru)" % (n, n/g))
    print()
    print("  proti obranci S BLOCKEM:      %8d   %5.1f %%   %5.2f na hru"
          % (tot['proti_Blocku'], 100.0*tot['proti_Blocku']/n, tot['proti_Blocku']/g))
    print("  z toho padl BOTH_DOWN:        %8d   %5.1f %% z nich   %5.2f na hru"
          % (tot['MRTVA_KOSTKA'], 100.0*tot['MRTVA_KOSTKA']/max(1,tot['proti_Blocku']),
             tot['MRTVA_KOSTKA']/g))
    print()
    print("  (pro srovnani) BOTH_DOWN proti obranci BEZ Blocku: %d  %5.2f na hru"
          % (tot['BD_proti_bezBlocku'], tot['BD_proti_bezBlocku']/g))
    print()
    print("⇒ MRTVA KOSTKA = %.2f na hru. Presne tady by Wrestle na trpaslikovi" % (tot['MRTVA_KOSTKA']/g))
    print("  zmenil 'nestane se nic' na 'obrance lezi' (a utocnik take).")
