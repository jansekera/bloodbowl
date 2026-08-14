#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
diag_numbers_advantage_20260811.py

Otazka: promeni se pocetni prevaha (attrition) v neco uzitecneho?
Presmerovane teziste: pomaha oslabeni soupere NASEMU prijimacimu drivu? (plan 1:0)

Body:
 1. Vznika prevaha vcas? (prubezny rozdil on-pitch a stojicich hracu po kolech)
 2. Chova se souper v prevaze jinak? (kontakty, dist nosice, hraci v nasi pulce,
    postup nosice, dodge/kolo)
 3. Chovame se MY jinak? (blitz/blok na nosice, markovani nosice)
 4. Volna pole kolem soupeova nosice na konci naseho kola
 5. (zuzeno) rozdeleni kola a vzdalenosti k jejich endzone pri ziskani mice
 6. NOVE TEZISTE: nase prijimaci drivy podle poctu chybejicich hracu soupere
    na zacatku drivu (0/1/2/3+): TD podil, tempo nosice, odpor v koridoru,
    ztraty mice (a cim), delka drivu.

Definice (zapsane PREDEM, nemenit zpetne):
 - on-pitch = delka pole hracu (hraci mimo jsou z pole odstraneni)
 - stojici = state == 0
 - prevaha = (dwarf on-pitch) - (opp on-pitch) na snimku zacatku kola
 - "prevaha >= 2" vs "vyrovnano" = |diff| <= 1
 - vzdalenost k endzone = |endzone_x - x| (jen osa x; HOME utoci na x=25)
 - vzdalenost hrac-hrac = Chebyshev
 - koridor odporu = stojici oupeni hraci s x ostre mezi nosicem a endzone
   a |y - y_nosice| <= 2 (pas sirky 5)
 - blitz = BLOCK event, kteremu v temze kole predchazel MOVE tehoz hrace
 - tempo nosice = prumerna zmena vzdalenosti k endzone za nase kolo s drzenim
   (TD kolo: cely zbytek vzdalenosti)
 - ziskani mice = dwarf ziska drzeni, kdyz predchozi drzitel v temze drivu byl
   souper (i pres mezistav volneho mice)
 - oslabeni behem drivu = MAX poctu chybejicich hracu soupere pres snimky
   naseho prijimaciho drivu (doplneno, protoze skupina "na zacatku" je
   degenerovana - kickoff soupere doplni)
 - aritmeticky nezhodnotitelne ziskani: dist/1.73 > zbyvajici nase kola v pulce
   (zbyvajici = presny pocet nasich dalsich logu v pulce vcetne aktualniho)
