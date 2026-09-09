import gzip, json, glob, sys
from collections import defaultdict

DATA = '/home/jenda/claude/blood-bowl/corpus_baseline_20260819_data'
files = sorted(glob.glob(DATA + '/g*.json.gz'))

def analyze_game(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return []
    home_race = d.get('home_race')
    away_race = d.get('away_race')
    we_home = None
    if home_race == 'dwarf':
        we_home = True
    elif away_race == 'dwarf':
        we_home = False
    else:
        return []
    results = []
    for idx, t in enumerate(d['turn_logs']):
        ours = t['home_players'] if we_home else t['away_players']
        # standing our players
        standing = [p for p in ours if p['state'] == 0]
        if len(standing) < 6:
            continue
        carrier = next((p for p in standing if p.get('has_ball')), None)
        if not carrier:
            continue
        cx, cy = carrier['x'], carrier['y']
        # diagonal neighbors
        diag = [(cx-1,cy-1),(cx+1,cy-1),(cx-1,cy+1),(cx+1,cy+1)]
        orth = [(cx-1,cy),(cx+1,cy),(cx,cy-1),(cx,cy+1)]
        occ_positions = {(p['x'], p['y']): p for p in standing if p['id'] != carrier['id']}
        diag_occ = sum(1 for sq in diag if sq in occ_positions)
        orth_occ = sum(1 for sq in orth if sq in occ_positions)
        if diag_occ < 3:
            continue
        # remaining players not in cage (not carrier, not in diag/orth adjacency used)
        cage_squares = set(diag) | set(orth) | {(cx,cy)}
        rest = [p for p in standing if (p['x'], p['y']) not in cage_squares and p['id'] != carrier['id']]
        # screen: group by y, find y with >=3 players spread in x
        byy = defaultdict(list)
        for p in rest:
            byy[p['y']].append(p['x'])
        best_y = None
        best_count = 0
        for y, xs in byy.items():
            xs_sorted = sorted(xs)
            if len(xs_sorted) >= 3:
                spread = xs_sorted[-1] - xs_sorted[0]
                if spread >= 4 and len(xs_sorted) > best_count:
                    best_count = len(xs_sorted)
                    best_y = y
        if best_y is None:
            continue
        results.append({
            'path': path, 'idx': idx, 'we_home': we_home,
            'diag_occ': diag_occ, 'orth_occ': orth_occ,
            'screen_y': best_y, 'screen_n': best_count,
            'turn': t['turn'], 'half': t['half'],
            'active_team': t['active_team'],
        })
    return results

all_results = []
for i, f in enumerate(files):
    if i % 500 == 0:
        print(f'... {i}/{len(files)}', file=sys.stderr)
    all_results.extend(analyze_game(f))

all_results.sort(key=lambda r: (-r['diag_occ'], -r['screen_n']))
print(f'Total candidates: {len(all_results)}')
for r in all_results[:15]:
    print(r)
