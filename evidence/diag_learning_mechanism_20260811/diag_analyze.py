"""Analyza rozhodnuti: co policy naseptava trpaslikovi vs co dela search.

Vstup: decisions.pkl (diag_collect.py). Vahy: weights_best_policy.json (promotnuta
policy cd72ed6b). Softmax pri temp=1.0 = presne to, co MCTS blenduje
(macro_mcts.cpp:378 "temp=1.0").
"""
import os, sys, json, pickle
import numpy as np

BB = '/home/jan/claude/bloodbowl'
SCR = os.path.dirname(os.path.abspath(__file__))
NUM_STATE = 73
NUM_ACT = 23

TYPES = ['SCORE','ADVANCE','CAGE','BLITZ','BLOCK','PICKUP','PASS','FOUL','REPOS','END_TURN']

def load_policy(path):
    d = json.load(open(path))
    H = d['policy_hidden_size']
    W1 = np.array(d['policy_W1'], dtype=np.float64).reshape(NUM_STATE + NUM_ACT, H)
    b1 = np.array(d['policy_b1'], dtype=np.float64)
    W2 = np.array(d['policy_W2'], dtype=np.float64)
    b2 = float(d['policy_b2'])
    return W1, b1, W2, b2

def logits(W1, b1, W2, b2, sf, afs):
    # sf (73,), afs (n,23)
    n = afs.shape[0]
    X = np.concatenate([np.repeat(sf[None, :], n, axis=0), afs], axis=1)  # (n,96)
    h = np.maximum(0.0, X @ W1 + b1)
    return h @ W2 + b2

def softmax(x):
    e = np.exp(x - x.max())
    return e / e.sum()

def mtype(af):
    t = int(np.argmax(af[:10]))
    if af[t] < 0.5:
        return 'NONE'
    name = TYPES[t]
    if name == 'BLITZ' and af[10] > 0.5:
        name = 'BLITZ&SCORE'
    return name

def decider_race(rec, dec):
    return rec['ra'] if dec['persp'] == 'home' else rec['rb']

def agg(decisions, W):
    """Vrati per-type prumernou visit vs policy prior masu + shodu top1."""
    W1, b1, W2, b2 = W
    vm = {}; pm = {}; cnt = 0; agree = 0
    chosen_by_type = {}; pol_top_by_type = {}
    disagreements = []
    for rec, d in decisions:
        afs = np.stack([a for a, _ in d['av']])
        vf = np.array([v for _, v in d['av']])
        lg = logits(W1, b1, W2, b2, d['sf'].astype(np.float64), afs)
        pp = softmax(lg)
        types = [mtype(a) for a, _ in d['av']]
        for t, v, p in zip(types, vf, pp):
            vm[t] = vm.get(t, 0.0) + v
            pm[t] = pm.get(t, 0.0) + p
        iv = int(np.argmax(vf)); ip = int(np.argmax(pp))
        chosen_by_type[types[iv]] = chosen_by_type.get(types[iv], 0) + 1
        pol_top_by_type[types[ip]] = pol_top_by_type.get(types[ip], 0) + 1
        cnt += 1
        if types[iv] == types[ip]:
            agree += 1
        else:
            disagreements.append((rec, d, types[iv], types[ip], float(pp[ip]),
                                  float(vf[iv]), float(pp[iv])))
    for k in vm: vm[k] /= cnt
    for k in pm: pm[k] /= cnt
    return dict(n=cnt, visit_mass=vm, prior_mass=pm, top1_agree=agree / max(cnt, 1),
                chosen=chosen_by_type, pol_top=pol_top_by_type,
                disagreements=disagreements)

def fmt_mass(a):
    keys = sorted(set(a['visit_mass']) | set(a['prior_mass']),
                  key=lambda k: -a['visit_mass'].get(k, 0))
    lines = [f"  {'typ':<12} {'search visit%':>13} {'policy prior%':>13} {'delta pp':>9}"]
    for k in keys:
        v = 100 * a['visit_mass'].get(k, 0); p = 100 * a['prior_mass'].get(k, 0)
        lines.append(f"  {k:<12} {v:13.1f} {p:13.1f} {p - v:+9.1f}")
    return '\n'.join(lines)

