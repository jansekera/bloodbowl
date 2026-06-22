"""Strop 1c: je plochý cíl artefakt tréninkového šumu (Dirichlet 0.3 + C=2.0)?
Nový engine umožní dirichlet_alpha + exploration_c parametrizovat.
Všechny s policy nahranou (heuristické priory aktivní = jako trénink), blend=0, MCTS=100.
Dekompozice: train(D0.3,C2.0) vs bez-dirichlet vs nízké-C vs eval(D0.0,C1.0)."""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
MCTS, TV = 100, 1200
VALUE, POLICY = 'weights_best.json', 'weights_policy.json'
N_GAMES = 6
CONFIGS = [
    ('train  (D0.3,C2.0)', 0.3, 2.0),
    ('no-dir (D0.0,C2.0)', 0.0, 2.0),
    ('low-C  (D0.3,C1.0)', 0.3, 1.0),
    ('eval   (D0.0,C1.0)', 0.0, 1.0),
]

def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)

def run(label, dα, c):
    Hs, Hn, top1, top3, n_act, near = [], [], [], [], [], 0
    nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        logged = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts',
            seed=8000 + g, weights_path=VALUE, policy_weights_path=POLICY,
            epsilon=0.1, mcts_iterations=MCTS, policy_blend=0.0, vf_blend=0.0,
            dirichlet_alpha=dα, exploration_c=c,
        )
        for d in logged.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]
            nn = len(vf)
            Hs.append(entropy_nats(vf)); Hn.append(entropy_nats(vf)/math.log(nn))
            top1.append(vf[0]); top3.append(sum(vf[:3])); n_act.append(nn)
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [{label}] {nd} decisí', flush=True)
    return dict(label=label, H=st.mean(Hs), Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))

rows = []
for label, dα, c in CONFIGS:
    print(f'== {label} ==', flush=True)
    rows.append(run(label, dα, c))

print('\n===== STROP 1c: ZDROJ PLOCHOSTI CÍLE (MCTS=100, heuristické priory) =====')
print(f"{'config':>20} {'H(nats)':>9} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near%':>7}")
for r in rows:
    print(f"{r['label']:>20} {r['H']:>9.3f} {r['Hn']:>8.3f} {r['top1']:>7.3f} {r['top3']:>7.3f} {r['near']:>7.1f}")
print('\nINTERPRETACE:')
print('  eval výrazně ostřejší než train (H dolů, top1 nahoru) → plochost = explorační ŠUM')
print('     → FIX NÁŠ: generovat tréninková data s menším šumem. per-player NETŘEBA (zatím).')
print('  eval ~stejně plochý jako train → šum to není, value/search opravdu nerozlišuje → per-player/Tým 1.')
