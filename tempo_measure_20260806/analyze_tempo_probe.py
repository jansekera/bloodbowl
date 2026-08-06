#!/usr/bin/env python3
# Vyhodnocení sondy tempo doktríny (Fable #3, 06.08.).
# Vstup: tempo_measure_20260806/probe_m{M}_g{G}.log (formát diag_f1_adoption_probe
# po 41fd8c5). Výstup: tabulky pro report (TEMPO rozpad, drive outcomy,
# baseline vs grind-shadow konverze, DICEY pto histogram u grind běhů).
import re, sys, glob, os
from collections import Counter, defaultdict

D = os.path.dirname(os.path.abspath(__file__))

def parse(path):
    r = {'tempo': [], 'drive': [], 'verdicts': Counter(), 'dicey_fail': [],
         'turns': 0, 'advance': 0}
    for line in open(path):
        m = re.match(r'\[TEMPO\] (\S+)-g(\d+) H(\d) T(\d+) (HOME|AWAY) dist=(\d+) '
                     r'turnsLeft=(\d+) usable=(-?\d+) req=([\d.]+) raw=(\d+) '
                     r'ach=(-?[\d.]+) resist=(\d+) br=(\w) carMA=(\d+) carTZ=(\d+) '
                     r'driveStart=(-?\d+) built=(\d+)', line)
        if m:
            g = m.groups()
            r['tempo'].append(dict(tag=g[0], gi=int(g[1]), half=int(g[2]),
                turn=int(g[3]), side=g[4], dist=int(g[5]), turnsLeft=int(g[6]),
                usable=int(g[7]), req=float(g[8]), raw=int(g[9]),
                ach=float(g[10]), resist=int(g[11]), br=g[12], carMA=int(g[13]),
                carTZ=int(g[14]), driveStart=int(g[15]), built=int(g[16])))
            continue
        m = re.match(r'\[DRIVE\] (\S+)-g(\d+) H(\d) start=H:(-?\d+)/A:(-?\d+) '
                     r'end=H:(-?\d+)/A:(-?\d+) outcome=(\S+) tempo=(\d+)/(\d+) '
                     r'ready=(\d+)/(\d+) dicey=(\d+)/(\d+) possLost=(\d)/(\d)', line)
        if m:
            g = m.groups()
            r['drive'].append(dict(gi=int(g[1]), half=int(g[2]),
                start=(int(g[3]), int(g[4])), end=(int(g[5]), int(g[6])),
                outcome=g[7], tempo=(int(g[8]), int(g[9])),
                ready=(int(g[10]), int(g[11])), dicey=(int(g[12]), int(g[13])),
                possLost=(int(g[14]), int(g[15]))))
            continue
        m = re.match(r'\s+verdict (\S+)\s+(\d+)', line)
        if m:
            r['verdicts'][m.group(1)] = int(m.group(2)); continue
        m = re.search(r'FAIL leg#\d+ .*pto=([\d.]+) ceil=([\d.]+).*execFail=(\d)', line)
        if m:
            r['dicey_fail'].append((float(m.group(1)), float(m.group(2)),
                                    int(m.group(3)))); continue
        m = re.match(r'=== SUMMARY \[\S+\] \((\d+) team-turns\)', line)
        if m: r['turns'] = int(m.group(1))
        m = re.match(r'\s+goal ADVANCE\s+(\d+)', line)
        if m: r['advance'] = int(m.group(1))
    return r

def tempo_breakdown(t):
    n = len(t)
    if n == 0: return "  (žádná TEMPO veta)"
    br = Counter(x['br'] for x in t)
    raw2 = sum(1 for x in t if x['raw'] >= 2)
    raw1 = sum(1 for x in t if x['raw'] == 1)
    raw0 = n - raw2 - raw1
    full = [x for x in t if x['driveStart'] <= 1]
    mid = [x for x in t if x['driveStart'] > 1]
    out = [f"  N={n} větve: {dict(br)} | raw>=2: {raw2} ({raw2/n:.0%}) raw=1: {raw1} raw=0: {raw0}"]
    out.append(f"  drive od zač. poločasu: {len(full)} ({len(full)/n:.0%}), mid-half: {len(mid)}")
    for lbl, grp in (("fullHalf", full), ("midHalf", mid)):
        if not grp: continue
        p = [x for x in grp if x['br'] == 'p']
        out.append(f"    {lbl}: br={dict(Counter(x['br'] for x in grp))}; "
                   f"p-větev meanReq={sum(x['req'] for x in p)/max(1,len(p)):.2f} "
                   f"meanAch={sum(x['ach'] for x in p)/max(1,len(p)):.2f} "
                   f"meanDist={sum(x['dist'] for x in p)/max(1,len(p)):.1f} "
                   f"meanTurnsLeft={sum(x['turnsLeft'] for x in p)/max(1,len(p)):.1f}")
    # pozdní kola (T>=7): veto "u" + nízké turnsLeft
    late = sum(1 for x in t if x['turnsLeft'] <= 2)
    out.append(f"  vet s turnsLeft<=2 (konec poločasu, drž míč): {late} ({late/n:.0%})")
    return "\n".join(out)

def drive_summary(dr):
    agg = Counter(); tot = 0
    for d in dr:
        for si in (0, 1):
            if d['tempo'][si] < 1: continue
            tot += 1
            if d['outcome'] == 'NO_SCORE': agg['noScore'] += 1
            elif (d['outcome'] == 'TD_HOME') == (si == 0): agg['tdFor'] += 1
            else: agg['tdAgainst'] += 1
            if d['possLost'][si]: agg['possLost'] += 1
    return (f"  drive-strany s >=1 TEMPO vetem: {tot} | TD pro: {agg['tdFor']} "
            f"TD proti: {agg['tdAgainst']} bez bodu: {agg['noScore']} "
            f"ztráta držení po vetu: {agg['possLost']}")

for mi, tag in ((0, 'dw-sk'), (1, 'dw-we'), (3, 'orc-sk')):
    base = os.path.join(D, f'probe_m{mi}_g0.log')
    if not os.path.exists(base): continue
    b = parse(base)
    print(f"\n===== {tag} baseline (grind=0), {b['turns']} team-turnů, ADVANCE {b['advance']} =====")
    print("  verdikty:", dict(b['verdicts']))
    print(tempo_breakdown(b['tempo']))
    print(drive_summary(b['drive']))
    shadow = os.path.join(D, f'probe_m{mi}_g1.log')
    if os.path.exists(shadow):
        s = parse(shadow)
        print(f"  --- grind SHADOW (tytéž hry/seedy): verdikty {dict(s['verdicts'])}")
        conv_from = b['verdicts']['TEMPO_INSUFFICIENT']
        print(f"      TEMPO {conv_from} -> {s['verdicts']['TEMPO_INSUFFICIENT']} "
              f"(otevřeno {conv_from - s['verdicts']['TEMPO_INSUFFICIENT']}); "
              f"PLAN_READY {b['verdicts']['PLAN_READY']} -> {s['verdicts']['PLAN_READY']}, "
              f"DICEY {b['verdicts']['DICEY']} -> {s['verdicts']['DICEY']}")
        pf = [x for x in s['dicey_fail'] if x[2] == 0]
        ef = sum(1 for x in s['dicey_fail'] if x[2] == 1)
        for cap in (0.10, 0.25, 0.40):
            k = sum(1 for p, c, _ in pf if p <= cap)
            print(f"      DICEY probe-fail pto<= {cap}: {k}/{len(pf)}", end="")
        print(f" | exec-fail {ef}/{len(s['dicey_fail'])}")
