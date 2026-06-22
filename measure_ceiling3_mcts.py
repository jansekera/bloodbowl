"""Strop 3: vyrobí víc MCTS simulací špičatější cíl?
Porovná entropii/koncentraci visit-distribuce při 100 vs 400 (vs 800) sims.
Pokud H klesá a top-1 mass roste s sims → plochý cíl je artefakt podsimulování (fixovatelné).
Pokud H zůstává ~uniformní i s víc sims → value nerozlišuje akce (hlubší, value-drift)."""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
TV = 1200
WEIGHTS = 'weights_best.json'
N_GAMES = 4
SIM_LEVELS = [100, 400, 800]

def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)

def measure(mcts):
    Hs, Hn, top1, top3, n_act, near = [], [], [], [], [], 0
    nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        logged = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts',
            seed=2000 + g, weights_path=WEIGHTS, epsilon=0.1,
            mcts_iterations=mcts, policy_blend=0.0, vf_blend=0.0,
        )
        for d in logged.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]
            n = len(vf)
            Hs.append(entropy_nats(vf)); Hn.append(entropy_nats(vf)/math.log(n))
            top1.append(vf[0]); top3.append(sum(vf[:3])); n_act.append(n)
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [MCTS={mcts}] {nd} decisí, n_act mean={st.mean(n_act):.1f}', flush=True)
    return dict(mcts=mcts, nd=nd, H=st.mean(Hs), Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))

rows = []
for m in SIM_LEVELS:
    print(f'== měřím MCTS={m} ({N_GAMES} her) ==', flush=True)
    rows.append(measure(m))

print('\n================ STROP 3: MCTS SIMS vs ŠPIČATOST CÍLE ================')
print(f"{'sims':>6} {'H(nats)':>9} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near-tie%':>10}")
for r in rows:
    print(f"{r['mcts']:>6} {r['H']:>9.3f} {r['Hn']:>8.3f} {r['top1']:>7.3f} {r['top3']:>7.3f} {r['near']:>10.1f}")
print('\nINTERPRETACE:')
print('  H klesá & top1 roste s sims → plochý cíl = podsimulování → FIX: víc sims v self-play')
print('  H ~stejná (uniformní) i s víc sims → value nerozlišuje akce → blocker je VALUE (drift/kvalita)')
