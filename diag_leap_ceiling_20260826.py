"""Strop pro Leap: kolik POLÍ NAVÍC směrem k endzone otevře skok proti chůzi.

⭐ Proč se to musí měřit jako PŘÍLEŽITOST, ne jako realizace: v korpusu z 21.08.
byl Leap MRTVÝ KÓD (nabídku dostal až 24.08., F12), takže "kolikrát se skočilo"
je tam nutně nula. Měříme tedy, kolikrát skok otevřel cestu, kterou chůze neumí.

⚠️ HORNÍ MEZ, a schválně volná: BFS jde přes prázdná pole a ignoruje tackle zóny
(dodge), takže chůzi spíš NADhodnocuje -- a tím strop pro skok PODhodnocuje.
Když i tak vyjde vysoko, je to argument; když vyjde nízko, noc nemá co měřit.

Kritérium (zadání 25.08.): A-fwd/hru proti metru M9 = 4,09 blitzů/hru.
"""
import gzip, json, glob, sys
from collections import deque, Counter

W, H = 26, 15                      # hrací plocha
WARD = ("Wardancer",)              # jediní s Leap v korpusových rosterech

def reachable_walk(occ, start, ma):
    """Pole dosažitelná chůzí do `ma` kroků přes PRÁZDNÁ pole (8 směrů)."""
    seen = {start: 0}; q = deque([start])
    while q:
        p = q.popleft(); d = seen[p]
        if d >= ma: continue
        x, y = p
        for dx in (-1,0,1):
            for dy in (-1,0,1):
                if dx == dy == 0: continue
                n = (x+dx, y+dy)
                if not (0 <= n[0] < W and 0 <= n[1] < H): continue
                if n in occ or n in seen: continue
                seen[n] = d+1; q.append(n)
    return seen

def main(dirs, limit=None):
    games = 0
    opp = Counter()          # kolik kol mělo aspoň jednu příležitost
    fwd_sq = 0               # pole navíc SMĚREM K ENDZONE, celkem
    turns_we = 0             # kol wood-elfa s aspoň jedním stojícím wardancerem
    best = Counter()

    for d in dirs:
        for f in sorted(glob.glob(d + '/*.json.gz'))[:limit]:
            g = json.load(gzip.open(f)); games += 1
            we = 'home' if g['home_race'] == 'wood-elf' else 'away'
            # wood-elf útočí směrem: home -> rostoucí x, away -> klesající x
            fwd = 1 if we == 'home' else -1
            ez  = W-1 if we == 'home' else 0
            for s in g['turn_logs']:
                if s['active_team'] != we: continue   # ⛔ je to STRING 'home'/'away', ne 0/1
                mine  = s[we + '_players']
                other = s[('away' if we == 'home' else 'home') + '_players']
                occ = {(p['x'], p['y']) for p in mine + other
                       if p['state'] == 0 and p['x'] >= 0}
                wards = [p for p in mine
                         if p['name'].startswith(WARD) and p['state'] == 0 and p['x'] >= 0]
                if not wards: continue
                turns_we += 1
                found = 0
                for p in wards:
                    start = (p['x'], p['y']); ma = p['ma']
                    walk = reachable_walk(occ - {start}, start, ma)
                    # skok: cíl do 2 polí, PRÁZDNÝ, stojí 2 MA; pak chůze zbytkem
                    gain = set()
                    for dx in range(-2, 3):
                        for dy in range(-2, 3):
                            if max(abs(dx), abs(dy)) != 2 and (dx, dy) != (0, 0):
                                if max(abs(dx), abs(dy)) != 1: continue
                            t = (start[0]+dx, start[1]+dy)
                            if t == start or t in occ: continue
                            if not (0 <= t[0] < W and 0 <= t[1] < H): continue
                            if max(abs(dx), abs(dy)) > 2: continue
                            after = reachable_walk(occ - {start}, t, max(0, ma-2))
                            gain |= set(after)
                    new = {q for q in gain if q not in walk}
                    # jen pole BLÍŽ k endzone než start
                    nf = {q for q in new if (q[0]-start[0])*fwd > 0}
                    if nf: found += 1
                    fwd_sq += len(nf)
                    best[min(len(nf), 10)] += 1
                if found: opp[found] += 1
    tot_opp = sum(opp.values())
    print(f"her: {games} | kol wood-elfa se stojícím wardancerem: {turns_we}")
    print(f"kol s PŘÍLEŽITOSTÍ ke skoku vpřed: {tot_opp} "
          f"({100*tot_opp/max(1,turns_we):.1f} % těch kol)")
    print(f"⭐ A-fwd NA HRU: {tot_opp/max(1,games):.2f}   (metr M9 = 4,09 blitzů/hru)")
    print(f"   polí navíc vpřed celkem: {fwd_sq} = {fwd_sq/max(1,games):.1f}/hru")
    print("\nrozdělení 'kolik polí navíc vpřed' na jednoho wardancera a kolo:")
    for k in sorted(best): print(f"   {'10+' if k==10 else k:>3}: {best[k]:6d}")

if __name__ == '__main__':
    lim = int(sys.argv[1]) if len(sys.argv) > 1 else None
    main(sorted(glob.glob('crosses_20260821_data/*wood-elf*')), lim)
