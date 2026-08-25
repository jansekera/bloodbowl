#!/usr/bin/env python3
"""STROP LEAPU — kolik pozic by skok otevřel, a chůze na ně nedosáhne?

Zadání z návrhu Fable 25.08. (`evidence/fable_leap_plan_20260825.md`).
⭐ Smysl: rozhodnout, jestli je chybějící makro pro Leap VELKÁ VADA, nebo
HYGIENA -- dřív, než se do něj investuje den práce. Táž role, jakou u P31
sehrálo M9 (4,09 blitzu na hru ⇒ velká vada, oprava se smí dělat).

BB2016 ř. 8270-8283: skok jde na PRÁZDNÉ pole do vzdálenosti 2 (i přes tělo),
stojí DVĚ pole pohybu, hází se na AG bez modifikátorů, 1× za kolo.
⇒ Skok NEPRODLUŽUJE DOSAH, jen mění, KUDY se dá projít.

Proto se počítá přesně tohle: pole do vzdálenosti 2, které je PRÁZDNÉ, ale
chůze přes prázdná pole se na ně v rozpočtu MA+2 nedostane. To je jediná
množina, kterou skok přidává.

⚠️ KONZERVATIVNÍ ZÁMĚRNĚ: rozpočet MA+2 je PLNÝ pohyb od začátku kola (korpus
neveze zbývající MA), takže se dosah chůze NADHODNOCUJE ⇒ nález je DOLNÍ mez.
⚠️ Neřeší se dodge: chůze ven z TZ stojí hod, skok ne (ř. 8277-8278). Tím se
hodnota skoku podceňuje podruhé.
"""
import gzip, json, glob, os, sys, collections
from multiprocessing import Pool

W, H = 26, 15
ON_PITCH = (0, 1, 2)          # STANDING, PRONE, STUNNED
STANDING = 0


def _reachable(occupied, start, budget):
    """Pole dosažitelná CHŮZÍ přes prázdná pole, 8 směrů, cena 1 za krok."""
    seen = {start: 0}
    q = collections.deque([start])
    while q:
        x, y = q.popleft()
        d = seen[(x, y)]
        if d >= budget:
            continue
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    continue
                nx, ny = x + dx, y + dy
                if not (0 <= nx < W and 0 <= ny < H):
                    continue
                if (nx, ny) in occupied or (nx, ny) in seen:
                    continue
                seen[(nx, ny)] = d + 1
                q.append((nx, ny))
    return seen


def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    # která strana je wood-elf
    we_home = d['home_race'] == 'wood-elf'
    ez_x = 25 if we_home else 0          # kam wood-elf skóruje
    c = collections.Counter()
    c['games'] = 1
    for t in d['turn_logs']:
        active_home = t.get('active_team', 0) == 0
        if active_home != we_home:
            continue                      # jen kola wood-elfa
        c['turns'] += 1
        mine = t['home_players'] if we_home else t['away_players']
        allp = t['home_players'] + t['away_players']
        occupied = {(p['x'], p['y']) for p in allp if p['state'] in ON_PITCH}
        for p in mine:
            if p['name'] != 'Wardancer' or p['state'] != STANDING:
                continue
            c['wardancer_turns'] += 1
            start = (p['x'], p['y'])
            budget = p['ma'] + 2          # MA + 2 GFI (Sprint wardancer nemá)
            if budget < 2:
                continue
            walk = _reachable(occupied, start, budget)
            # pole, která přidá SKOK: prázdná, do vzdálenosti 2, chůzí nedosažitelná
            gained, gained_fwd = [], []
            for dx in range(-2, 3):
                for dy in range(-2, 3):
                    if dx == 0 and dy == 0:
                        continue
                    sq = (start[0] + dx, start[1] + dy)
                    if not (0 <= sq[0] < W and 0 <= sq[1] < H):
                        continue
                    if sq in occupied or sq in walk:
                        continue
                    gained.append(sq)
                    # A-fwd: blíž k soupeřově endzóně než COKOLI dosažitelného chůzí
                    best_walk = min((abs(s[0] - ez_x) for s in walk), default=99)
                    if abs(sq[0] - ez_x) < best_walk:
                        gained_fwd.append(sq)
            if gained:
                c['A_turns'] += 1
                c['A_squares'] += len(gained)
            if gained_fwd:
                c['A_fwd_turns'] += 1
                if p.get('has_ball'):
                    c['A_carrier_turns'] += 1
    return c


if __name__ == '__main__':
    root = sys.argv[1] if len(sys.argv) > 1 else 'crosses_20260821_data'
    files = sorted(glob.glob(os.path.join(root, '*wood-elf*', 'g*.json.gz')))
    if not files:
        print(f"⛔ ŽÁDNÉ HRY v {root}/*wood-elf* — kontrola NEPROBĚHLA")
        raise SystemExit(2)
    print(f"korpus: {root}   souborů: {len(files)}")
    # ⚠️ 25.08.: noc drží 8 z 12 jader. Tři workery + nice necháváji stroj
    # dýchat; čtyři už ho vytíží úplně a noc se prodlouží. Rozbor, který
    # zdrží měření, je špatný obchod.
    os.nice(19)
    with Pool(3) as pool:
        rows = [r for r in pool.map(one, files, chunksize=25) if r is not None]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = max(tot['games'], 1)
    print()
    print(f"her: {tot['games']}   kol wood-elfa: {tot['turns']}   "
          f"aktivací wardancera: {tot['wardancer_turns']}")
    print()
    print(f"{'veličina':<44}{'celkem':>10}{'na hru':>10}")
    for k, label in (('A_turns',        'A  — skok otevře pole, chůze ne'),
                     ('A_fwd_turns',    'A-fwd — a je BLÍŽ k endzóně'),
                     ('A_carrier_turns',' z toho wardancer NESE MÍČ'),
                     ('A_squares',      '   (polí celkem, ne aktivací)')):
        print(f"{label:<44}{tot[k]:>10}{tot[k]/g:>10.3f}")
    print()
    print("⭐ ROZHODOVACÍ KRITÉRIUM (předregistrováno v návrhu Fable 25.08.):")
    print("   A-fwd na hru se čte proti metru M9 = 4,09 blitzu/hru (VELKÁ vada).")
    print(f"   Změřeno: {tot['A_fwd_turns']/g:.3f}/hru ⇒ ", end="")
    v = tot['A_fwd_turns'] / g
    print("VELKÁ VADA" if v >= 2.0 else ("STŘEDNÍ" if v >= 0.5 else
          "HYGIENA — zařadit ZA okruh POHYB"))
    print()
    print("⚠️ Je to DOLNÍ mez: rozpočet MA+2 se bere jako plný pohyb od začátku")
    print("   kola (nadhodnocuje chůzi) a dodge za odchod z TZ se nepočítá.")
