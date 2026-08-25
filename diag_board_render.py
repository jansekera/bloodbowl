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

# ⭐ ZNAČENÍ PODLE UŽIVATELE (25.08.): RASOVÉ PÍSMENO + ROLE.
#    D dwarf · W wood-elf · S skaven · O orc · H human
#    role: L lineman/Longbeard · LG Longbeard+Guard · B Blitzer · T Troll Slayer
#          R Runner · W Wardancer · C Catcher · TH Thrower · TR Treeman
#          G Gutter Runner · BO Black Orc · OG Ogre · DR Deathroller
ROLE = {'Longbeard': 'L', 'Longbeard +Guard': 'LG', 'Blitzer +Guard+Tackle': 'B',
        'Blitzer +Guard': 'B', 'Blitzer +Mighty Blow': 'B', 'Blitzer ball-hunter': 'B',
        'Troll Slayer +Guard+Tackle': 'T', 'Runner +Block': 'R', 'Deathroller': 'DR',
        'Lineman': 'L', 'Lineman +Wrestle': 'LW', 'Wardancer ball-hunter': 'W',
        'Wardancer +Side Step': 'W', 'Catcher +Block': 'C', 'Thrower +Block': 'TH',
        'Treeman +Guard': 'TR', 'Treeman': 'TR', 'Gutter Runner +Sure Feet': 'G',
        'Black Orc +Guard+Block': 'BO', 'Ogre +Block': 'OG'}
RASA = {'dwarf': 'D', 'wood-elf': 'W', 'skaven': 'S', 'orc': 'O', 'human': 'H'}


