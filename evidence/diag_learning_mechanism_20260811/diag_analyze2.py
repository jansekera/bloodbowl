"""Doplnkove kvantifikace: END_TURN ranking, REPOS uniformita, PICKUP AG-slepota,
risk-mix zvoleneho makra A vs B."""
import os, json, pickle
import numpy as np
from diag_analyze import load_policy, logits, softmax, mtype, decider_race

BB = '/home/jan/claude/bloodbowl'
SCR = os.path.dirname(os.path.abspath(__file__))

data = pickle.load(open(os.path.join(SCR, 'decisions.pkl'), 'rb'))
W1, b1, W2, b2 = load_policy(os.path.join(BB, 'weights_best_policy.json'))

groups = {}
for rec in data:
    for d in rec['decs']:
        groups.setdefault((rec['tag'], decider_race(rec, d)), []).append((rec, d))

# (a) END_TURN: kdyz search voli END_TURN (>=50 % visitu), kam ho radi policy?
print('=== a) END_TURN ranking policy, kdyz ho search jednoznacne voli ===')
for key in [('A', 'dwarf'), ('C', 'wood-elf')]:
    ranks, priors_on_et = [], []
    for rec, d in groups[key]:
        vf = np.array([v for _, v in d['av']])
        types = [mtype(a) for a, _ in d['av']]
        iv = int(np.argmax(vf))
        if types[iv] != 'END_TURN' or vf[iv] < 0.5 or len(types) < 3:
            continue
        pp = softmax(logits(W1, b1, W2, b2, d['sf'].astype(np.float64),
                            np.stack([a for a, _ in d['av']])))
        rank = int(np.sum(pp > pp[iv]))  # 0 = policy top1
        ranks.append((rank, len(types)))
        priors_on_et.append(float(pp[iv]))
    if ranks:
        top1 = sum(1 for r, _ in ranks if r == 0)
        print(f"  {key}: n={len(ranks)} jednoznacnych END_TURN; policy je ma jako top1: "
              f"{top1}x ({100*top1/len(ranks):.0f} %); median rank {np.median([r for r,_ in ranks]):.0f} "
              f"z mediane {np.median([n for _,n in ranks]):.0f} kandidatu; "
              f"prumerny policy prior na END_TURN {100*np.mean(priors_on_et):.1f} %")

# (b) REPOS: kolik odlisnych prior hodnot mezi REPOS kandidaty jedne pozice?
print('\n=== b) REPOS: rozlisitelnost kandidatu pro policy ===')
tot, distinct_ratio = 0, []
for rec, d in groups[('A', 'dwarf')]:
    types = [mtype(a) for a, _ in d['av']]
    idx = [i for i, t in enumerate(types) if t == 'REPOS']
    if len(idx) < 3:
        continue
    pp = softmax(logits(W1, b1, W2, b2, d['sf'].astype(np.float64),
                        np.stack([a for a, _ in d['av']])))
    vals = np.round(pp[idx], 6)
    distinct_ratio.append(len(set(vals)) / len(idx))
    tot += 1
print(f"  n={tot} rozhodnuti s >=3 REPOS kandidaty; prumerny podil ODLISNYCH prior "
      f"hodnot mezi REPOS kandidaty: {100*np.mean(distinct_ratio):.0f} % "
      f"(100 % = kazdy kandidat jiny prior, nizke = policy je nerozezna)")

# jak moc se lisi visit fractions mezi REPOS kandidaty (search je rozeznava)
spread_v, spread_p = [], []
for rec, d in groups[('A', 'dwarf')]:
    types = [mtype(a) for a, _ in d['av']]
    idx = [i for i, t in enumerate(types) if t == 'REPOS']
    if len(idx) < 3:
        continue
    vf = np.array([v for _, v in d['av']])[idx]
    pp = softmax(logits(W1, b1, W2, b2, d['sf'].astype(np.float64),
                        np.stack([a for a, _ in d['av']])))[idx]
    if vf.sum() > 0:
        spread_v.append(vf.max() - vf.min())
        spread_p.append(pp.max() - pp.min())
print(f"  spread (max-min) mezi REPOS kandidaty: search visity {np.mean(spread_v):.3f} "
      f"vs policy priory {np.mean(spread_p):.3f}")

# (c) PICKUP naseptavani podle AG tymu: prior na PICKUP v loose-ball stavech
print('\n=== c) PICKUP prior v loose-ball stavech podle rasy ===')
for key in [('A', 'dwarf'), ('C', 'wood-elf'), ('A', 'skaven')]:
    pri, vis = [], []
    for rec, d in groups[key]:
        if d['sf'][14] < 0.5:  # ball not loose
            continue
        types = [mtype(a) for a, _ in d['av']]
        if 'PICKUP' not in types:
            continue
        vf = np.array([v for _, v in d['av']])
        pp = softmax(logits(W1, b1, W2, b2, d['sf'].astype(np.float64),
                            np.stack([a for a, _ in d['av']])))
        pmass = sum(p for t, p in zip(types, pp) if t == 'PICKUP')
        vmass = sum(v for t, v in zip(types, vf) if t == 'PICKUP')
        pri.append(pmass); vis.append(vmass)
    print(f"  {key}: n={len(pri)}; policy prior masa na PICKUP {100*np.mean(pri):.1f} % "
          f"vs search visit masa {100*np.mean(vis):.1f} % (delta {100*(np.mean(pri)-np.mean(vis)):+.1f} pp)")

# (d) risk-mix zvoleneho makra (feature [13] risk_level) dwarf A vs B
print('\n=== d) prumerna riskovost ZVOLENEHO makra (feature[13]) ===')
for key in [('A', 'dwarf'), ('B', 'dwarf'), ('A', 'skaven'), ('B', 'skaven')]:
    risks = []
    for rec, d in groups[key]:
        vf = np.array([v for _, v in d['av']])
        af = d['av'][int(np.argmax(vf))][0]
        risks.append(float(af[13]))
    print(f"  {key}: n={len(risks)}, mean chosen risk {np.mean(risks):.4f}")

# (e) END_TURN podil na celo-tahovem konci: kolik em konci dwarf tah drive (proxy)
print('\n=== e) pocet rozhodnuti na hru (proxy delky tahu) ===')
for key in [('A', 'dwarf'), ('B', 'dwarf')]:
    per_game = {}
    for rec, d in groups[key]:
        per_game[rec['seed']] = per_game.get(rec['seed'], 0) + 1
    print(f"  {key}: prumer {np.mean(list(per_game.values())):.1f} rozhodnuti/hru")
