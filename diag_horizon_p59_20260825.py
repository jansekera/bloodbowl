#!/usr/bin/env python3
"""P59 — HORIZONT: co dokáže soupeř ve SVÉM PŘÍŠTÍM TAHU, když míč držíme my.

Zadání z rozhovoru 25.08. Uživatel: *„nic nemusíš odhadovat — co udělá soupeř,
vždy dopočítáš, kam dojde; jestli má možnost přihrát na někoho v dosahu TD…
vždy ti stačí tvoje aktuální kolo a co na to soupeř v jeho následujícím."*

⭐ VĚTEV (A): míč držíme MY, takže soupeř musí ZAČÍT ODEBRÁNÍM. Cena odebrání
se odečítá — blitz stojí přiblížení + 1 pole na blok, a míč se pak musí zvednout.
*(Větev (B), kdy míč už má, je čistý dosah a tady se nepočítá.)*

⛔ NEPOTŘEBUJE OTTD. Uživatel 25.08.: *„elfové OTTD nemají — jejich je dát TD
hned ten sám tah, co seberou míč."* Stačí blitz + příjemce už stojící v dosahu.

⚠️ JE TO HORNÍ MEZ, a to schválně:
  · ignoruje tackle zóny, dodge, GFI hody i hod na sebrání míče
  · ignoruje, že mu v cestě stojíme
  · předpokládá, že blitz nosiče uspěje
⇒ číslo říká „kolik situací tu MOŽNOST vůbec dává", ne „jak často to vyjde".
Dolní mez by chtěla simulaci; tahle horní mez stačí na otázku, jestli se tím
má plánovač vůbec zabývat.
"""
import gzip, json, glob, os, sys, collections
from multiprocessing import Pool

W, H = 26, 15
STANDING = 0
LONG_BOMB = 13          # nejdelší hod, BB2016 rozsahy pasu


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    c = collections.Counter()
    for me_home in (True, False):
        opp = ar if me_home else hr
        me_side = 'home' if me_home else 'away'
        their_ez = 25 if not me_home else 0     # kam skóruje SOUPEŘ
        for t in d['turn_logs']:
            # zajímá nás stav na začátku NAŠEHO tahu: co udělá on POTOM
            if t.get('active_team') != me_side:
                continue
            mine = t['home_players'] if me_home else t['away_players']
            them = t['away_players'] if me_home else t['home_players']
            car = next((p for p in mine if p.get('has_ball')), None)
            if car is None:
                continue
            C = (car['x'], car['y'])
            c[f'{opp}|kol'] += 1
            std = [p for p in them if p['state'] == STANDING]
            # (1) dosáhne někdo na nosiče a zbude mu 1 pole na blok?
            takers = [p for p in std if cheb((p['x'], p['y']), C) <= p['ma'] + 1]
            if not takers:
                continue
            c[f'{opp}|umi_odebrat'] += 1
            # (2a) BĚH: někdo dojde k míči a odtud do endzóny
            run = False
            for p in std:
                P = (p['x'], p['y'])
                if cheb(P, C) + abs(C[0] - their_ez) <= p['ma'] + 2:
                    run = True
                    break
            # (2b) PŘIHRÁVKA: někdo zvedne míč (kdokoli dosáhne na C)
            #      a hodí na příjemce, který sám doskáče do endzóny
            pas = False
            if any(cheb((p['x'], p['y']), C) <= p['ma'] + 2 for p in std):
                for r in std:
                    R = (r['x'], r['y'])
                    if cheb(R, C) > LONG_BOMB:
                        continue
                    if abs(R[0] - their_ez) <= r['ma'] + 2:
                        pas = True
                        break
            if run:
                c[f'{opp}|behem'] += 1
            if pas:
                c[f'{opp}|prihravkou'] += 1
            if run or pas:
                c[f'{opp}|SKORUJE'] += 1
            if pas and not run:
                c[f'{opp}|jen_prihravkou'] += 1
    return c


if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    files = []
    for dd in sorted(glob.glob(os.path.join(root, '*/'))):
        files += sorted(glob.glob(os.path.join(dd, 'g*.json.gz')))[:400]
    if not files:
        print(f"⛔ ŽÁDNÉ HRY v {root}")
        raise SystemExit(2)
    print(f"korpus: {root}   souborů: {len(files)} (napříč všemi dvojicemi)")
    os.nice(19)
    with Pool(3) as pool:
        tot = collections.Counter()
        for r in pool.imap_unordered(one, files, chunksize=50):
            if r:
                tot += r
    races = sorted({k.split('|')[0] for k in tot})
    print()
    print("VĚTEV (A): míč držíme MY. Co zvládne soupeř ve svém PŘÍŠTÍM tahu?")
    print()
    print(f"{'soupeř':<11}{'kol':>8}{'umí odebrat':>13}{'a SKÓRUJE':>12}"
          f"{'z toho během':>14}{'přihrávkou':>12}{'JEN přihrávkou':>16}")
    for r in races:
        n = tot[f'{r}|kol']
        if not n:
            continue
        od = tot[f'{r}|umi_odebrat']
        sk = tot[f'{r}|SKORUJE']
        print(f"{r:<11}{n:>8}{100*od/n:>12.1f}%{100*sk/n:>11.1f}%"
              f"{100*tot[f'{r}|behem']/n:>13.1f}%{100*tot[f'{r}|prihravkou']/n:>11.1f}%"
              f"{100*tot[f'{r}|jen_prihravkou']/n:>15.1f}%")
    print()
    print("⚠️ HORNÍ MEZ: bez tackle zón, dodge, GFI hodů i hodu na sebrání míče;")
    print("   předpokládá se, že blitz nosiče vyjde. Říká, kolik situací tu")
    print("   MOŽNOST dává — ne jak často se povede.")
    print("⭐ Sloupec 'JEN přihrávkou' je ten, na který naše doktrína nemá NIC:")
    print("   běh se dá zablokovat tělem, hod ne.")