def board_ascii(d, home_char='D', away_char='s'):
    grid = [['.'] * 26 for _ in range(15)]
    for (pid, x, y, st, hb, name, ma, stt, ag, av) in d['home']:
        if 0 <= x < 26 and 0 <= y < 15:
            c = home_char if st == 0 else home_char.lower() if home_char.isupper() else '+'
            grid[y][x] = c if not hb else '@'
    for (pid, x, y, st, hb, name, ma, stt, ag, av) in d['away']:
        if 0 <= x < 26 and 0 <= y < 15:
            c = away_char.upper() if st == 0 else away_char.lower()
            grid[y][x] = c if not hb else '&'
    bx, by, bh, bc = d['ball']
    if not bh and 0 <= bx < 26 and 0 <= by < 15:
        grid[by][bx] = '*'
    return '\n'.join(''.join(r) for r in grid)

def main():
    data = pickle.load(open(os.path.join(SCR, 'decisions.pkl'), 'rb'))
    W = load_policy(os.path.join(BB, 'weights_best_policy.json'))

    # --- 1) shoda policy vs search podle rasy (bez policy v behu: A a C) ---
    groups = {}
    for rec in data:
        for d in rec['decs']:
            race = decider_race(rec, d)
            key = (rec['tag'], race)
            groups.setdefault(key, []).append((rec, d))

    print('=== PRIOR vs VISIT masa podle typu makra (policy OFF behy) ===')
    for key in [('A', 'dwarf'), ('A', 'skaven'), ('C', 'wood-elf'), ('C', 'skaven')]:
        if key not in groups: continue
        a = agg(groups[key], W)
        print(f"\n--- {key[1]} (run {key[0]}), n={a['n']} rozhodnuti, "
              f"top1 shoda policy vs search: {100*a['top1_agree']:.1f} % ---")
        print(fmt_mass(a))
        print(f"  search chosen types: { {k:v for k,v in sorted(a['chosen'].items(), key=lambda x:-x[1])} }")
        print(f"  policy top1  types: { {k:v for k,v in sorted(a['pol_top'].items(), key=lambda x:-x[1])} }")

    # --- 2) posun chovani: dwarf chosen-type distribuce A (blend0) vs B (blend0.2) ---
    print('\n=== POSUN CHOVANI: dwarf chosen macro typy, blend 0.0 (A) vs 0.2 (B) ===')
    for tag in ['A', 'B']:
        key = (tag, 'dwarf')
        ch = {}
        tot = 0
        for rec, d in groups.get(key, []):
            vf = np.array([v for _, v in d['av']])
            t = mtype(d['av'][int(np.argmax(vf))][0])
            ch[t] = ch.get(t, 0) + 1; tot += 1
        print(f"  run {tag} (n={tot}): " + ', '.join(
            f"{k}:{100*v/tot:.1f}%" for k, v in sorted(ch.items(), key=lambda x: -x[1])))
    # skaven kontrola
    for tag in ['A', 'B']:
        key = (tag, 'skaven')
        ch = {}; tot = 0
        for rec, d in groups.get(key, []):
            vf = np.array([v for _, v in d['av']])
            t = mtype(d['av'][int(np.argmax(vf))][0])
            ch[t] = ch.get(t, 0) + 1; tot += 1
        print(f"  [kontrola skaven] run {tag} (n={tot}): " + ', '.join(
            f"{k}:{100*v/tot:.1f}%" for k, v in sorted(ch.items(), key=lambda x: -x[1])))

    print('\n=== SKORE her (dwarf home vs skaven away, stejne seedy) ===')
    for tag in ['A', 'B']:
        sc = [(r['seed'], r['score']) for r in data if r['tag'] == tag]
        tdd = sum(s[1][0] for s in sc); tds = sum(s[1][1] for s in sc)
        print(f"  run {tag}: dwarf TD celkem {tdd}, skaven TD celkem {tds}; "
              f"vysledky: {[s[1] for s in sorted(sc)]}")

    # --- 3) kontrafakt: prepis capability featur dwarf->elf, zmeni se typova preference? ---
    print('\n=== KONTRAFAKT: dwarf stavy s elfimi capability featurami ===')
    W1, b1, W2, b2 = W
    key = ('A', 'dwarf')
    shift = []
    for rec, d in groups.get(key, [])[:400]:
        afs = np.stack([a for a, _ in d['av']])
        sf = d['sf'].astype(np.float64)
        pp0 = softmax(logits(W1, b1, W2, b2, sf, afs))
        sf2 = sf.copy()
        # elf-typicke hodnoty: avg ST 3.0/5, AG 4.0/5, AV 7.7/10, block 0.1, dodge 0.5
        sf2[19] = 3.0 / 5; sf2[38] = 4.0 / 5; sf2[36] = 7.7 / 10
        sf2[48] = 0.10; sf2[50] = 0.50
        pp1 = softmax(logits(W1, b1, W2, b2, sf2, afs))
        types = [mtype(a) for a, _ in d['av']]
        m0 = {}; m1 = {}
        for t, p0, p1 in zip(types, pp0, pp1):
            m0[t] = m0.get(t, 0) + p0; m1[t] = m1.get(t, 0) + p1
        l1 = 0.5 * sum(abs(m1.get(t, 0) - m0.get(t, 0)) for t in set(m0) | set(m1))
        shift.append(l1)
    print(f"  n={len(shift)}, prumerna TV-vzdalenost typove distribuce priors po "
          f"prepnuti capability featur dwarf->elf: {np.mean(shift):.4f} "
          f"(0=policy capability featury ignoruje, 1=uplne jine preference)")

    # --- 4) konkretni divergentni situace (dwarf, run A) ---
    print('\n=== KONKRETNI SITUACE: dwarf rozhodnuti, kde policy top1 != search top1 ===')
    a = agg(groups[('A', 'dwarf')], W)
    dis = sorted(a['disagreements'], key=lambda x: -x[4])  # nejsebejistejsi policy
    picked = []
    seen_pairs = set()
    for rec, d, t_search, t_pol, p_pol, v_search, p_on_search in dis:
        pair = (t_search, t_pol)
        if pair in seen_pairs: continue
        seen_pairs.add(pair)
        picked.append((rec, d, t_search, t_pol, p_pol, v_search, p_on_search))
        if len(picked) >= 8: break
    for i, (rec, d, ts, tp, ppol, vs, pos) in enumerate(picked):
        bx, by, bh, bc = d['ball']
        carrier = None
        for side in ('home', 'away'):
            for p in d[side]:
                if p[4]: carrier = (side, p)
        print(f"\n--- situace {i+1} (seed {rec['seed']}, persp {d['persp']}) ---")
        print(f"  search hraje: {ts} (visit {100*vs:.0f} %) | policy naseptava: {tp} "
              f"(prior {100*ppol:.0f} %, na search-tahu jen {100*pos:.0f} %)")
        print(f"  mic: held={bh} carrier={carrier[1][5] if carrier else None} "
              f"({carrier[0] if carrier else '-'}) pos=({bx},{by})")
        sf = d['sf']
        print(f"  stav: turn_prog={sf[3]:.2f} score_diff={sf[0]*6:+.0f} "
              f"iHaveBall={sf[12]:.0f} oppHasBall={sf[13]:.0f} loose={sf[14]:.0f} "
              f"carrier_distTD={sf[15]*26:.0f} myAG={sf[38]*5:.1f} oppAG={sf[39]*5:.1f}")
        print(board_ascii(d))
        # top-5 kandidatu podle policy a podle visitu
        afs = np.stack([x for x, _ in d['av']])
        vf = np.array([v for _, v in d['av']])
        pp = softmax(logits(W1, b1, W2, b2, d['sf'].astype(np.float64), afs))
        types = [mtype(x) for x, _ in d['av']]
        order_p = np.argsort(-pp)[:5]; order_v = np.argsort(-vf)[:5]
        print('  policy top5: ' + ', '.join(f"{types[j]}:{100*pp[j]:.0f}%" for j in order_p))
        print('  search top5: ' + ', '.join(f"{types[j]}:{100*vf[j]:.0f}%" for j in order_v))

if __name__ == '__main__':
    main()