def kod(name, race, state, ball, acted=False):
    """RASA+ROLE; stunned malými; ležící `_`; ODEHRANÝ `-`; míč `o`.

    ⭐ DVA REŽIMY, a `-` patří jen do jednoho (uživatel 25.08.):
      · ROZBOR KORPUSU  -- snímek je ze ZAČÁTKU kola, nikdo ještě nehrál,
        a `hasActed` se do `turn_logs` stejně neukládá => `-` se nepoužije.
      · ŽIVÁ HRA        -- deska se překresluje UPROSTŘED tahu po každé
        aktivaci, aby bylo vidět, s kým už se hýbalo a nehýbalo se s ním
        podruhé. Tam `hasActed` k dispozici JE (`Player::hasActed`).
    ⇒ značka existuje tady, aby ji živý režim mohl použít beze změny formátu.
    """
    c = RASA.get(race, '?') + ROLE.get(name, '?')
    if state == 2:                     # STUNNED -> malými
        c = c.lower()
    if ball:
        return c + 'o'
    if state == 1:
        return c + '_'
    return c + ('-' if acted else '')


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
    r_ours = d['home_race'] if we_home else d['away_race']
    r_them = d['away_race'] if we_home else d['home_race']
    ez = our_endzone(d, we_home)
    cell = {}
    # ⭐ NAŠI vždy VELKÝMI, JEJICH vždy malými -- první pokus 25.08. dával
    # velká i jejich stojícím, takže `LB` (náš Longbeard) a `LM` (jejich
    # Lineman) vypadaly jako totéž. Stav jde do TŘETÍHO znaku, ne do velikosti.
    for p in ours:
        if p['state'] in (0, 1, 2):
            cell[(p['x'], p['y'])] = kod(p['name'], r_ours, p['state'],
                                         p.get('has_ball', False), p.get('has_acted', False))
    for p in them:
        if p['state'] in (0, 1, 2):
            cell[(p['x'], p['y'])] = kod(p['name'], r_them, p['state'],
                                         p.get('has_ball', False), p.get('has_acted', False))
    print(f"{path.split('/')[-1]}  idx {idx}  půle {t['half']} kolo {t['turn']}  "
          f"hraje: {'MY' if (t.get('active_team')=='home')==we_home else 'ONI'}")
    if ez is not None:
        smer = '←' if ez == 0 else '→'
        print(f"naše endzóna x={ez} {smer} (ODVOZENO z reálného TD v téže hře, ne z domněnky)")
    print()
    # ⭐ TACKLE ZÓNY (uživatel žádal už dřív a nebylo to zapsané -- 25.08.):
    #   · PRÁZDNÉ pole  -> kolik JEJICH zón na něj dosahuje (kam smím a za co)
    #   · NAŠE tělo     -> VŽDY `/n`, i `/0` -- v kolika JEJICH zónách stojí.
    #     ⭐ Lomítko je i důvod, proč jsou NAŠI na první pohled k poznání
    #        (uživatel 25.08.: „když u našich to lomítko bude vždy i s 0,
    #         tak poznám naše lépe").
    #   · JEJICH tělo   -> ⛔ BEZ čísla. „To je matoucí." (uživatel 25.08.)
    # Ležící ani omráčení zónu NEVYZAŘUJÍ (`exertsTacklezone`), proto jen state==0.
    st_ours = {(p['x'], p['y']) for p in ours if p['state'] == 0}
    st_them = {(p['x'], p['y']) for p in them if p['state'] == 0}
    def zon(sq, kdo):
        return sum(1 for q in kdo if max(abs(q[0]-sq[0]), abs(q[1]-sq[1])) == 1)
    for (x, y), v in list(cell.items()):
        if (x, y) in st_ours:                       # jen NAŠI stojící, vždy
            cell[(x, y)] = f'{v}/{zon((x, y), st_them)}'
    # ⭐ ořez se odvodí z OBSAZENÝCH polí, ne z hádání -- a kdo zůstane venku,
    #    ten se VYPÍŠE (25.08. jsem ořezem kolem míče vyhodil volného příjemce).
    occx = [x for (x, y) in cell]
    if (xlo, xhi) == (0, 25) and occx:
        xlo, xhi = max(0, min(occx) - 1), min(25, max(occx) + 1)
    venku = [(sq, v) for sq, v in cell.items() if not (xlo <= sq[0] <= xhi)]
    # ⭐ CELÉ buňky s ohraničením, i prázdné (uživatel 25.08.: „sice zabere víc
    #    prostoru, ale to je OK"). Šířka 5 dává kódům jako `DLG_` vzduch.
    # ⛔ 25.08.: hlavička byla CENTROVANÁ (`{x:^8}`), zatímco obsah buňky je
    #    zarovnaný DOLEVA -- číslo tak viselo o dva znaky vpravo od pole, které
    #    popisuje, a deska se četla o SLOUPEC VEDLE. Zarovnat na začátek buňky.
    print('       ' + ''.join(f' {x:<6} ' for x in range(xlo, xhi + 1)))
    line = '      +' + '-------+' * (xhi - xlo + 1)
    ys = sorted({y for (x, y) in cell if xlo <= x <= xhi})
    for y in range(max(0, min(ys) - 1), min(15, max(ys) + 2)):
        print(line)
        row = f' y={y:<3}|'
        for x in range(xlo, xhi + 1):
            v = cell.get((x, y))
            if v:
                row += f' {v:<6}|'
            else:
                z = zon((x, y), st_them)
                row += (f'   {z}   |' if z else '       |')
        print(row)
    print(line)
    print(f'\nD dwarf · W wood-elf · S skaven · O orc · H human   +role')
    print('stunned = malými · `_` = leží · `o` = drží míč')
    print('ČÍSLO v prázdném poli = kolik JEJICH tackle zón na něj dosahuje')
    print('`/n` = JEN u NAŠICH stojících, VŽDY (i `/0`) — v kolika jejich zónách stojí')
    print('`-` = ODEHRANÝ — jen v ŽIVÉ HŘE (překreslení uprostřed tahu).')
    print('   V korpusu se nepoužije: snímek je ze ZAČÁTKU kola a `hasActed` se neukládá.')
    if venku:
        print(f'⚠️ MIMO VÝSEK x={xlo}..{xhi} stojí: ' +
              ', '.join(f'{v}@{sq}' for sq, v in sorted(venku)))


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(__doc__)
        print("použití: diag_board_render.py <hra.json.gz> <idx> [we_home=1] [xlo] [xhi]")
        raise SystemExit(1)
    a = sys.argv
    render(a[1], int(a[2]), bool(int(a[3])) if len(a) > 3 else True,
           int(a[4]) if len(a) > 4 else 0, int(a[5]) if len(a) > 5 else 25)
