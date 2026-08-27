#!/usr/bin/env python3
"""M7 -- ČETNOST A VÝSLEDKY BLOKŮ DO OBRÁNCE S WRESTLE.

Zadání z Fable auditu pohybu (24.08.), položka B2: `estimateBlockFailChance`
NEZNÁ obranný Wrestle -- plánovač počítá „Both Down je pro útočníka s Blockem
bezpečný". Proti obránci s Wrestle to neplatí: BB2016 ř. 8670-8676 --
*„Both players are Placed Prone in their respective squares EVEN IF ONE OR BOTH
HAVE THE BLOCK SKILL."* ⇒ útočník s Blockem jde k zemi taky.

Kdo Wrestle má: skavení TV1200 `Lineman +Wrestle` ×2. Jinde nikdo.

⚠️ KORPUS JE Z 21.08., tedy PŘED opravou vybírače kostky (24.08., `cac87c3d`).
Tehdy obránce Wrestle použil VŽDY, útočník s Blockem nikdy. ⇒ tohle měří,
kolik ta vada stála PŘED opravou -- a tedy jak velký je strop pro B2.

KÓDOVÁNÍ KOSTKY (odvozeno z dat 25.08., ne z hlavičky):
  0 = Attacker Down · 1 = Both Down · 2 = Pushed
  3 = Defender Stumbles · 4 = Defender Down
"""
import gzip, json, glob, os, sys, collections
from multiprocessing import Pool

BOTH_DOWN = 1


def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    c = collections.Counter()
    c['games'] = 1
    for t in d['turn_logs']:
        names = {p['id']: p['name'] for p in t['home_players'] + t['away_players']}
        ev = t.get('events', [])
        for i, e in enumerate(ev):
            if e['type'] != 'BLOCK':
                continue
            att, dfn = e['player_id'], e['target_id']
            dn = names.get(dfn, '')
            wr = 'Wrestle' in dn
            tag = 'wrestle' if wr else 'jiny'
            c[f'{tag}|bloku'] += 1
            if e['roll'] != BOTH_DOWN:
                continue
            c[f'{tag}|both_down'] += 1
            # ⛔ NEHLEDAT JEN `KNOCKED_DOWN` (past z 25.08.): Wrestle je
            # PLACED PRONE, ne knockdown -- nemá hod na zbroj, takže se
            # `KNOCKED_DOWN` NEEMITUJE. Emituje se `SKILL` na obránci a obě
            # těla prostě přejdou do stavu 1. Kdo hledá jen knockdown, napočítá
            # „nic ve 100 %" a vypadá to úplně věrohodně.
            attdown = defdown = wrestle_used = False
            for x in ev[i + 1:]:
                if x['type'] == 'BLOCK':
                    break
                if x['type'] == 'SKILL' and x['player_id'] == dfn:
                    wrestle_used = True
                if x['type'] == 'KNOCKED_DOWN':
                    if x['player_id'] == att:
                        attdown = True
                    elif x['player_id'] == dfn:
                        defdown = True
            if wrestle_used:
                # Wrestle: OBA na zem, bez ohledu na Block (ř. 8670-8676)
                c[f'{tag}|oba k zemi'] += 1
            elif attdown and defdown:
                c[f'{tag}|oba k zemi'] += 1
            elif attdown:
                c[f'{tag}|jen utocnik'] += 1
            elif defdown:
                c[f'{tag}|jen obrance'] += 1
            else:
                c[f'{tag}|nic'] += 1
    return c


if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    files = sorted(glob.glob(os.path.join(root, '*skaven*', 'g*.json.gz')))
    if not files:
        print(f"⛔ ŽÁDNÉ HRY se skavenem v {root} — NEPROBĚHLO")
        raise SystemExit(2)
    print(f"korpus: {root}   souborů (dvojice se skavenem): {len(files)}")
    os.nice(19)
    with Pool(3) as pool:
        tot = collections.Counter()
        for r in pool.imap_unordered(one, files, chunksize=100):
            if r:
                tot += r
    g = tot['games']
    print(f"her: {g}\n")
    print(f"{'obránce':<12}{'bloků':>9}{'/hru':>8}{'Both Down':>11}{'podíl':>8}")
    for tag, lab in (('wrestle', 's Wrestle'), ('jiny', 'ostatní')):
        b, bd = tot[f'{tag}|bloku'], tot[f'{tag}|both_down']
        print(f"{lab:<12}{b:>9}{b/g:>8.2f}{bd:>11}{100*bd/b if b else 0:>7.1f}%")
    print(f"\n=== CO SE PŘI BOTH DOWN STALO ===")
    print(f"{'obránce':<12}{'n':>7}{'oba k zemi':>12}{'jen útočník':>13}{'jen obránce':>13}{'nic':>8}")
    for tag, lab in (('wrestle', 's Wrestle'), ('jiny', 'ostatní')):
        n = tot[f'{tag}|both_down'] or 1
        row = [tot[f'{tag}|{k}'] for k in ('oba k zemi', 'jen utocnik', 'jen obrance', 'nic')]
        print(f"{lab:<12}{n:>7}" + ''.join(f"{100*v/n:>12.1f}%" for v in row))
    n = tot['wrestle|both_down'] or 1
    bad = tot['wrestle|oba k zemi'] + tot['wrestle|jen utocnik']
    print(f"\n⭐ STROP PRO B2: u obránce s Wrestle skončilo Both Down pádem NAŠEHO těla")
    print(f"   v {bad} z {n} případů ({100*bad/n:.1f} %) = {bad/g:.3f} na hru.")
    print(f"   Plánovač přitom Both Down proti útočníkovi s Blockem počítal jako BEZPEČNÝ.")
    print(f"\n⚠️ Korpus je z 21.08., PŘED opravou vybírače (24.08.). Měří tedy strop,")
    print(f"   ne dnešní stav.")