"""
import gzip, json, glob, sys, os, statistics
from collections import Counter, defaultdict

PACE_NEEDED = 3.14   # z drive-failure mereni
PACE_ACTUAL = 1.73

def load_games(datadir):
    games = []
    for f in sorted(glob.glob(os.path.join(datadir, 'g*.json.gz'))):
        try:
            games.append(json.load(gzip.open(f)))
        except Exception as e:
            print('!! nectitelny %s: %s' % (f, e))
    return games

def dwarf_side(g):
    if g['home_race'] == 'dwarf': return 'home'
    if g['away_race'] == 'dwarf': return 'away'
    return None

def side_players(t, side):
    return t['home_players'] if side == 'home' else t['away_players']

def ez_x(side):
    # HOME utoci na x=25, AWAY na x=0
    return 25 if side == 'home' else 0

def own_half(side, x):
    # HOME brani x=0..12, AWAY x=13..25
    return x <= 12 if side == 'home' else x >= 13

def cheb(a, b):
    return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

def find_player(t, pid):
    for p in t['home_players'] + t['away_players']:
        if p['id'] == pid: return p
    return None

def holder_side(t):
    if not t['ball_held'] or t['ball_carrier_id'] < 0: return None
    for p in t['home_players']:
        if p['id'] == t['ball_carrier_id']: return 'home'
    for p in t['away_players']:
        if p['id'] == t['ball_carrier_id']: return 'away'
    return None

def split_drives(tl):
    """Vrati seznam (start_idx, end_idx_exclusive). Novy drive: zacatek, zmena
    pulky, nebo predchozi log mel touchdown=True."""
    bounds = [0]
    for i in range(1, len(tl)):
        if tl[i]['half'] != tl[i-1]['half'] or tl[i-1]['touchdown']:
            bounds.append(i)
    bounds.append(len(tl))
    return [(bounds[k], bounds[k+1]) for k in range(len(bounds)-1)]

def receiver_of_drive(tl, s, e):
    t = tl[s]
    hs = holder_side(t)
    if hs: return hs
    return 'home' if t['ball_x'] <= 12 else 'away'

def fmt_stats(vals, pct=False):
    if not vals: return 'n=0'
    m = statistics.mean(vals)
    med = statistics.median(vals)
    if pct:
        return 'n=%d prumer=%.1f%% ' % (len(vals), 100*m)
    return 'n=%d prumer=%.2f median=%.1f' % (len(vals), m, med)

def analyze(datadir, label):
    games = load_games(datadir)
    print('='*78)
    print('KORPUS %s (%s), her: %d' % (label, datadir, len(games)))
    print('='*78)

    # ---------- akumulatory ----------
    adv_by_turn = defaultdict(list)        # global turn -> [on-pitch diff]
    std_by_turn = defaultdict(list)        # global turn -> [standing diff]
    first_adv2 = []                        # prvni globalni kolo s prevahou >=2 (per game)
    never_adv2 = 0
    adv_turn_share = Counter()             # kolik dwarf kol v prevaze >=2 / celkem

    # bod 2-4: metriky per kolo, klicovane skupinou prevahy ('adv2','even','behind')
    opp_metrics = defaultdict(lambda: defaultdict(list))
    my_metrics  = defaultdict(lambda: defaultdict(list))

    # bod 5: ziskani mice
    gains = []   # dict(seed, half, turn, gturn, dist, remaining, unconvertible, opp)

    # bod 6: prijimaci drivy dwarfa
    drives = []  # dict per receiving drive

    # per-race agregace klicovych cisel
    per_race_drives = defaultdict(list)

    situations = []  # kandidati na konkretni situace
    end_missing = {'dw': [], 'opp': []}  # chybejici na poslednim snimku hry

    for g in games:
        ds = dwarf_side(g)
        if ds is None: continue
        os_ = 'away' if ds == 'home' else 'home'
        opp_race = g['away_race'] if ds == 'home' else g['home_race']
        tl = g['turn_logs']
        dez = ez_x(ds)      # endzone, na kterou utoci dwarf (soupeova)
        oez = ez_x(os_)     # endzone, na kterou utoci souper (nase)
        end_missing['dw'].append(11 - len(side_players(tl[-1], ds)))
        end_missing['opp'].append(11 - len(side_players(tl[-1], os_)))

        # ---------- bod 1: prevaha po kolech ----------
        game_first_adv2 = None
        for t in tl:
            if t['turn'] > 8: continue
            gt = (t['half']-1)*8 + t['turn']
            diff = len(side_players(t, ds)) - len(side_players(t, os_))
            sdiff = (sum(1 for p in side_players(t, ds) if p['state']==0)
                     - sum(1 for p in side_players(t, os_) if p['state']==0))
            if t['active_team'] == ds:
                adv_by_turn[gt].append(diff)
                std_by_turn[gt].append(sdiff)
                adv_turn_share['total'] += 1
                if diff >= 2:
                    adv_turn_share['adv2'] += 1
                    if game_first_adv2 is None: game_first_adv2 = gt
        if game_first_adv2 is not None: first_adv2.append(game_first_adv2)
        else: never_adv2 += 1

        # ---------- body 2-4 (per kolo) ----------
        for i, t in enumerate(tl):
            if t['turn'] > 8: continue
            nxt = tl[i+1] if i+1 < len(tl) else None
            same_drive_next = (nxt is not None and nxt['half'] == t['half']
                               and not t['touchdown'])
            diff = len(side_players(t, ds)) - len(side_players(t, os_))
            grp = 'adv2' if diff >= 2 else ('even' if abs(diff) <= 1 else 'behind')
            hs = holder_side(t)

            if t['active_team'] == os_:
                # metriky chovani soupere v jeho kole
                blocks = sum(1 for e in t['events'] if e['type']=='BLOCK')
                dodges = sum(1 for e in t['events'] if e['type']=='DODGE')
                opp_metrics[grp]['blocks'].append(blocks)
                opp_metrics[grp]['dodges'].append(dodges)
                if same_drive_next:
                    dw_stand = [(p['x'],p['y']) for p in side_players(nxt, ds) if p['state']==0]
                    opp_stand = [p for p in side_players(nxt, os_) if p['state']==0]
                    engaged = sum(1 for p in opp_stand
                                  if any(cheb((p['x'],p['y']), q) == 1 for q in dw_stand))
                    opp_metrics[grp]['engaged'].append(engaged)
                    inhalf = sum(1 for p in side_players(nxt, os_)
                                 if own_half(ds, p['x']))
                    opp_metrics[grp]['in_our_half'].append(inhalf)
                    if hs == os_ and holder_side(nxt) == os_:
                        c0 = find_player(t, t['ball_carrier_id'])
                        c1 = find_player(nxt, nxt['ball_carrier_id'])
                        prog = abs(oez - c0['x']) - abs(oez - c1['x'])
                        opp_metrics[grp]['carrier_progress'].append(prog)
                        if dw_stand:
                            nd = min(cheb((c1['x'],c1['y']), q) for q in dw_stand)
                            opp_metrics[grp]['carrier_nearest_dwarf'].append(nd)

            if t['active_team'] == ds and hs == os_:
                # nase kolo, souper drzi mic
                car = t['ball_carrier_id']
                # blitz/blok detekce
                moved = set()
                block_on_carrier = False; any_block = False
                blitz_on_carrier = False; any_blitz = False
                for e in t['events']:
                    if e['type'] == 'MOVE': moved.add(e['player_id'])
                    if e['type'] == 'BLOCK':
                        any_block = True
                        isblitz = e['player_id'] in moved
                        if isblitz: any_blitz = True
                        if e['target_id'] == car:
                            block_on_carrier = True
                            if isblitz: blitz_on_carrier = True
                my_metrics[grp]['any_block'].append(1 if any_block else 0)
                my_metrics[grp]['any_blitz'].append(1 if any_blitz else 0)
                my_metrics[grp]['block_on_carrier'].append(1 if block_on_carrier else 0)
                my_metrics[grp]['blitz_on_carrier'].append(1 if blitz_on_carrier else 0)
                if same_drive_next:
                    c1 = find_player(nxt, car)
                    if c1 is not None and holder_side(nxt) == os_ and nxt['ball_carrier_id'] == car:
                        dw_stand = [(p['x'],p['y']) for p in side_players(nxt, ds) if p['state']==0]
                        marked = any(cheb((c1['x'],c1['y']), q) == 1 for q in dw_stand)
                        my_metrics[grp]['carrier_marked_end'].append(1 if marked else 0)
                        # bod 4: volna pole kolem nosice (obsazenost, ne zony)
                        occ = set((p['x'],p['y']) for p in nxt['home_players']+nxt['away_players'])
                        free = 0
                        for dx in (-1,0,1):
                            for dy in (-1,0,1):
                                if dx==0 and dy==0: continue
                                nx_, ny_ = c1['x']+dx, c1['y']+dy
                                if 0 <= nx_ <= 25 and 0 <= ny_ <= 14 and (nx_,ny_) not in occ:
                                    free += 1
                        my_metrics[grp]['carrier_free_squares'].append(free)
                        if grp == 'adv2':
                            situations.append(dict(kind='escape', seed=g['seed'],
                                opp=opp_race, idx=i, free=free, diff=diff))
                # sebrani mice behem naseho kola
                if same_drive_next:
                    my_metrics[grp]['took_ball'].append(
                        1 if holder_side(nxt) != os_ else 0)

        # ---------- bod 5: ziskani mice ----------
        for (s, e) in split_drives(tl):
            last_holder = None
            for i in range(s, e):
                t = tl[i]
                hs = holder_side(t)
                if hs == ds and last_holder == os_:
                    car = find_player(t, t['ball_carrier_id'])
                    dist = abs(dez - car['x'])
                    remaining = sum(1 for j in range(i, len(tl))
                                    if tl[j]['half'] == t['half']
                                    and tl[j]['active_team'] == ds
                                    and tl[j]['turn'] <= 8)
                    unconv = dist / PACE_ACTUAL > remaining
                    gains.append(dict(seed=g['seed'], half=t['half'], turn=t['turn'],
                        gturn=(t['half']-1)*8+t['turn'], dist=dist,
                        remaining=remaining, unconv=unconv, opp=opp_race, idx=i))
                if hs is not None: last_holder = hs

        # ---------- bod 6: prijimaci drivy dwarfa ----------
        for (s, e) in split_drives(tl):
            if receiver_of_drive(tl, s, e) != ds: continue
            t0 = tl[s]
            if t0['turn'] > 8: continue   # T9 artefakt po TD v 8. kole
            opp_missing = 11 - len(side_players(t0, os_))
            dw_turns = [i for i in range(s, e) if tl[i]['active_team'] == ds
                        and tl[i]['turn'] <= 8]
            if not dw_turns: continue
            # TD dwarfa v tomto drivu?
            sc0 = t0[ds.replace('home','home_score').replace('away','away_score')] \
                  if False else (t0['home_score'] if ds=='home' else t0['away_score'])
            last = tl[e-1]
            sc1 = (last['home_score'] if ds=='home' else last['away_score'])
            td = (last['touchdown'] and last['active_team'] == ds) or sc1 > sc0
            # tempo nosice + odpor
            paces = []; resist = []
            held_any = False; lost = False; loss_cause = None; loss_idx = None
            for i in dw_turns:
                t = tl[i]
                if holder_side(t) != ds: continue
                held_any = True
                car = find_player(t, t['ball_carrier_id'])
                d0 = abs(dez - car['x'])
                opp_stand = [p for p in side_players(t, os_) if p['state']==0]
                lo, hi = min(car['x'], dez), max(car['x'], dez)
                res = sum(1 for p in opp_stand
                          if lo < p['x'] < hi and abs(p['y']-car['y']) <= 2)
                resist.append(res)
                if t['touchdown'] and t['active_team'] == ds:
                    paces.append(d0)
                elif i+1 < len(tl) and tl[i+1]['half'] == t['half']:
                    nxt = tl[i+1]
                    if holder_side(nxt) == ds:
                        c1 = find_player(nxt, nxt['ball_carrier_id'])
                        paces.append(d0 - abs(dez - c1['x']))
            # ztrata mice v drivu (kdykoli, i v kole soupere)
            for i in range(s, e-1):
                if tl[i]['turn'] > 8: continue
                if holder_side(tl[i]) == ds and holder_side(tl[i+1]) != ds \
                   and not tl[i]['touchdown']:
                    lost = True; loss_idx = i
                    ev = tl[i]['events']
                    carid = tl[i]['ball_carrier_id']
                    cause = 'jine/neurceno'
                    for e_ in ev:
                        ty = e_['type']
                        if ty == 'BLOCK' and e_['target_id'] == carid:
                            cause = 'blok/blitz soupere na nosice'
                        elif ty == 'DODGE' and e_['player_id'] == carid and not e_['success']:
                            cause = 'nezvladnuty dodge nosice'
                        elif ty == 'GFI' and e_['player_id'] == carid and not e_['success']:
                            cause = 'nezvladnute GFI nosice'
                        elif ty in ('PASS','CATCH') and not e_['success']:
                            cause = 'nezvladnuta prihravka/chytani'
                        elif ty == 'PICKUP' and not e_['success']:
                            cause = 'nezvladnuty pickup'
                    if tl[i]['active_team'] == os_ and cause == 'jine/neurceno':
                        cause = 'kolo soupere (jine)'
                    loss_cause = cause
                    break
            # nezvednuty mic: drive, kde jsme nikdy nedrzeli
            opp_missing_max = max(11 - len(side_players(tl[i], os_))
                                  for i in range(s, e))
            rec = dict(seed=g['seed'], opp=opp_race, opp_missing=opp_missing,
                       opp_missing_max=opp_missing_max, start_turn=t0['turn'],
                       td=td, pace=(statistics.mean(paces) if paces else None),
                       resist=(statistics.mean(resist) if resist else None),
                       held=held_any, lost=lost, loss_cause=loss_cause,
                       loss_idx=loss_idx, nturns=len(dw_turns), start=s, end=e)
            drives.append(rec)
            per_race_drives[opp_race].append(rec)

    # =================== VYSTUP ===================
    print('\n--- 0. KONTEXT: chybejici hraci na POSLEDNIM snimku hry (srovnani s tabulkou G) ---')
    for side, lab in (('dw','my'), ('opp','souper')):
        v = end_missing[side]
        c = Counter(min(x,3) for x in v)
        print('  %-7s prumer=%.2f  0:%d%% 1:%d%% 2:%d%% 3+:%d%%' % (lab,
              statistics.mean(v), *[round(100*c.get(k,0)/len(v)) for k in (0,1,2,3)]))

    print('\n--- 1. VZNIKA PREVAHA VCAS? (rozdil dwarf - souper, snimek zacatku naseho kola) ---')
    print('kolo | on-pitch prumer | stojici prumer | podil kol s prevahou >=2')
    for gt in range(1, 17):
        a = adv_by_turn.get(gt, []); sdf = std_by_turn.get(gt, [])
        if not a: continue
        p2 = sum(1 for x in a if x >= 2)/len(a)
        print('%4d | %8.2f (n=%d) | %8.2f | %5.1f%%' %
              (gt, statistics.mean(a), len(a), statistics.mean(sdf), 100*p2))
    if first_adv2:
        print('Prvni kolo s prevahou >=2: median %s, prumer %.1f (her s prevahou: %d, nikdy: %d)'
              % (statistics.median(first_adv2), statistics.mean(first_adv2),
                 len(first_adv2), never_adv2))
    tot = adv_turn_share['total']
    print('Podil vsech nasich kol odehranych v prevaze >=2: %.1f%% (%d/%d)'
          % (100*adv_turn_share['adv2']/max(tot,1), adv_turn_share['adv2'], tot))

    print('\n--- 2. CHOVANI SOUPERE (jeho kola) podle prevahy ---')
    for key, lab in [('blocks','bloku soupere za kolo'), ('dodges','dodgu soupere za kolo'),
                     ('engaged','jeho hracu v kontaktu (konec jeho kola)'),
                     ('in_our_half','jeho hracu v nasi pulce'),
                     ('carrier_progress','postup nosice k nasi endzone (poli/kolo)'),
                     ('carrier_nearest_dwarf','vzdalenost nosice od nejbl. stojiciho dwarfa')]:
        row = []
        for grp in ('even','adv2'):
            row.append('%s: %s' % (grp, fmt_stats(opp_metrics[grp][key])))
        print('  %-46s %s' % (lab, ' | '.join(row)))

    print('\n--- 3. NASE CHOVANI (nase kola, souper drzi mic) podle prevahy ---')
    for key, lab in [('any_block','kol s aspon jednim blokem'), ('any_blitz','kol s blitzem'),
                     ('block_on_carrier','kol s blokem NA NOSICE'),
                     ('blitz_on_carrier','kol s blitzem NA NOSICE'),
                     ('carrier_marked_end','nosic markovan na konci naseho kola'),
                     ('took_ball','mic souperi odebran v nasem kole')]:
        row = []
        for grp in ('even','adv2'):
            v = my_metrics[grp][key]
            row.append('%s: n=%d %.1f%%' % (grp, len(v), 100*statistics.mean(v)) if v else '%s: n=0' % grp)
        print('  %-46s %s' % (lab, ' | '.join(row)))

    print('\n--- 4. VOLNA POLE KOLEM JEJICH NOSICE (konec naseho kola) ---')
    for grp in ('even','adv2'):
        v = my_metrics[grp]['carrier_free_squares']
        if v:
            c = Counter(v)
            dist = ' '.join('%d:%d%%' % (k, round(100*c[k]/len(v))) for k in sorted(c))
            print('  %s: %s  [%s]' % (grp, fmt_stats(v), dist))

    print('\n--- 5. ZISKANI MICE (rozdeleni kola a vzdalenosti; zuzeno dle pokynu) ---')
    print('  ziskani celkem: %d (%.2f na hru)' % (len(gains), len(gains)/max(len(games),1)))
    if gains:
        print('  kolo v pulce: median %.1f | vzdalenost k jejich endzone: median %.1f poli'
              % (statistics.median([x['turn'] for x in gains]),
                 statistics.median([x['dist'] for x in gains])))
        print('  zbyvajici nase kola v pulce: median %.1f'
              % statistics.median([x['remaining'] for x in gains]))
        unc = sum(1 for x in gains if x['unconv'])
        print('  aritmeticky nezhodnotitelnych (dist/1.73 > zbyvajici kola): %.0f%% (%d/%d)'
              % (100*unc/len(gains), unc, len(gains)))
        c = Counter(x['turn'] for x in gains)
        print('  histogram kola:', ' '.join('T%d:%d' % (k, c[k]) for k in sorted(c)))
        cd = Counter(min(x['dist']//5*5, 20) for x in gains)
        print('  histogram vzdalenosti:', ' '.join('%d-%d:%d' % (k, k+4, cd[k]) for k in sorted(cd)))

    print('\n--- 6. NASE PRIJIMACI DRIVY podle chybejicich hracu soupere na startu ---')
    def grp_of(m): return '3+' if m >= 3 else str(m)
    header = '%-4s %5s %6s %8s %8s %8s %8s %7s %8s' % (
        'opp-','drivu','TD %','tempo','odpor','ztrata%','nedrzel%','kol','ztrata blokem%')
    print(header)
    groups = defaultdict(list)
    for r in drives: groups[grp_of(r['opp_missing'])].append(r)
    for k in ('0','1','2','3+'):
        rs = groups.get(k, [])
        if not rs:
            print('%-4s %5d' % (k, 0)); continue
        tdp = 100*sum(1 for r in rs if r['td'])/len(rs)
        paces = [r['pace'] for r in rs if r['pace'] is not None]
        res = [r['resist'] for r in rs if r['resist'] is not None]
        lostp = 100*sum(1 for r in rs if r['lost'])/len(rs)
        nheld = 100*sum(1 for r in rs if not r['held'])/len(rs)
        nt = statistics.mean([r['nturns'] for r in rs])
        lb = [r for r in rs if r['lost']]
        lbp = 100*sum(1 for r in lb if r['loss_cause'] and 'blok' in r['loss_cause'])/max(len(lb),1)
        print('%-4s %5d %5.0f%% %8.2f %8.2f %7.0f%% %7.0f%% %7.1f %8.0f%%' % (
            k, len(rs), tdp,
            statistics.mean(paces) if paces else float('nan'),
            statistics.mean(res) if res else float('nan'),
            lostp, nheld, nt, lbp))
    print('\n--- 6b. TOTEZ podle MAXIMA chybejicich hracu soupere BEHEM drivu ---')
    print('  (skupina "na zacatku" je degenerovana: kickoff soupere doplni; tady je')
    print('   otazka: kdyz uz je souper behem naseho drivu dole, konvertujeme lip?)')
    print(header)
    groups_m = defaultdict(list)
    for r in drives: groups_m[grp_of(r['opp_missing_max'])].append(r)
    for k in ('0','1','2','3+'):
        rs = groups_m.get(k, [])
        if not rs:
            print('%-4s %5d' % (k, 0)); continue
        tdp = 100*sum(1 for r in rs if r['td'])/len(rs)
        paces = [r['pace'] for r in rs if r['pace'] is not None]
        res = [r['resist'] for r in rs if r['resist'] is not None]
        lostp = 100*sum(1 for r in rs if r['lost'])/len(rs)
        nheld = 100*sum(1 for r in rs if not r['held'])/len(rs)
        nt = statistics.mean([r['nturns'] for r in rs])
        lb = [r for r in rs if r['lost']]
        lbp = 100*sum(1 for r in lb if r['loss_cause'] and 'blok' in r['loss_cause'])/max(len(lb),1)
        print('%-4s %5d %5.0f%% %8.2f %8.2f %7.0f%% %7.0f%% %7.1f %8.0f%%' % (
            k, len(rs), tdp,
            statistics.mean(paces) if paces else float('nan'),
            statistics.mean(res) if res else float('nan'),
            lostp, nheld, nt, lbp))

    print('\n--- 6c. KONTROLA KONFOUNDU: jen drivy zacinajici v T1-T2 pulky (plna delka) ---')
    full = [r for r in drives if r['start_turn'] <= 2]
    print(header)
    groups_f = defaultdict(list)
    for r in full: groups_f[grp_of(r['opp_missing_max'])].append(r)
    for k in ('0','1','2','3+'):
        rs = groups_f.get(k, [])
        if not rs:
            print('%-4s %5d' % (k, 0)); continue
        tdp = 100*sum(1 for r in rs if r['td'])/len(rs)
        paces = [r['pace'] for r in rs if r['pace'] is not None]
        res = [r['resist'] for r in rs if r['resist'] is not None]
        lostp = 100*sum(1 for r in rs if r['lost'])/len(rs)
        nheld = 100*sum(1 for r in rs if not r['held'])/len(rs)
        nt = statistics.mean([r['nturns'] for r in rs])
        lb = [r for r in rs if r['lost']]
        lbp = 100*sum(1 for r in lb if r['loss_cause'] and 'blok' in r['loss_cause'])/max(len(lb),1)
        print('%-4s %5d %5.0f%% %8.2f %8.2f %7.0f%% %7.0f%% %7.1f %8.0f%%' % (
            k, len(rs), tdp,
            statistics.mean(paces) if paces else float('nan'),
            statistics.mean(res) if res else float('nan'),
            lostp, nheld, nt, lbp))

    print('\n  Priciny ztraty mice v prijimacich drivech (vsechny skupiny):')
    cc = Counter(r['loss_cause'] for r in drives if r['lost'])
    for cause, n in cc.most_common():
        print('    %-42s %d' % (cause, n))

    print('\n  Po rasach (prijimaci drivy: n, TD%%, tempo, odpor):')
    for race, rs in sorted(per_race_drives.items()):
        tdp = 100*sum(1 for r in rs if r['td'])/len(rs)
        paces = [r['pace'] for r in rs if r['pace'] is not None]
        res = [r['resist'] for r in rs if r['resist'] is not None]
        rs3 = [r for r in rs if r['opp_missing'] >= 3]
        tdp3 = 100*sum(1 for r in rs3 if r['td'])/max(len(rs3),1)
        print('    %-10s n=%3d TD=%3.0f%% tempo=%.2f odpor=%.2f | oslabeny 3+: n=%d TD=%.0f%%'
              % (race, len(rs), tdp,
                 statistics.mean(paces) if paces else float('nan'),
                 statistics.mean(res) if res else float('nan'),
                 len(rs3), tdp3))

    return dict(games=games, drives=drives, gains=gains,
                my_metrics=my_metrics, opp_metrics=opp_metrics,
                situations=situations, adv_by_turn=adv_by_turn)

if __name__ == '__main__':
    dirs = sys.argv[1:] or ['diag_replay_mine_20260811b_data']
    for d in dirs:
        analyze(d, os.path.basename(d.rstrip('/')))
