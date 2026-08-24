#!/usr/bin/env python3
"""Křížový korpus 21.-22.08.: rasa proti rase. Čte jen to, co je v záznamu."""
import gzip, json, glob, os, sys, collections
from multiprocessing import Pool

ROOT = 'crosses_20260821_data'
# stav hráče: 0 = stojí; hledáme kódy pro PRONE/STUNNED empiricky přes četnost
def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    hs, as_ = d['home_score'], d['away_score']
    tl = d['turn_logs']
    ev = collections.Counter()
    states = collections.Counter()
    for t in tl:
        for e in t['events']:
            ev[e.get('type')] += 1
        for p in t['home_players'] + t['away_players']:
            states[p['state']] += 1
    return (hr, ar, hs, as_, len(tl), ev, states)

if __name__ == '__main__':
    files = sorted(glob.glob(os.path.join(ROOT, '*', 'g*.json.gz')))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]

    # --- (1) tabulka dvojic ---
    pair = collections.defaultdict(lambda: dict(n=0, hs=0, as_=0, hw=0, aw=0, dr=0, nil=0, turns=0))
    race = collections.defaultdict(lambda: dict(n=0, td=0, conc=0, w=0, d=0, l=0, blank=0))
    ev_tot = collections.Counter(); st_tot = collections.Counter(); turns_tot = 0
    for hr, ar, hs, as_, nt, ev, states in rows:
        k = (hr, ar); p = pair[k]
        p['n'] += 1; p['hs'] += hs; p['as_'] += as_; p['turns'] += nt
        if hs > as_: p['hw'] += 1
        elif as_ > hs: p['aw'] += 1
        else: p['dr'] += 1
        if hs == 0 and as_ == 0: p['nil'] += 1
        for r, mine, theirs in ((hr, hs, as_), (ar, as_, hs)):
            x = race[r]; x['n'] += 1; x['td'] += mine; x['conc'] += theirs
            if mine > theirs: x['w'] += 1
            elif mine < theirs: x['l'] += 1
            else: x['d'] += 1
            if mine == 0: x['blank'] += 1
        ev_tot += ev; st_tot += states; turns_tot += nt

    ng = len(rows)
    print(f"HER CELKEM: {ng}  (15 dvojic x 1200)   kol celkem: {turns_tot}")
    print()
    print("=== (1) DVOJICE ===")
    print(f"{'dvojice':<22}{'n':>5}{'TD dom':>8}{'TD host':>9}{'V-R-P dom':>12}{'0:0':>7}{'kol/hru':>9}")
    for k in sorted(pair, key=lambda k: -(pair[k]['hs']+pair[k]['as_'])/pair[k]['n']):
        p = pair[k]; n = p['n']
        print(f"{k[0]+' vs '+k[1]:<22}{n:>5}{p['hs']/n:>8.3f}{p['as_']/n:>9.3f}"
              f"{str(p['hw'])+'-'+str(p['dr'])+'-'+str(p['aw']):>12}{100*p['nil']/n:>6.1f}%{p['turns']/n:>9.1f}")
    print()
    print("=== (2) RASY (obě strany dohromady) ===")
    print(f"{'rasa':<12}{'zápasů':>8}{'TD/hru':>9}{'obdrž.':>9}{'rozdíl':>9}{'výhra%':>9}{'remíza%':>9}{'nula%':>8}")
    for r in sorted(race, key=lambda r: -race[r]['td']/race[r]['n']):
        x = race[r]; n = x['n']
        print(f"{r:<12}{n:>8}{x['td']/n:>9.3f}{x['conc']/n:>9.3f}{(x['td']-x['conc'])/n:>+9.3f}"
              f"{100*x['w']/n:>8.1f}%{100*x['d']/n:>8.1f}%{100*x['blank']/n:>7.1f}%")
    print()
    print("=== (3) UDÁLOSTI NA HRU (celý korpus) ===")
    for t, c in ev_tot.most_common():
        print(f"  {str(t):<16}{c/ng:>9.2f}")
    print()
    print("=== (4) STAVY HRÁČŮ (podíl razítek ve všech kolech) ===")
    tot = sum(st_tot.values())
    for s, c in sorted(st_tot.items()):
        print(f"  state {s}: {100*c/tot:>6.2f}%   ({c})")
