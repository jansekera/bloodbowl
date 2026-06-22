"""Strop 1b (OPRAVA): byla předchozí měření zkreslená UNIFORMNÍMI priory?
Nález: heuristické priory v macro_mcts se počítají JEN když je nahraná policy síť
(macro_mcts.cpp:214). Strop 1/3/4 běžely BEZ policy → uniformní priory → uměle plochý cíl.
A/B: stejné hry, jednou bez policy (uniform priory = jako předtím), jednou s policy
(heuristické priory aktivní = jako trénink). Dirichlet 0.3 + C=2.0 jsou v obou (hardcoded)."""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
MCTS, TV = 100, 1200
VALUE = 'weights_best.json'
POLICY = 'weights_policy.json'
N_GAMES = 6

def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)

def run(policy_path, label):
    Hs, Hn, top1, top3, n_act, near = [], [], [], [], [], 0
    nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        kw = dict(seed=7000 + g, weights_path=VALUE, epsilon=0.1,
                  mcts_iterations=MCTS, policy_blend=0.0, vf_blend=0.0)
        if policy_path:
            kw['policy_weights_path'] = policy_path
        logged = bb_engine.simulate_game_logged(hr, ar, 'macro_mcts', 'macro_mcts', **kw)
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
    print(f'  [{label}] {nd} decisí, n_act mean={st.mean(n_act):.1f}', flush=True)
    return dict(label=label, nd=nd, H=st.mean(Hs), Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))

rows = []
print('== bez policy (uniformní priory = jako strop 1/3/4) ==', flush=True)
rows.append(run('', 'BEZ policy (uniform)'))
print('== s policy (heuristické priory aktivní = jako trénink) ==', flush=True)
rows.append(run(POLICY, 'S policy (heuristic)'))

print('\n===== STROP 1b: VLIV PRIORŮ NA ŠPIČATOST CÍLE (MCTS=100) =====')
print(f"{'config':>22} {'H(nats)':>9} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near%':>7}")
for r in rows:
    print(f"{r['label']:>22} {r['H']:>9.3f} {r['Hn']:>8.3f} {r['top1']:>7.3f} {r['top3']:>7.3f} {r['near']:>7.1f}")
print('\nINTERPRETACE:')
print('  S policy výrazně NIŽŠÍ H / VYŠŠÍ top1 → moje strop měření byla zkreslená uniform priory.')
print('    → závěr "value neumí rozlišit → per-player" PADÁ, přeměřit strop 3/4 správně.')
print('  ~stejné → priory to nebyly, plochý cíl drží i s heuristikami → původní závěr platí.')
