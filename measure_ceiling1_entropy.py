"""Strop 1: entropie MCTS visit-distribuce (cíle policy imitace).
Měří, jestli policy_loss ~2.24 je u nezredukovatelného dna H(target).
Bez tréninku — jen odehraje pár her a sebere get_policy_decisions()."""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
MCTS = 100
TV = 1200
WEIGHTS = 'weights_best.json'   # value weights pro search (blend=0 → priory heuristické)
N_GAMES = 6

def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)

all_H, all_Hnorm, all_n, all_top1, all_top3, all_top5, near_tie = [], [], [], [], [], [], 0
n_dec = 0

for g in range(N_GAMES):
    away = AWAY[g % len(AWAY)]
    hr = bb_engine.get_developed_roster('human', TV)
    ar = bb_engine.get_developed_roster(away, TV)
    logged = bb_engine.simulate_game_logged(
        hr, ar, 'macro_mcts', 'macro_mcts',
        seed=1000 + g, weights_path=WEIGHTS, epsilon=0.1,
        mcts_iterations=MCTS, policy_blend=0.0, vf_blend=0.0,
    )
    decs = logged.get_policy_decisions()
    for d in decs:
        vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
        s = sum(vf)
        if s <= 0 or len(vf) < 2:
            continue
        vf = [x / s for x in vf]
        n = len(vf)
        H = entropy_nats(vf)
        all_H.append(H)
        all_Hnorm.append(H / math.log(n) if n > 1 else 0.0)
        all_n.append(n)
        all_top1.append(vf[0])
        all_top3.append(sum(vf[:3]))
        all_top5.append(sum(vf[:5]))
        if vf[0] < 1.25 * vf[1]:
            near_tie += 1
        n_dec += 1
    print(f'  hra {g+1}/{N_GAMES} ({away}): {len(decs)} decisí, kumul {n_dec}', flush=True)

def pct(xs, p):
    xs = sorted(xs); k = int(p/100*(len(xs)-1)); return xs[k]

print('\n================ STROP 1: ENTROPIE CÍLE ================')
print(f'rozhodnutí celkem: {n_dec}')
print(f'n_actions:  mean={st.mean(all_n):.1f}  median={st.median(all_n):.0f}  p90={pct(all_n,90)}  max={max(all_n)}')
print(f'H(cíl) nats: mean={st.mean(all_H):.3f}  median={st.median(all_H):.3f}  p10={pct(all_H,10):.3f}  p90={pct(all_H,90):.3f}')
print(f'H normalizovaná (H/ln n): mean={st.mean(all_Hnorm):.3f}  (1.0 = uniformní, 0 = jeden tah)')
print(f'top-1 mass (max visit_fraction): mean={st.mean(all_top1):.3f}  median={st.median(all_top1):.3f}')
print(f'top-3 mass: mean={st.mean(all_top3):.3f}   top-5 mass: mean={st.mean(all_top5):.3f}')
print(f'near-tie (top1 < 1.25x top2): {near_tie}/{n_dec} = {100*near_tie/max(n_dec,1):.1f}%')
print(f'\nPOROVNÁNÍ: policy_loss z tréninku ~2.24  vs  mean H(cíl)={st.mean(all_H):.3f}')
gap = 2.24 - st.mean(all_H)
print(f'mezera loss - H = {gap:+.3f}')
print('  ~0 → policy je u entropického dna, top1 je artefakt → pivot blend>0')
print('  >>0 → reálná mezera → strop 2 (kapacita)')
