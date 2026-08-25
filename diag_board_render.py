#!/usr/bin/env python3
"""VYKRESLENÍ POZICE Z KORPUSU — jednotné značení, ať se nevymýšlí pokaždé znovu.

Vzniklo 25.08. při rozboru situace pro Q4, kde se ukázaly TŘI vady vykreslení:
  ⛔ ŠPATNÁ ORIENTACE — popisek tvrdil „naše endzóna vpravo", a byla vlevo.
     ⇒ směr se tu ODVOZUJE Z DAT (kde padl TD), ne z domněnky.
  ⛔ OŘÍZNUTÍ KOLEM MÍČE — výsek x=12..16 vyřízl volného příjemce, tedy přesně
     to nejdůležitější. ⇒ default je CELÁ deska; ořez se musí říct výslovně.
  ⛔ MATOUCÍ KÓDY — `DL` = Longbeard, `WL` = jejich Lineman; obojí „L".
     ⇒ kódy jsou teď podle ROLE, ne podle písmene jména.

KÓDY (dva znaky, velká = stojí, malá druhá = leží/omráčen, `o` = drží míč):
  naši:   LB Longbeard · LG Longbeard+Guard · BZ Blitzer · TS Troll Slayer
          RN Runner · DR Deathroller
  jejich: lm Lineman · wd Wardancer · ct Catcher · th Thrower · tr Treeman
          gr Gutter Runner · bo Black Orc · og Ogre
"""
import gzip, json, sys

NAS = {'Longbeard': 'LB', 'Longbeard +Guard': 'LG', 'Blitzer +Guard+Tackle': 'BZ',
       'Blitzer +Guard': 'BZ', 'Blitzer +Mighty Blow': 'BZ', 'Blitzer ball-hunter': 'BZ',
       'Troll Slayer +Guard+Tackle': 'TS', 'Runner +Block': 'RN', 'Deathroller': 'DR'}
JEJICH = {'Lineman': 'lm', 'Wardancer ball-hunter': 'wd', 'Wardancer +Side Step': 'wd',
          'Catcher +Block': 'ct', 'Thrower +Block': 'th', 'Treeman +Guard': 'tr',
          'Gutter Runner +Sure Feet': 'gr', 'Black Orc +Guard+Block': 'bo',
          'Ogre +Block': 'og', 'Lineman +Wrestle': 'lw'}


def our_endzone(d, we_are_home):
    """Kam SKÓRUJE soupeř = kterou endzónu bráníme. Odvozeno z reálného TD."""
    for t in d['turn_logs']:
        if t.get('touchdown'):
            scorer_home = (t.get('active_team') == 'home')
            bx = t.get('ball_x', -1)
            if bx is not None and bx >= 0:
                target = 0 if bx < 13 else 25          # kam skóroval on
                return target if scorer_home != we_are_home else (25 - target)
    return None


def render(path, idx, we_home=True, xlo=0, xhi=25):
    d = json.load(gzip.open(path))
    t = d['turn_logs'][idx]
    ours = t['home_players'] if we_home else t['away_players']
    them = t['away_players'] if we_home else t['home_players']
    ez = our_endzone(d, we_home)
    cell = {}
    # ⭐ NAŠI vždy VELKÝMI, JEJICH vždy malými -- první pokus 25.08. dával
    # velká i jejich stojícím, takže `LB` (náš Longbeard) a `LM` (jejich
    # Lineman) vypadaly jako totéž. Stav jde do TŘETÍHO znaku, ne do velikosti.
    for p in ours:
        if p['state'] in (0, 1, 2):
            cell[(p['x'], p['y'])] = (NAS.get(p['name'], '??').upper(),
                                      p.get('has_ball', False), p['state'])
    for p in them:
        if p['state'] in (0, 1, 2):
            cell[(p['x'], p['y'])] = (JEJICH.get(p['name'], '??').lower(),
                                      p.get('has_ball', False), p['state'])
    print(f"{path.split('/')[-1]}  idx {idx}  půle {t['half']} kolo {t['turn']}  "
          f"hraje: {'MY' if (t.get('active_team')=='home')==we_home else 'ONI'}")
    if ez is not None:
        smer = '←' if ez == 0 else '→'
        print(f"naše endzóna x={ez} {smer} (ODVOZENO z reálného TD v téže hře, ne z domněnky)")
    print()
    print('       ' + ''.join(f'{x:^4}' for x in range(xlo, xhi + 1)))
    line = '      +' + '---+' * (xhi - xlo + 1)
    ys = sorted({y for (x, y) in cell if xlo <= x <= xhi})
    for y in range(max(0, min(ys) - 1), min(15, max(ys) + 2)):
        print(line)
        row = f' y={y:<3}|'
        for x in range(xlo, xhi + 1):
            v = cell.get((x, y))
            if v:
                mark = 'o' if v[1] else ('.' if v[2] != 0 else ' ')
                row += f'{v[0]}{mark}|'
            else:
                row += '   |'
        print(row)
    print(line)
    print('\nVELKÁ = NAŠI · malá = JEJICH · 3. znak:  o = drží míč,  . = leží/omráčen')
    if (xlo, xhi) != (0, 25):
        print(f'⚠️ OŘEZ x={xlo}..{xhi} — mimo výsek MŮŽE STÁT NĚKDO DŮLEŽITÝ')


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        print("použití: diag_board_render.py <hra.json.gz> <idx> [we_home=1] [xlo] [xhi]")
        raise SystemExit(1)
    a = sys.argv
    render(a[1], int(a[2]), bool(int(a[3])) if len(a) > 3 else True,
           int(a[4]) if len(a) > 4 else 0, int(a[5]) if len(a) > 5 else 25)
